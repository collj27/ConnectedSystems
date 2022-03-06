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
#include "soc/uart_struct.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "soc/timer_group_struct.h"
#include <string.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <http_server.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"


/* DEFINITIONS */
#define TRIGGER_GPIO 17             // Front Ultrasonic: (TX)
#define ECHO_GPIO 16                // Front Ultrasonic: (RX)
// Ultrasonic error ranges
#define MAX_DISTANCE_CM 500         // SONIC: Max usable distance of the sensors
#define GPIO_R_PWM0A_OUT 33         // MOTORS: Set GPIO 33 as Right PWM0A
#define GPIO_R_PWM0B_OUT 15         // MOTORS: Set GPIO 15 as Right PWM0B
#define GPIO_L_PWM1A_OUT 25         // MOTORS: Set GPIO 25 as Left PWM1A
#define GPIO_L_PWM1B_OUT 4          // MOTORS: Set GPIO 4 as Left PWM1B
#define UART_NUM UART_NUM_2         // IR RX: Set UART number to use
#define IR_BUF_SIZE (1024)          // IR RX: Set IR UART buffer size
#define RX (14)                     // IR RX: Set IR RX GPIO pin
#define TX (21)                     // IR RX: Set IR TX GPIO pin !! UNUSED !! Was 17
#define TIMER_INTR_SEL TIMER_INTR_LEVEL // TIMER: level interrupt
#define TIMER_GROUP TIMER_GROUP_0   // TIMER: timer group 0
#define TIMER_DIVIDER 80            // TIMER: hardware timer clock divider, 80e6 to get 1MHz clock to timer
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)  // TIMER: used to calculate counter value
#define TIMER_INTERVAL0_SEC (0.05)   // TIMER: timer set to 1/20 second
#define timer_idx TIMER_0           // TIMER: id value
#define PCNT_UNIT_1ST PCNT_UNIT_0   // PULSE COUNTER: 1st unit number
#define PCNT_UNIT_2ND PCNT_UNIT_1   // PULSE COUNTER: 2nd unit number
#define PCNT_H_LIM_VAL 1000000      // PULSE COUNTER: high limit value
#define PCNT_L_LIM_VAL -10          // PULSE COUNTER: low limit value
#define PCNT_INPUT_SIG_IOR 27       // PULSE COUNTER: input GPIO right sensor
#define PCNT_INPUT_SIG_IOL 32       // PULSE COUNTER: input GPIO left sensor
#define PCNT_INPUT_CTRL_IO 5        // PULSE COUNTER: Control GPIO HIGH=count up, LOW=count down
#define e 2.71828                   // Value of constant
#define EXAMPLE_WIFI_SSID  "Group_11" //CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS  "smart-systems" //CONFIG_WIFI_PASSWORD
#define IRR_DEFAULT_VREF    1100        // IRR

xQueueHandle pcnt_evt_queue;        // PULSE COUNTER: A queue to handle pulse counter events
pcnt_isr_handle_t user_isr_handle = NULL;  // PULSE COUNTER: user's ISR service handle

static esp_adc_cal_characteristics_t *adc_chars;  // IRR
static const adc_channel_t irr_channel = ADC_CHANNEL_6;     // IRR GPIO34
static const adc_atten_t irr_atten = ADC_ATTEN_DB_11;  // IRR
static const adc_unit_t irr_unit = ADC_UNIT_1;  // IRR
/* END DEFINITIONS */

