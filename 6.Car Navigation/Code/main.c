
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
#include "freertos/event_groups.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "driver/adc.h"
#include "esp_http_client.h"
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
#define EXAMPLE_ESP_WIFI_SSID  "Group_11" //CONFIG_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS  "smart-systems" //CONFIG_WIFI_PASSWORD
#define IRR_DEFAULT_VREF    1100        // IRR

/* END DEFINITIONS */

/* GLOBAL VARIABLES */

xQueueHandle pcnt_evt_queue;        // PULSE COUNTER: A queue to handle pulse counter events
pcnt_isr_handle_t user_isr_handle = NULL;  // PULSE COUNTER: user's ISR service handle
static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
char currState[1] = "I";  // Defaults state to IDLE
char prevState[1] = "I";  // Defaults state to IDLE
char nextState[1] = "I";  // Defaults state to IDLE
char com[2] = "00";       // Commands 
int frontError, distFront; //Front Error and Front Distance
int16_t pcnt_left_turn_end = 0;  // Pulse counter variable to track count of pulses during turn
int16_t max_pulse = 0;           // max pulse number of pulse per turn; determined by commands
volatile int vtimer = 0;  // Volatile variable updated by Timer
int nvtimer = 0;  // Non-volatile variable that's compared to timer in main While loop
static const char *TAG="APP";

// rpi URLs
char sensor_url[50] = "http://192.168.1.108:3000/sensor/";          // sensor values appended at end of url
char beacon_url[50] = "http://192.168.1.108:3000/beacon/?id_frag="; // id and fragment appended at end of url
char stop_url[50] = "http://192.168.1.108:3000/stop";               // initiates connection in "IDLE" to indicate car is stopped
    
//FOB ID
char fob[1] = "0";

//Fragment Bits
char frag1[1] = "0";
char frag2[1] = "0";
char frag3[1] = "0";
char frag4[1] = "0";
char frag5[1] = "0";
char frag6[1] = "0";
char frag7[1] = "0";
char frag8[1] = "0";
char frag9[1] = "0";
char frag10[1] = "0";

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

static void forward(float left, float right)  // Moves forward; Use "left" and "right" to vary duty cycle on either side
{
  // If left or right specified in function call, alter that side's duty cycle accordingly
  float leftDuty = 75.0 + left;
  float rightDuty = 70.0 + right;
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, leftDuty);
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_1, rightDuty);
}

static void backward()  // moves backward
{
  brushed_motor_backward(MCPWM_UNIT_0, MCPWM_TIMER_0, 60.0);
  brushed_motor_backward(MCPWM_UNIT_0, MCPWM_TIMER_1, 60.0);
}

static void stop()  // STop both motors
{
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_1);
}

static void turn_left()  // Turn left by driving only the right motor
{
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_1, 65.0);
}

static void turn_right()  // Turn Righ by driving only the left motor
{
  brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_1);
  brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, 70.0);
}
/* END MOTOR CONTROL */

////////////////////// HTTP SERVER /////////////////////// 

// Handles Rpi post commands and updates "com" variable
esp_err_t command_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            /*if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                Retry receiving if timeout occurred 
                continue;
            }
            return ESP_FAIL;*/
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }
    com[0] = buf[0];
    com[1] = buf[1];
    //Print command for error checking
    printf("Command: %c%c \n", com[0],com[1]);
   
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

