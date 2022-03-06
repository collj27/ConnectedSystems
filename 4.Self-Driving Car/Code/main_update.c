/* Quest 4 - Group 11 */
#include <stdio.h>
#include <stdbool.h>
#include <ultrasonic.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "driver/pcnt.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/uart.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "soc/timer_group_struct.h"


/* DEFINITIONS */
#define TRIGGER_GPIO 17             // Front Ultrasonic: (TX)
#define ECHO_GPIO 16                // Front Ultrasonic: (RX)
#define TRIGGER_GPIO2 26            // Side Ultrasonic: (may need to change if required for motors) (A0)
#define ECHO_GPIO2 34               // Side Ultrasonic: (A2)
// Ultrasonic error ranges
#define FRONT_RANGE 20              // car must be > 20 cm from front wall
#define SIDE_RANGE 20               // 20 cm
#define SIDE_ERROR_RANGE 10         // car can be within 10 cm of the SIDE_RANGE
#define GPIO_R_PWM0A_OUT 33         // MOTORS: Set GPIO 33 as Right PWM0A
#define GPIO_R_PWM0B_OUT 15         // MOTORS: Set GPIO 15 as Right PWM0B
#define GPIO_L_PWM1A_OUT 25         // MOTORS: Set GPIO 25 as Left PWM1A
#define GPIO_L_PWM1B_OUT 4          // MOTORS: Set GPIO 4 as Left PWM1B
#define UART_NUM UART_NUM_1         // IR RX: Set UART number to use
#define IR_BUF_SIZE (1024)          // IR RX: Set IR UART buffer size
#define RX (14)                     // IR RX: Set IR RX GPIO pin
#define TX (17)                     // IR RX: Set IR TX GPIO pin !! UNUSED !!
#define TIMER_INTR_SEL TIMER_INTR_LEVEL // TIMER: level interrupt
#define TIMER_GROUP TIMER_GROUP_0   // TIMER: timer group 0
#define TIMER_DIVIDER 80            // TIMER: hardware timer clock divider, 80e6 to get 1MHz clock to timer
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)  // TIMER: used to calculate counter value
#define TIMER_INTERVAL0_SEC (0.1)   // TIMER: timer set to 1/10 second
#define timer_idx TIMER_0           // TIMER: id value
#define PCNT_UNIT_1ST PCNT_UNIT_0   // PULSE COUNTER: 1st unit number
#define PCNT_UNIT_2ND PCNT_UNIT_1   // PULSE COUNTER: 2nd unit number
#define PCNT_H_LIM_VAL 1000         // PULSE COUNTER: high limit value
#define PCNT_L_LIM_VAL -10          // PULSE COUNTER: low limit value
#define PCNT_INPUT_SIG_IOR 27       // PULSE COUNTER: input GPIO right sensor
#define PCNT_INPUT_SIG_IOL 32       // PULSE COUNTER: input GPIO left sensor
#define PCNT_INPUT_CTRL_IO 5        // PULSE COUNTER: Control GPIO HIGH=count up, LOW=count down
#define e 2.71828                   // Value of constant e
xQueueHandle pcnt_evt_queue;        // PULSE COUNTER: A queue to handle pulse counter events
pcnt_isr_handle_t user_isr_handle = NULL;  // PULSE COUNTER: user's ISR service handle
/* END DEFINITIONS */

/* GLOBAL VARIABLES */
//F = Forward
//T = Turn
//A = Adjust
//I = IDLE
//W = Waiting
bool power;  // Indicates and controls if car is running or "idle"; updated by GET request
char beacon[1] = '9';  // Indicates which beacon is seen (or '9' for no beacon seen)
char currState[1] = 'I';
char prevState[1] = 'I';
char nextState[1] = 'I';
char com[1] = ','
int sideError, frontError, distSide, distSidePrev, distFront;
float wheelSpeedLeft, wheelSpeedRight;  // !UPD Coming out as floats now (in m/s) - cm/s or mm/s?
int16_t pcnt_count_1 = 0;  // Pulse counter variable to retrieve count of pulses from buffer - 1st counter
int16_t pcnt_count_2 = 0;  // Pulse counter variable to retrieve count of pulses from buffer - 2nd counter
int16_t pcnt_curr_1 = 0;  // Pulse counter variable to track count of pulses this timer cycle - 1st counter
int16_t pcnt_curr_2 = 0;  // Pulse counter variable to track count of pulses this timer cycle - 2nd counter
int16_t pcnt_prev_1 = 0;  // Pulse counter variable to track count of pulses previous timer cycle - 1st counter
int16_t pcnt_prev_2 = 0;  // Pulse counter variable to track count of pulses previous timer cycle - 2nd counter
volatile int vtimer = 0;  // Volatile variable updated by Timer
int nvtimer = 0;  // Non-volatile variable that's compared to timer in main While loop
static const char *TAG="APP";
int cycleCount = 0;  // When waiting, used to track 5 seconds of waiting before initiating turn
/* END GLOBAL VARIABLES */



