#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "soc/rmt_reg.h"
#include "driver/uart.h"
#include "driver/periph_ctrl.h"
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
#include "tcpip_adapter.h"

// RMT definitions
#define RMT_TX_CHANNEL    1     // RMT channel for transmitter
#define RMT_TX_GPIO_NUM   25    // GPIO number for transmitter signal -- A1
#define RMT_CLK_DIV       100   // RMT counter clock divider
#define RMT_TICK_10_US    (80000000/RMT_CLK_DIV/100000)   // RMT counter value for 10 us.(Source clock is APB clock)
#define rmt_item32_tIMEOUT_US   9500     // RMT receiver timeout value(us)

// UART definitions
#define UART_TX_GPIO_NUM 26 // A0
#define UART_RX_GPIO_NUM 34 // A2
#define BUF_SIZE (1024)
#define EXAMPLE_WIFI_SSID  "Group_11" //CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS  "smart-systems" //CONFIG_WIFI_PASSWORD

// Hardware interrupt definitions
#define GPIO_INPUT_IO_1       4
#define ESP_INTR_FLAG_DEFAULT 0
#define GPIO_INPUT_PIN_SEL    1ULL<<GPIO_INPUT_IO_1

// LED Output pins definitions
#define GREENPIN   15

// Default ID
#define ID 49 // ID = 1
// #define ID 50 // ID = 2
// #define ID 51 // ID = 3

// ID = 1 -> 192.168.1.133 port 80
// ID = 2 -> 192.168.1.117 port 80
// ID = 3 -> 192.168.1.104 port 80

// Define variables for start byte, ID, get req bools
char start = 0x0A;
char myID = (char) ID;
char rxID;
int len_out = 6;
volatile bool lock= true; // beacon detection
volatile bool unlock = false; // power ON/OFF
static const char *TAG="APP";

// Queue for button press
static xQueueHandle gpio_evt_queue = NULL;

// Button interrupt handler -- add to queue
static void IRAM_ATTR gpio_isr_handler(void* arg){
  uint32_t gpio_num = (uint32_t) arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

esp_err_t unlock_get_handler(httpd_req_t *req)
{
  if (lock == true) {
    lock = false;
    unlock = true;
    const char* resp_str = "UNLOCKED";
    httpd_resp_set_hdr(req, "access-control-allow-origin", "*");
        httpd_resp_send(req, resp_str, strlen(resp_str));
  } 
  else {
    const char* resp_str = "UNLOCKED";
    httpd_resp_set_hdr(req, "access-control-allow-origin", "*");
    httpd_resp_send(req, resp_str, strlen(resp_str));
  }
  return ESP_OK;
}

httpd_uri_t unlockURI = {
    .uri       = "/unlock",
    .method    = HTTP_GET,
    .handler   = unlock_get_handler,
    .user_ctx  = NULL
};


esp_err_t lock_get_handler(httpd_req_t *req)
{
  if (unlock == true) {
    lock = true;
    unlock = false;
    const char* resp_str = "LOCKED";
    httpd_resp_set_hdr(req, "access-control-allow-origin", "*");
    httpd_resp_send(req, resp_str, strlen(resp_str));
  } 
  else {
    const char* resp_str = "LOCKED";
    httpd_resp_set_hdr(req, "access-control-allow-origin", "*");
    httpd_resp_send(req, resp_str, strlen(resp_str));
  }
  return ESP_OK;
}

httpd_uri_t lockURI = {
    .uri       = "/lock", 
    .method    = HTTP_GET,
    .handler   = lock_get_handler,
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
      httpd_register_uri_handler(server, &lockURI);
      httpd_register_uri_handler(server, &unlockURI);
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

// RMT tx init
static void rmt_tx_init() {
  rmt_config_t rmt_tx;
  rmt_tx.channel = RMT_TX_CHANNEL;
  rmt_tx.gpio_num = RMT_TX_GPIO_NUM;
  rmt_tx.mem_block_num = 1;
  rmt_tx.clk_div = RMT_CLK_DIV;
  rmt_tx.tx_config.loop_en = false;
  rmt_tx.tx_config.carrier_duty_percent = 50;
  // Carrier Frequency of the IR receiver
  rmt_tx.tx_config.carrier_freq_hz = 38000;
  rmt_tx.tx_config.carrier_level = 1;
  rmt_tx.tx_config.carrier_en = 1;
  // Never idle -> aka ontinuous TX of 38kHz pulses
  rmt_tx.tx_config.idle_level = 1;
  rmt_tx.tx_config.idle_output_en = true;
  rmt_tx.rmt_mode = 0;
  rmt_config(&rmt_tx);
  rmt_driver_install(rmt_tx.channel, 0, 0);
}

// Configure UART
static void uart_init() {
  // Basic configs
  uart_config_t uart_config = {
    .baud_rate = 1200, // Slow BAUD rate
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };
  uart_param_config(UART_NUM_1, &uart_config);

  // Set UART pins using UART0 default pins
  uart_set_pin(UART_NUM_1, UART_TX_GPIO_NUM, UART_RX_GPIO_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  // Reverse receive logic line
  uart_set_line_inverse(UART_NUM_1,UART_INVERSE_RXD);

  // Install UART driver
  uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
}

// GPIO init for LEDs
static void led_init() {
  gpio_pad_select_gpio(GREENPIN);
  gpio_set_direction(GREENPIN, GPIO_MODE_OUTPUT);
}

// Button interrupt init
static void hw_int_init() {
  gpio_config_t io_conf;
  //interrupt of rising edge
  io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
  //bit mask of the pins, use GPIO4 here
  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
  //set as input mode
  io_conf.mode = GPIO_MODE_INPUT;
  //enable pull-up mode
  io_conf.pull_up_en = 1;
  gpio_config(&io_conf);
  gpio_intr_enable(GPIO_INPUT_IO_1 );
  //install gpio isr service
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
  //hook isr handler for specific gpio pin
  gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);
  //create a queue to handle gpio event from isr
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
  //start gpio task
}

// Send start byte, id, and code
void button_task() {
  uint32_t io_num;
  int counter = 0;
  if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
    printf("Button pressed.\n");
    while(counter < 5) {
      char *data_out = (char *) malloc(len_out);

      data_out[0] = start;
      data_out[1] = myID;
      data_out[2] = 55;
      data_out[3] = 53;
      data_out[4] = 54;
      data_out[5] = 56;

      uart_write_bytes(UART_NUM_1, data_out, len_out+1);

      vTaskDelay(5 / portTICK_PERIOD_MS);
      gpio_set_level(GREENPIN, 1);
      vTaskDelay(50);
      counter++;
    }
  vTaskDelay(100 / portTICK_PERIOD_MS);
  gpio_set_level(GREENPIN, 0);
  }
}

void app_main() {
  // Initialize button interrupt, LED, WiFi, tx, uart
  static httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(nvs_flash_init());
  initialise_wifi(&server);
  rmt_tx_init();
  uart_init();
  led_init();
  hw_int_init();

  // Start button task
  xTaskCreate(button_task, "button_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
}