httpd_uri_t command = {
    .uri       = "/command",
    .method    = HTTP_POST,
    .handler   = command_post_handler,
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
        httpd_register_uri_handler(server, &power);
        httpd_register_uri_handler(server, &command);
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

/////////////////////// HTTP CLIENT /////////////////////////

// HTTP client event handler
esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                printf("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void double_to_string(char *buf, int s)
{
  if (s > 999) s = 999;  // If a very large value is input, use 999 to prevent buffer issues
  if (s < 0) s = 0;  // If a negative number is input, use 0 to prevent buffer issues
  sprintf(buf, "%d", s);  // Convert the input number to a char array value
}
void sensor_get_request() 
{   
    //stores url and additions
    char url[100] = {0};
    char buf[5] = {0};
    double_to_string(buf,distFront); // converts sensor values to char array, stores in "buf"
    //append code and fob id to url 
    //Format: "sensor_URL/sensorvalue"
    strcat(url,sensor_url);
    strcat(url, buf);
    // Print URL for error checking
    printf("Sensor URL: ");
    for (int i = 0; i < strlen(url); i++)
        printf("%c", url[i]);
    printf("\n");

    //client configuration
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handle, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    printf( "Sensor Client Configuration Complete\n");

    // perform get request - rpi pulls sensor value from url
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("Status = %d, content_length = %d \n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
            printf( "CONNECTION FAILED\n");
    }
       
    esp_http_client_cleanup(client);
}

// rpi get request
void beacon_get_request() 
{   
    //stores url and additions
    char url[100] = {0};
    //append fragments and fob id to url 
    //Format: "beacon_url/ID_Fragment"
    strcat(url,beacon_url);
    strcat(url,fob);
    strcat(url,"_");
    strcat(url,frag1);
    strcat(url,frag2);
    strcat(url,frag3);
    strcat(url,frag4);
    strcat(url,frag5);
    strcat(url,frag6);
    strcat(url,frag7);
    strcat(url,frag8);
    strcat(url,frag9);
    strcat(url,frag10);

    //Print URL for error checking
    printf("Beacon URL: ");
    for (int i = 0; i < strlen(url); i++)
        printf("%c", url[i]);
    printf("\n");
    
    //client configuration
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handle, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    printf( "Beacon Client Configuration Complete\n");

    // perform get request - rpi pulls ID and Fragment from url
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("Status = %d, content_length = %d \n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
            printf( "CONNECTION FAILED\n");
    }
       
    esp_http_client_cleanup(client);
}

// initiates connection to show car is stopped
void stop_get_request() 
{   
    //stores url
    char url[50] = {0};
    strcat(url,stop_url);

    //Print URL for error chekcing
    printf("Stop URL: ");
    for (int i = 0; i < strlen(url); i++)
        printf("%c", url[i]);
    printf("\n");
    
    //client configuration
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handle, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    printf( "Beacon Client Configuration Complete\n");

    // perform get request - rpi pulls "stop" url
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("Status = %d, content_length = %d \n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
            printf( "CONNECTION FAILED\n");
    }
       
    esp_http_client_cleanup(client);
}


//timer isr updates vtimer
void IRAM_ATTR timer_isr(void *para){// timer group 0, ISR
    int timer_idx = (int) para;
    uint32_t intr_status = TIMERG0.int_st_timers.val; // access struct for interrupt status
    if((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        TIMERG0.hw_timer[timer_idx].update = 1;
        TIMERG0.int_clr_timers.t0 = 1;
        TIMERG0.hw_timer[timer_idx].config.alarm_en = 1;
        vtimer++;
    }
}

//timer intialization
static void vtimer_interrupt_init()
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
    //Configure timer
    timer_init(timer_group, timer_idx, &config);
    //Stop timer counter
    timer_pause(timer_group, timer_idx);
    //Load counter value
    timer_set_counter_value(timer_group, timer_idx, 0x00000000ULL);
    //Set alarm value
    timer_set_alarm_value(timer_group, timer_idx, (TIMER_INTERVAL0_SEC * TIMER_SCALE));
    //Enable timer interrupt
    timer_enable_intr(timer_group, timer_idx);
    //Set ISR handler
    timer_isr_register(timer_group, timer_idx, timer_isr, (void*) timer_idx, ESP_INTR_FLAG_IRAM, NULL);
    //Start timer counter
    timer_start(timer_group, timer_idx);
    printf("TIMER INITIALIZED \n");
}
//event handler starts/stops webserver based on wifi connection
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

//Wifi Initialization
void wifi_init_sta(void *arg)
{
    s_wifi_event_group = xEventGroupCreate();
    
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, arg) );
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    
    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

// UART Configuration
void ir_uart_config() {
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
    printf("UART Configuration Complete\n");
  }

void app_main()
{
  /* INITIALIZE COMPONENTS */
  mcpwm_motor_gpio_initialize();  // MOTOR: mcpwm gpio init
  mcpwm_motor_gpio_config();  // MOTOR: mcpwm configuration
  ir_uart_config();  // IR RX: uart init
  vtimer_interrupt_init();  // TIMER: init and config
  static httpd_handle_t server = NULL;  // HTTP: init and config
  ESP_ERROR_CHECK(nvs_flash_init());
  wifi_init_sta(&server);  // WIFI: init and config
  ultrasonic_sensor_t sensor = {
      .trigger_pin = TRIGGER_GPIO,
      .echo_pin = ECHO_GPIO
  };
  ultrasonic_init(&sensor);  // ULTRASONIC: init
  /* DONE INITIALIZING COMPONENTS */

  /* MAIN CODE */
  // Initialize variables
  uint8_t* data_in = (uint8_t*) malloc(IR_BUF_SIZE);  // IR RX: data buffer init
  while(true) {
    // Executed every 50 ms
    if (nvtimer != vtimer) {
      nvtimer = vtimer;  // Update the non-volatile timer variable with teh volatile timer value

        //Changes "nextstate" and "maxpulse" based on following commands:
        // F1 = small increment Forward 
        // F2 = medium increment Forward 
        // F3 = large increment Forward   
        // B = Backward 
        // R = Right    
        // L = Left   
        // FR = Forward Right 
        // FL = Forward Left  
        // BR = Right
        // BL = Left
        if (com[0] == 'B' && com[1] == '0') {
            nextState[0] = 'B';
            max_pulse = 10;
        } else if (com[0] == 'F' && com[1] == '1') {
             nextState[0] = 'F';
             max_pulse = 5;
        } else if (com[0] == 'F' && com[1] == '2') {
            nextState[0] = 'F';
            max_pulse = 10;
        } else if (com[0] == 'F' && com[1] == '3') {
            nextState[0] = 'F';
            max_pulse = 20;
        } else if (com[0] == 'F' && com[1] == 'R') {
            nextState[0] = 'R';
            max_pulse = 8;
        } else if (com[0] == 'F' && com[1] == 'L') {
            nextState[0] = 'L';
            max_pulse = 8;
        } else if (com[0] == 'B' && com[1] == 'R') {
            nextState[0] = 'R';
            max_pulse = 24;
        } else if (com[0] == 'B' && com[1] == 'L') {
            nextState[0] = 'L';
            max_pulse = 24;
        } else if (com[0] == 'L' && com[1] == '0') {
            nextState[0] = 'L';
            max_pulse = 16;
        } else if (com[0] == 'R' && com[1] == '0') {
            nextState[0] = 'R';
            max_pulse = 16;
        } else {
            com[0] = '0';
            com[1] = '0';
        }

        // reset command variable
        com[0] = '0'; 
        com[1] = '0';

      if (nvtimer % 20 == 0) { // Update the sensor values once per second
        sensor_get_request();
      }

      if (nvtimer % 2 == 0) {  // Processes executed every 100 ms
        prevState[0] = currState[0];  // GLOBAL: update states
        currState[0] = nextState[0];  // GLOBAL: update states
        distFront = ultrasonic_front();  // GLOBAL: update the front distance value
        front_error();  // PID: update front error value
      
        // IR RX: Read IR RX
        const int rxBytes = uart_read_bytes(UART_NUM, data_in, IR_BUF_SIZE, 20 / portTICK_RATE_MS);
        if (rxBytes > 0) {  // IR RX: if there is data, update the variable 'beacon'
          //gpio_set_level(LED, 1); // LED on when recieving data
          for (int i = 0; i < 24; i++) {
          //check for leading bit, store fob ID and Code,
          // and initiate get request
            if (data_in[i] == 0x1B) {
              fob[0] = data_in[i + 1] + 48;
              frag1[0] = data_in[i + 2];
              frag2[0] = data_in[i + 3];
              frag3[0] = data_in[i + 4];
              frag4[0] = data_in[i + 5];
              frag5[0] = data_in[i + 6];
              frag6[0] = data_in[i + 7];
              frag7[0] = data_in[i + 8];
              frag8[0] = data_in[i + 9];
              frag9[0] = data_in[i + 10];
              frag10[0] = data_in[i + 11];
              //printf("CODE: %c%c%c%c \n", c1[0],c2[0],c3[0],c4[0]);
              // printf("ID: %c \n", fob[0]);
              beacon_get_request();
              break;
            } 
              uart_flush(UART_NUM);  
            }    
        } else {
            //gpio_set_level(LED, 0); // LED off when no data recieved
        }
        char state = currState[0];
        switch (state) {
          case 'I':  // IDLE
            if (prevState[0] != 'I') {  // First time in Idle state
              stop_get_request();       // tell rpi car is stopped
              stop();  // Turn off the motors and stop the car
              nextState[0] = 'I';  // Stay in IDLE state (unless changed by Command)
            } else {  // Already in Idle state for at least 1 cycle
              nextState[0] = 'I';  // Stay in IDLE state (unless changed by Command)
            }
            break;
          case 'F':  // FORWARD
            printf("Forward: %c \n", prevState[0]);
            if (prevState[0] == 'I') {  
              stop();  // Stop the car
              forward(0.0,0.0);  // Turn on the motors and move forward at normal speed
              pcnt_left_turn_end = 0; // reset counting variable
              nextState[0] = 'C';  // moves to counting state
            } 
            break;
          case 'B':  // BACKWARD
           printf("RIGHT: %c \n", prevState[0]);  
            if (prevState[0] == 'I') {  // First time in Turning state
              stop();  // Turn off the motors and stop the car
              backward();  // Switch the motors to turn backward 
              pcnt_left_turn_end = 0;  // reset counting variable
              nextState[0] = 'C';      // moves to counting state
            } 
            break;
          case 'R':  // TURNING RIGHT
            printf("RIGHT: %c \n", prevState[0]);  
            if (prevState[0] == 'I') {  // First time in Turning state
              stop();  // Turn off the motors and stop the car
              turn_right();  //switch motors to turn right
              pcnt_left_turn_end = 0;  // reset counting variable
              nextState[0] = 'C';   // moves to counting state
            } 
            break;
          case 'L':  // TURNING LEFT
           printf("RIGHT: %c \n", prevState[0]);  
            if (prevState[0] == 'I') {  // First time in Turning state 
              stop();  // Turn off the motors and stop the car
              turn_left();  // Switch the motors to turn left
              pcnt_left_turn_end = 0;  //reset counting variable
              nextState[0] = 'C';      // moves to counting state
            } 
            break;
          case 'C':  // COUNTING - determines time of movement based on "maxpulse"
              pcnt_left_turn_end++;
              // if too close to an object and previous state was forward, return to IDLE
              // max pulse threshold allows for "F1" command so car can still inch forward
              if (frontError == 1 && max_pulse > 5 && prevState[0] == 'F') 
                nextState[0] = 'I';     
              if (pcnt_left_turn_end > max_pulse) {  // if counting variable is greater than max pulse, return to IDLE
                nextState[0] = 'I';
              }
          break;
          default:  // Default state - nothing to do here...
              printf("default \n");
        }
      }  // END processes executed every 100 ms
     }  // END processes executed every 50 ms
    }
  }
  /* END MAIN CODE */