/* PULSE COUNTER - WHEELSPEED SENSOR */
static void pcnt_init(void)  // Init and config pulse counters
{
    /* Prepare configuration for the 1st PCNT unit */
    pcnt_config_t pcnt_config_1st = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = PCNT_INPUT_SIG_IOR,
        .ctrl_gpio_num = PCNT_INPUT_CTRL_IO,
        .channel = PCNT_CHANNEL_0,
        .unit = PCNT_UNIT_1ST,
        // What to do on the positive / negative edge of pulse input?
        .pos_mode = PCNT_COUNT_INC,   // Count up on the positive edge
        .neg_mode = PCNT_COUNT_DIS,   // Keep the counter value on the negative edge
        // What to do when control input is low or high?
        .lctrl_mode = PCNT_MODE_REVERSE, // Reverse counting direction if low
        .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
        // Set the maximum and minimum limit values to watch
        .counter_h_lim = PCNT_H_LIM_VAL,
        .counter_l_lim = PCNT_L_LIM_VAL,
    };
    /* Initialize 1st PCNT unit */
    pcnt_unit_config(&pcnt_config_1st);
    /* Prepare configuration for the 2nd PCNT unit */
    pcnt_config_t pcnt_config_2nd = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = PCNT_INPUT_SIG_IOL,
        .ctrl_gpio_num = PCNT_INPUT_CTRL_IO,
        .channel = PCNT_CHANNEL_0,
        .unit = PCNT_UNIT_2ND,
        // What to do on the positive / negative edge of pulse input?
        .pos_mode = PCNT_COUNT_INC,   // Count up on the positive edge
        .neg_mode = PCNT_COUNT_DIS,   // Keep the counter value on the negative edge
        // What to do when control input is low or high?
        .lctrl_mode = PCNT_MODE_REVERSE, // Reverse counting direction if low
        .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
        // Set the maximum and minimum limit values to watch
        .counter_h_lim = PCNT_H_LIM_VAL,
        .counter_l_lim = PCNT_L_LIM_VAL,
    };
    /* Initialize 2nd PCNT unit */
    pcnt_unit_config(&pcnt_config_2nd);
    /* Configure and enable the input filters */
    pcnt_set_filter_value(PCNT_UNIT_1ST, 100);
    pcnt_filter_enable(PCNT_UNIT_1ST);
    pcnt_set_filter_value(PCNT_UNIT_2ND, 100);
    pcnt_filter_enable(PCNT_UNIT_2ND);
    /* Initialize PCNTs's counters */
    pcnt_counter_pause(PCNT_UNIT_1ST);
    pcnt_counter_clear(PCNT_UNIT_1ST);
    pcnt_counter_pause(PCNT_UNIT_2ND);
    pcnt_counter_clear(PCNT_UNIT_2ND);
    /* Everything is set up, now go to counting */
    pcnt_counter_resume(PCNT_UNIT_1ST);
    pcnt_counter_resume(PCNT_UNIT_2ND);
}

static float calcWheelSpeed(int wheel)  // Calculate the actual wheel speed from the sensors
{
  float speed = 0;
  // M/s = Distance / Time
  // Time = 0.1 seconds
  // Distance = number of ticks * 2piR/10
  // -- R = radius of disk = 0.5 inch = 0.0127 m (1.27 cm); 2(3.14)(0.0127) ~ 0.07979645
  // -- 10 is the number of ticks over the entire circumference; 0.07979645/10 = 0.007979645
  if (wheel == 1) {  // Wheel 1 is the right wheel
    pcnt_get_counter_value(PCNT_UNIT_1ST, &pcnt_count_1);
    pcnt_curr_1 = pcnt_count_1;
    speed = (pcnt_curr_1 - pcnt_prev_1) * (0.007979645) / 0.1; //wheel speed
    pcnt_prev_1 = pcnt_curr_1;
  }
  else if (wheel == 2) {  // Wheel 2 is the left wheel
    pcnt_get_counter_value(PCNT_UNIT_2ND, &pcnt_count_2);
    pcnt_curr_2 = pcnt_count_2;
    speed = (pcnt_curr_2 - pcnt_prev_2) * (0.007979645) / 0.1; //wheel speed
    pcnt_prev_2 = pcnt_curr_2;
  }
 return speed;
}
/* END PULSE COUNTER - WHEELSPEED SENSOR */