/* GLOBAL VARIABLES */
//D = Driving
//T = Turning
//A = Adjusting
//I = IDLE
//W = Waiting
volatile bool power = false;  // Indicates and controls if car is running or "idle"; updated by GET request
volatile bool bcon = false; //virtual beacon
char beacon[1] = "9";  // Indicates which beacon is seen (or '9' for no beacon seen)
char currState[1] = "I";  // Defaults state to IDLE
char prevState[1] = "I";  // Defaults state to IDLE
char nextState[1] = "I";  // Defaults state to IDLE
char com[1] = ",";
int sideError, frontError, distSide, distSidePrev, distFront, distBack;
float wheelSpeedLeft, wheelSpeedRight;
int16_t pcnt_count_1 = 0;  // Pulse counter variable to retrieve count of pulses from buffer - 1st counter
int16_t pcnt_count_2 = 0;  // Pulse counter variable to retrieve count of pulses from buffer - 2nd counter
int16_t pcnt_curr_1 = 0;  // Pulse counter variable to track count of pulses this timer cycle - 1st counter
int16_t pcnt_curr_2 = 0;  // Pulse counter variable to track count of pulses this timer cycle - 2nd counter
int16_t pcnt_prev_1 = 0;  // Pulse counter variable to track count of pulses previous timer cycle - 1st counter
int16_t pcnt_prev_2 = 0;  // Pulse counter variable to track count of pulses previous timer cycle - 2nd counter
int16_t pcnt_left_turn_end = 0;  // Pulse counter variable to track count of pulses during left turn
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
  // CM/sec = Distance / Time
  // Time = 1 seconds
  // Distance = number of ticks * piD/10
  // -- D = diameter of wheel = 6 cm; 3.14(6) ~ 18.85 cm circumference of wheel
  // -- 10 is the number of ticks over the entire circumference; 18.85/10 = 1.885 cm per tick
  if (wheel == 1) {  // Wheel 1 is the right wheel
    pcnt_get_counter_value(PCNT_UNIT_1ST, &pcnt_count_1);
    pcnt_curr_1 = pcnt_count_1;
    speed = (pcnt_curr_1 - pcnt_prev_1) * (1.885); //wheel speed in cm/sec
    pcnt_prev_1 = pcnt_curr_1;
  }
  else if (wheel == 2) {  // Wheel 2 is the left wheel
    pcnt_get_counter_value(PCNT_UNIT_2ND, &pcnt_count_2);
    pcnt_curr_2 = pcnt_count_2;
    speed = (pcnt_curr_2 - pcnt_prev_2) * (1.885); //wheel speed in cm/sec
    pcnt_prev_2 = pcnt_curr_2;
  }
 return speed;
}
/* END PULSE COUNTER - WHEELSPEED SENSOR */

/* SONIC SENSORS */
int ultrasonic_front() {
    uint32_t distance;
    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO
    };
    esp_err_t res = ultrasonic_measure_cm(&sensor, MAX_DISTANCE_CM, &distance);
    return distance;
}
void front_error()
{
  int turnpoint = 35;  // Car should turn when it is within 35 cm of a front object
  if (distFront > 36) {
    frontError = distFront - turnpoint;  // Set "frontError" to actual distance to front object
  } else if (distFront < 34) {
    frontError = 1;  // Too close to a front object
  } else {
    frontError = 0;  // Ideal range to front object (only here temporarily)
  }
}

void side_error()
{
  // Car should maintain a distance between 22 and 31 cm from objects on its left side
  if (distSide > 31) {
    sideError = 2;  // Too far from a side object
  } else if (distSide < 22) {
    sideError = 1;  // Too close to a side object
  } else {
    sideError = 0;  // Ideal range
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

static void forward(float left, float right)  // Use "left" and "right" to vary duty cycle on either side
{
  // If left or right specified in function call, alter that side's duty cycle accordingly
  float leftDuty = 75.0 + left;
  float rightDuty = 70.0 + right;
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, leftDuty);
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_1, rightDuty);
}

static void stop()  // STop both motors
{
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_1);
}

static void turn_left()  // Turn left by driving only the right motor
{
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_1, 60.0);
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
/* END IR RECEIVER */


/* HTTPD SERVER */
//calls power function and returns ON or OFF
esp_err_t power_get_handler(httpd_req_t *req)
{
    if (power == false) {
        power = true;
        const char* resp_str = "ON";
        httpd_resp_set_hdr(req, "access-control-allow-origin", "*");
        httpd_resp_send(req, resp_str, strlen(resp_str));
    } else {
        power = false;
        const char* resp_str = "OFF";
        httpd_resp_set_hdr(req, "access-control-allow-origin", "*");
        httpd_resp_send(req, resp_str, strlen(resp_str));
    }
    return ESP_OK;
}

httpd_uri_t pwr = {
    .uri       = "/power",
    .method    = HTTP_GET,
    .handler   = power_get_handler,
    .user_ctx  = NULL
};


void double_to_string(char *buf, int s)
{
  if (s > 999) s = 999;  // If a very large value is input, use 999 to prevent buffer issues
  if (s < 0) s = 0;  // If a negative number is input, use 0 to prevent buffer issues
  sprintf(buf, "%d", s);  // Convert the input number to a char array value
}

esp_err_t sss_get_handler(httpd_req_t *req)
{
    // Create buffers for sensor information
    char vr_buf[5] = {0};
    char vl_buf[5] = {0};
    char fd_buf[5] = {0};
    char sd_buf[5] = {0};

    // Convert sensor values to character arrays
    double_to_string(vr_buf,wheelSpeedRight);
    double_to_string(vl_buf,wheelSpeedLeft);
    double_to_string(fd_buf,distFront);
    double_to_string(sd_buf,distSide);

    // Combine segment, state, and sensor data in one array, comma seperated
    char str[100] = {0};
    strcat(str,beacon);  // Segment
    strcat(str,com);
    strcat(str,currState);  // State
    strcat(str,com);
    strcat(str, vr_buf);  // Right Wheel Speed
    strcat(str,com);
    strcat(str, vl_buf);  // Left Wheel Speed
    strcat(str,com);
    strcat(str, fd_buf);  // Front Distance
    strcat(str,com);
    strcat(str, sd_buf);  // Side Distance

    // Return combined charracter array
    const char* resp_str = str;
    httpd_resp_set_hdr(req, "access-control-allow-origin", "*");
    httpd_resp_send(req, resp_str, strlen(resp_str));

    return ESP_OK;
}

