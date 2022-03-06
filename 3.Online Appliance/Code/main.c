#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "driver/gpio.h"
#include <http_server.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include <stdlib.h>
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp_system.h"
#include "esp_adc_cal.h"
#include <math.h>
#include <string.h>

//ESP IP: '192.168.1.113'

#define EXAMPLE_WIFI_SSID "Group_11" //CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS "smart-systems" //CONFIG_WIFI_PASSWORD

#define DEFAULT_VREF    1100
#define ADC1_EXAMPLE_CHANNEL    ADC1_CHANNEL_6
#define CIRCUIT_RESISTANCE 10000.0 //Equivalent resistance of resistors other than the thermistor


// "pulses" variable used to set angle of rotation of servo
// 500 = Minimum pulse width in microsecond
// 2150 = Maximum pulse width in microsecond
const int pulses[2] = {550,2150};
int leftright = 0;

static const char *TAG="APP";

//converts temp to character array
// "int t" automatically converts double to int
void double_to_string(char *cbuf, int t)
{
    sprintf(cbuf, "%d", t);
}

// gets temperature from thermistor
double getTemp()
{
    static esp_adc_cal_characteristics_t *adc_chars;
    uint32_t read_raw;
    double resistance;
    double temperature;
    
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_EXAMPLE_CHANNEL, ADC_ATTEN_DB_11);
    
    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    
    // Obtains the voltage through the thermistor to determine the thermistor's current
    // resistance --> determines the temperature based on the current resistance
    read_raw = adc1_get_raw(ADC1_EXAMPLE_CHANNEL);
    uint32_t voltage = (esp_adc_cal_raw_to_voltage(read_raw, adc_chars));
    resistance = (-1.0)*(CIRCUIT_RESISTANCE*(voltage/1000.0))/((voltage/1000.0) - 5);
    temperature = (650.0/pow((resistance/1000.0),1.4)); // degrees C
    temperature = (1.8 * temperature) + 32; // degrees F
    
    return temperature;
}

//get temperature and return as character array
esp_err_t temp_get_handler(httpd_req_t *req)
{
    double temp = getTemp();
    char str[2] = {0};
    double_to_string(str,temp);
    const char* resp_str = str;
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

httpd_uri_t temp = {
    .uri       = "/temp",
    .method    = HTTP_GET,
    .handler   = temp_get_handler,
    .user_ctx  = NULL
};

bool moveServo()
{
    // move left (bool = 0) turns OFF, right (bool = 1) turns ON
    if (leftright == 0) {
        mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pulses[leftright]); // Actually move the servo
        leftright++; // Increment the leftright variable to 1
        return 1;
    } else {
        mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pulses[leftright]); // Actually move the servo
        leftright--; // Increment the leftright variable back to 0
        return 0;
    }
}

//get handler sweeps servo and returns state of device
esp_err_t echo_get_handler(httpd_req_t *req)
{
    bool state = moveServo();
    if (state == 1) {
        const char* resp_str = "ON";
        httpd_resp_send(req, resp_str, strlen(resp_str));
    } else {
        const char* resp_str = "OFF";
        httpd_resp_send(req, resp_str, strlen(resp_str));
    }
    return ESP_OK;
}

httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_GET,
    .handler   = echo_get_handler,
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
        httpd_register_uri_handler(server, &temp);
        httpd_register_uri_handler(server, &echo);
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

//intiliaze wifi
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

//initialze servo
void initServo()
{
    // Set GPIO 15 as PWM0A (for servo)
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, 15);
    
    // Init mcpwm configuration
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 50;    // frequency = 50Hz, i.e. for every servo motor time period should be 20ms
    pwm_config.cmpr_a = 0;    // duty cycle of PWMxA = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    // Configure PWM0A with above settings
}

// main function
void app_main()
{
    initServo();
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(nvs_flash_init());
    initialise_wifi(&server);
}