/* SONIC SENSORS */

void ultrasonic_setup(void *pvParamters) {

    ultrasonic_sensor_t2 sensor2 = {
        .trigger_pin2 = TRIGGER_GPIO2,
        .echo_pin2 = ECHO_GPIO2
    };

    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO
    };

    ultrasonic_init(&sensor);
    ultrasonic_init2(&sensor2);

}

int ultrasonic_front(void *pvParamters) {
    uint32_t distance;
    esp_err_t res = ultrasonic_measure_cm(&sensor, MAX_DISTANCE_CM, &distance);
    // detects if car is too close to the wall in front
    if (distance < FRONT_RANGE) {
        return 0;
    }
    // car is within optimal range by not being too close to front wall
    else {
        return 1;
    }
    
}

int ultrasonic_side(void *pvParamters) {
    uint32_t distance2;
    esp_err_t res2 = ultrasonic_measure_cm2(&sensor2, MAX_DISTANCE_CM, &distance2);
    // detects if car is too close to the side wall
    if (distance2 < (SIDE_RANGE - SIDE_ERROR_RANGE)) {
        return 0;
    }
    // detects if car is too far from the side wall
    else if (distance2 > (SIDE_RANGE + SIDE_ERROR_RANGE)) {
        return 2;
    }
    // detects if car is within optimal range of the side wall
    else {
        return 1;
    }
}

/* END SONIC SENSORS */

/* MOTOR CONTROL */
static void mcpwm_motor_gpio_initialize()  // Init the PWM channels for the motors
{
    printf("initializing mcpwm gpio...\n");
    // Motor 0 - right wheel
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_R_PWM0A_OUT);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_R_PWM0B_OUT);
    // Motor 1 - left wheel
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, GPIO_L_PWM1A_OUT);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1B, GPIO_L_PWM1B_OUT);
}

static void mcpwm_motor_gpio_config()  // Config the PWM channels and Timers for the motors
{
  printf("Configuring Initial Parameters of mcpwm...\n");
  mcpwm_config_t pwm_config;
  pwm_config.frequency = 1000;    //frequency = 500Hz,
  pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
  pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A & PWM0B with above settings
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);    //Configure PWM0A & PWM0B with above settings
}

static float calc_wheel_speed(float distFromError) 
{
    return 100.0 * (0.0135783 - (-0.4030417/1.305376)*(1-pow(e,-1.305376*distFromError)));
}

/** @brief motor moves in forward direction, with duty cycle = duty % */
static void brushed_motor_forward(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num , float duty_cycle)
{
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_B);
    mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_A, duty_cycle);
    mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); //call this each time, if operator was previously in low/high state
}

/** @brief motor moves in backward direction, with duty cycle = duty % */
static void brushed_motor_backward(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num , float duty_cycle)
{
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_A);
    mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_B, duty_cycle);
    mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);  //call this each time, if operator was previously in low/high state
}

/** @brief motor stop */
static void brushed_motor_stop(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num)
{
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_A);
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_B);
}

static void forward(float left = 0.0, float right = 0.0)  // Use "left" and "right" to vary duty cycle on either side
{
  // 50% duty cycle is norm; if left or right specified in function call, alter that side's duty cycle accordingly
  float leftDuty = 50.0 + left;
  float rightDuty = 50.0 + right;
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, rightDuty);
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_1, leftDuty);
}

static void backward()
{
  brushed_motor_backward(MCPWM_UNIT_0, MCPWM_TIMER_0, 50.0);
  brushed_motor_backward(MCPWM_UNIT_0, MCPWM_TIMER_1, 50.0);
}

static void stop()
{
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_1);
}

static void turn_left()
{
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_1);
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, 30.0);
}

static void turn_right()
{
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_1, 30.0);
}
/* END MOTOR CONTROL */