httpd_uri_t sss = {
    .uri       = "/sss",  // Sensors, segment, state
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
        httpd_register_uri_handler(server, &pwr);
        httpd_register_uri_handler(server, &sss);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

// Stops webserver
void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

// Event handler starts/stops webserver and checks wifi connection
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
            // Stop the web server
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
  mcpwm_motor_gpio_initialize();  // MOTOR: mcpwm gpio init
  mcpwm_motor_gpio_config();  // MOTOR: mcpwm configuration
  ir_uart_config();  // IR RX: uart init
  pcnt_init();  // PULSE COUNTER: init and config
  vtimer_interrupt_init();  // TIMER: init and config
  static httpd_handle_t server = NULL;  // HTTP: init and config
  ESP_ERROR_CHECK(nvs_flash_init());
  initialise_wifi(&server);  // WIFI: init and config
  // IRR Configure ADC
  adc1_config_width(ADC_WIDTH_BIT_10);
  adc1_config_channel_atten(irr_channel, irr_atten);
  // IRR Characterize ADC
  adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(irr_unit, irr_atten, ADC_WIDTH_BIT_12, IRR_DEFAULT_VREF, adc_chars);
  // ULTRASONIC: config
  ultrasonic_sensor_t sensor = {
      .trigger_pin = TRIGGER_GPIO,
      .echo_pin = ECHO_GPIO
  };
  ultrasonic_init(&sensor);  // ULTRASONIC: init
  /* DONE INITIALIZING COMPONENTS */

  /* MAIN CODE */
  // Initialize variables
  char beaconValue = '9';  // Beacon "9" means no beacon seen
  uint8_t* ir_data = (uint8_t*) malloc(IR_BUF_SIZE);  // IR RX: data buffer init
  uint32_t adc_reading = 0;  // IRR: tracking variable init
  while(true) {
    // Executed every 50 ms
    if (nvtimer != vtimer) {
      nvtimer = vtimer;  // Update the non-volatile timer variable with teh volatile timer value
      adc_reading += adc1_get_raw((adc1_channel_t)irr_channel);  // Update the IRR tracking variable with an IRR reading
      if (nvtimer % 5 == 0) { // Update the side distance & error only once per 0.25 second
        adc_reading /= 5;  // Takes average of 5 samples
        double v = (adc_reading / 1023.0) * 3.225;  // Converts reading to voltage value
        double d = 306.439 + v*(-512.611 + v*(382.268 + v*(-129.893 + v*16.2537)));  // Converts voltage value to distance in cm
        distSide = d;  // Sets side variable to calculated distance
        side_error();  // PID: update side error value
        adc_reading = 0;  // Reset the IRR tracking variable
      }
      if (nvtimer % 20 == 0) { // Update the wheel speed only once per second
        wheelSpeedRight = calcWheelSpeed(1);  // WHEEL SPEED: update right wheel speed
        wheelSpeedLeft = calcWheelSpeed(2);  // WHEEL SPEED: update left wheel speed
      }
      if (nvtimer % 2 == 0) {  // Processes executed every 100 ms
        prevState[0] = currState[0];  // GLOBAL: update states
        currState[0] = nextState[0];  // GLOBAL: update states
        distFront = ultrasonic_front();  // GLOBAL: update the front distance value
        front_error();  // PID: update front error value

        // IR RX: Read IR RX
        const int rxBytes = uart_read_bytes(UART_NUM, ir_data, 9, 10 / portTICK_RATE_MS);
        if (rxBytes > 0) {  // IR RX: if there is data, update the variable 'beacon'
          if (ir_data[0] == 0x00) {  // No beacon seen
            beacon[0] = '9';
          } else {  // Beacon seen; update beacon variable
            for(int i = 0; i < 8; i++) {
                printf("data: %X\n", ir_data[i]);
                if (ir_data[i] == 0x00) beacon[0] = '0';
                if (ir_data[i] == 0x01) beacon[0] = '1';
                if (ir_data[i] == 0x02) beacon[0] = '2';
                if (ir_data[i] == 0x03) beacon[0] = '3';
             }
          }
          uart_flush(UART_NUM);  // IR RX: flush buffer
        }
        char state = currState[0];
        switch (state) {
          case 'I':  // IDLE
            if (prevState[0] != 'I') {  // First time in Idle state
              stop();  // Turn off the motors and stop the car
              nextState[0] = 'I';  // Stay in IDLE state (unless changed by power interrupt)
            } else {  // Already in Idle state for at least 1 cycle
              nextState[0] = 'I';  // Stay in IDLE state (unless changed by power interrupt)
            }
            break;
          case 'D':  // DRIVING
            if (prevState[0] != 'D' && prevState[0] != 'A') {  // First time in Driving state; not coming from Adjusting state
              stop();  // Stop the car
              forward(0.0,0.0);  // Turn on the motors and move forward at normal speed
              nextState[0] = 'D';  // Stay in DRIVING state
            } else if (prevState[0] == 'A') {  // If previously in Adjusting state
              stop();  // Stop the car
              forward(0.0,0.0);  // Turn on the motors and move forward at normal speed
              nextState[0] = 'D';  // Stay in DRIVING state
            } else {  // Already in Driving state for at least 1 cycle, so maintain forward motion as-is
              if (sideError != 0 && frontError > 1) nextState[0] = 'A';  // Unless too close or too fsr from left wall
              else if (frontError == 1 && beacon[0] != '9') nextState[0] = 'T';  // Unless there is a wall and a beacon (meaning "Turn")
              else if (frontError == 1 && beacon[0] == '9') nextState[0] = 'W';  // Unless there is a wall, but no beacon (meaning "Wait")
            }
            break;
          case 'W':  // WAITING
            if (prevState[0] != 'W') {  // First time in Waiting state
              stop();  // Turn off the motors and stop the car
              nextState[0] = 'W';  // Stay in WAITING state
              cycleCount++;  // Only want to stay in Waiting for 2 seconds - this variable tracks the waiting period
            } else {  // Already in Waiting state for at least 1 cycle
              if (frontError > 1) {  // If no front obstacle, start driving again
                nextState[0] = 'D';
                cycleCount = 0;
              } else if (frontError == 1 && cycleCount < 20) {  // If still a front obstacle, but 2 seconds haven't elapsed, keep Waiting
                nextState[0] = 'W';
                cycleCount++;
              } else if (cycleCount >= 20) {  // If still a front obstacle, and 2 seconds have elapsed, start Turning
                nextState[0] = 'T';
                cycleCount = 0;
              }
            }
            break;
          case 'A':  // ADJUSTING
            if (prevState[0] != 'A') {  // First time in Adjusting state
              stop();  // Turn off the motors and stop the car
            }
            if (sideError == 0 && frontError > 1) {  // If at ideal distance from left wall and no front obstacle, start Driving again
              nextState[0] = 'D';
            } else if (sideError == 1  && frontError > 1) {
              // If too close to Left wall, set motors to matched duty cycle - car will natuarlly veer left
              brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, 60);
              brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_1, 60);
              nextState[0] = 'A';
            } else if (sideError == 2  && frontError > 1) {
              // If too far from Left wall, decrease right motor duty cycle - car will veer right
              brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, 70);
              brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_1, 50);
              nextState[0] = 'A';
            } else if (frontError == 1 && beacon[0] != '9') nextState[0] = 'T';  // If there is a wall and a beacon (meaning "Turn")
            else if (frontError == 1 && beacon[0] == '9') nextState[0] = 'W';  // If there is a wall, but no beacon (meaning "Wait")
            break;
          case 'T':  // TURNING
            if (prevState[0] != 'T') {  // First time in Turning state
              stop();  // Turn off the motors and stop the car
              turn_left();  // Switch the motors to turn left
              pcnt_left_turn_end = 0;  // Tracking variable used to time the left hand turn to happen for 1.6 seconds (shoudl be approx. 90 degrees)
              nextState[0] = 'T';
            } else {  // Already in Turning state for at least 1 cycle
              pcnt_left_turn_end++;
              if (pcnt_left_turn_end > 16) {  // After 1.6 seconds of turning, clear the beacon variable and go back to Waiting state (to check for front obstacles)
                beaconValue = '9';
                stop();
                nextState[0] = 'W';
              }
            }
            break;
          default:  // Default state - nothing to do here...
              printf("default \n");
        }
      }  // END processes executed every 100 ms

    }  // END processes executed every 50 ms

   // Asynchronous state update on Power command being sent
    if (currState[0] == 'I' && power) {
      nextState[0] = 'D';
    }
    if (currState[0] != 'I' && !power) {
      nextState[0] = 'I';
    }
  }
  /* END MAIN CODE */

}