/* IR RECEIVER */
void ir_uart_config() {  // Configure parameters of UART driver/ communication pins
    uart_config_t uart_config = {
        .baud_rate = 1200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TX, RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_line_inverse(UART_NUM, UART_INVERSE_RXD);
    uart_driver_install(UART_NUM, IR_BUF_SIZE * 2, 0, 0, NULL, 0);
}

static char updateBeacon() {  // Check the IR RX to see if there is data and update the beacon
  char beaconValue = '9';
  uint8_t* ir_data = (uint8_t*) malloc(IR_BUF_SIZE);  // IR RX: data buffer init
  // IR RX: check UART buffer for data
  const int rxBytes = uart_read_bytes(UART_NUM, ir_data, 9, 10 / portTICK_RATE_MS);
  // IR RX: if there is data, update the variable 'beacon'
  if (rxBytes > 0) {
    // 2 bytes received, 2nd is the ID
    // format: START (0x0A) and ID (0-3)
    for(int i = 0; i < 16; i++) {
        printf("data: %c\n", ir_data[i]);
        // will probably need to search for correct data byte and update beaconValue !UPD
    }
    uart_flush(UART_NUM);  // IR RX: flush buffer
  }
  beaconValue = ir_data[15];
  return beaconValue;
}
/* END IR RECEIVER */


/* HTTPD SERVER */

//calls power function and returns ON or OFF
esp_err_t power_get_handler(httpd_req_t *req)
{
    if (power == false) {
        power = true;
        const char* resp_str = "ON";
        httpd_resp_send(req, resp_str, strlen(resp_str));
    } else {
        power = false;
        const char* resp_str = "OFF";
        httpd_resp_send(req, resp_str, strlen(resp_str));
    }
    return ESP_OK;
}

httpd_uri_t power = {
    .uri       = "/power",
    .method    = HTTP_GET,
    .handler   = power_get_handler,
    .user_ctx  = NULL
};


void double_to_string(char *buf, int s)
{
        sprintf(buf, "%d", s);

}

esp_err_t sss_get_handler(httpd_req_t *req)
{

    //create buffers for sensor information
    char vr_buf[5] = {0};
    char vl_buf[5] = {0};
    char fd_buf[5] = {0};
    char sd_buf[5] = {0};

    //convert sensor values to character arrays
    double_to_string(vr_buf,wheelSpeedRight);
    double_to_string(vl_buf,wheelSpeedLeft);
    double_to_string(fd_buf,distFront);
    double_to_string(sd_buf,distSide);

    //combine segment, state, and sensor data in one array, comma seperated
    char str[30] = {0};
    strcat(str,beacon);  //segment
    strcat(str,com);
    strcat(str,currState); //state
    strcat(str,com);
    strcat(str, vr_buf);
    strcat(str,com);
    strcat(str, vl_buf);
    strcat(str,com);
    strcat(str, fd_buf);
    strcat(str,com);
    strcat(str, sd_buf);

    //return combined charracter array
    const char* resp_str = str;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    return ESP_OK;
}

httpd_uri_t sss = {
    .uri       = "/sss", //sensors, segment, state
    .method    = HTTP_GET,
    .handler   = sss_get_handler,
    .user_ctx  = NULL
};


//starts webserver
httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        //httpd_register_uri_handler(server, &segstate);
        httpd_register_uri_handler(server, &power);
        //httpd_register_uri_handler(server, &beacon);
        httpd_register_uri_handler(server, &sss);
        return server;
    }
    
    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

//stops webserver
void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

//event handler starts/stops webserver and checks wifi connection
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    httpd_handle_t *server = (httpd_handle_t *) ctx;
    
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            ESP_LOGI(TAG, "Got IP: '%s'",
                     ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            // Start the web server
            if (*server == NULL) {
                *server = start_webserver();
            }
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            ESP_ERROR_CHECK(esp_wifi_connect());
            //Stop the web server
            if (*server) {
                stop_webserver(*server);
                *server = NULL;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}



/* END HTTPD SERVER */

/* WIFI */
static void initialise_wifi(void *arg)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, arg));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
/* END WIFI */

/* TIMER */
void IRAM_ATTR vtimer_isr(void *para){  // Config timer ISR group 0
    int timer_idx = (int) para;
    uint32_t intr_status = TIMERG0.int_st_timers.val;  // Access struct for interrupt status
    if((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        TIMERG0.hw_timer[timer_idx].update = 1;
        TIMERG0.int_clr_timers.t0 = 1;
        TIMERG0.hw_timer[timer_idx].config.alarm_en = 1;
        vtimer++;  // When timer triggers, update volatile variable
    }
}

static void vtimer_interrupt_init()  // Init and config timer
{
    int timer_group = TIMER_GROUP_0;
    int timer_idx = TIMER_0;
    timer_config_t config;
    config.alarm_en = 1;
    config.auto_reload = 1;
    config.counter_dir = TIMER_COUNT_UP;
    config.divider = TIMER_DIVIDER;
    config.intr_type = TIMER_INTR_SEL;
    config.counter_en = TIMER_PAUSE;
    /*Configure timer*/
    timer_init(timer_group, timer_idx, &config);
    /*Stop timer counter*/
    timer_pause(timer_group, timer_idx);
    /*Load counter value */
    timer_set_counter_value(timer_group, timer_idx, 0x00000000ULL);
    /*Set alarm value*/
    timer_set_alarm_value(timer_group, timer_idx, (TIMER_INTERVAL0_SEC * TIMER_SCALE));
    /*Enable timer interrupt*/
    timer_enable_intr(timer_group, timer_idx);
    /*Set ISR handler*/
    timer_isr_register(timer_group, timer_idx, vtimer_isr, (void*) timer_idx, ESP_INTR_FLAG_IRAM, NULL);
    /*Start timer counter*/
    timer_start(timer_group, timer_idx);
    //printf("TIMER INITIALIZED \n");
}
/* END TIMER */

void app_main()
{
  /* INITIALIZE COMPONENTS */
  //mcpwm_motor_gpio_initialize();  // !UPD MOTOR: mcpwm gpio init - disabled for testing other components
  //mcpwm_motor_gpio_config();  // !UPD MOTOR: mcpwm configuration - disabled for testing other components
  ir_uart_config();  // IR RX: uart init
  pcnt_init();  // PULSE COUNTER: init and config
  vtimer_interrupt_init();  // TIMER: init and config
  static httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(nvs_flash_init());
  initialise_wifi(&server);
  /* DONE INITIALIZING COMPONENTS */

  /* MAIN CODE */
  while(true) {
    // Executed every 100 ms
    if (nvtimer != vtimer) {
      prevState = currState;  // GLOBAL: update states
      currState = nextState;  // GLOBAL: update states
      wheelSpeedRight = calcWheelSpeed(1);  // WHEEL SPEED: update right wheel speed
      wheelSpeedLeft = calcWheelSpeed(2);  // WHEEL SPEED: update left wheel speed
      distSidePrev = distSide;  // SONIC: hold previous value for comparison during Turning only
      distSide = 0;  // SONIC: update side distance !UPD
      distFront = 0;  // SONIC: update front distance !UPD
      sideError = 0;  // PID: update side error value !UPD
      frontError = 0;  // PID: update front error value !UPD
      beacon = updateBeacon();  // IR RX: check for value and update beacon

      switch (currState) {
        case 'I':
          if (prevState != 'I') {  // First time in Idle state
            stop();  // Turn off the motors and stop the car
            nextState = 'I';
          } else {  // Already in Idle state for at least 1 cycle
            nextState = 'I';
          }
          break;
        case 'D':
          if (prevState != 'D') {  // First time in Driving state
            forward();  // Turn on the motors and move forward
            nextState = 'D';
          } else {  // Already in Driving state for at least 1 cycle
            if (sideError != 0 && frontError > 0) nextState = 'A';
            else if (frontError <= 0 && beacon != '9') nextState = 'T';
            else if (frontError <= 0 && beacon == '9') nextState = 'W';
          }
          break;
        case 'W':
          if (prevState != 'W') {  // First time in Waiting state
            stop();  // Turn off the motors and stop the car
            nextState = 'W';
            cycleCount++;
          } else {  // Already in Waiting state for at least 1 cycle
            if (frontError > 0) {
              nextState = 'D';
              cycleCount = 0;
            } else if (frontError <= 0 && cycleCount < 50) {
              nextState = 'W';
              cycleCount++;
            } else if (cycleCount >= 50) {
              nextState = 'T';
              cycleCount = 0;
            }
          }
          break;
        case 'A':
          if (sideError == 0) nextState = 'D';
          else {
            float left = 0;  // !UPD Will need to call function to set adjustment speed
            float right = 0;  // !UPD Will need to call function to set adjustment speed
            forward(left, right);  // Update speed of motors in forward direction to correct course
            nextState = 'A';
          }
          break;
        case 'T':
          if (prevState != 'T') {  // First time in Turning state
            turn_left();  // Switch the motors to turn left
            nextState = 'T';
          } else {  // Already in Turning state for at least 1 cycle
            if (distSide > distSidePrev && frontError > 0) nextState = 'D';
            else nextState = 'T';
          }
          break;
      }

      /* UPD! for testing wheel speed - remove below! */
      if (wheelSpeedRight > 0 || wheelSpeedLeft > 0) {
        printf("wheel 1: %f, wheel 2: %f\n", wheelSpeedRight, wheelSpeedLeft);
      }
      /* UPD! for testing wheel speed - remove above! */

      nvtimer = vtimer;  // !UPD ?? Need some way to set this back to 0 when it nears the limit of the data type (or regularly) ??
    }  // END processes executed every 100 ms

    // Asynchronous state update on Power command being sent
    if (currState == 'I' && power) {
      nextState = 'D';
    }
    if (currState != 'I' && !power) {
      nextState = 'I';
    }
  }
  /* END MAIN CODE */

}
