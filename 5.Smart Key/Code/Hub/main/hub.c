#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_client.h"
#include "esp_intr_alloc.h"
#include "soc/soc.h"
#include "esp_types.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"

bool flag = false;
char url[31] = "http://192.168.1.108:3000/hub/";
char fob[1] = "0";
 
#define EXAMPLE_ESP_WIFI_SSID   "Group_11" //CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS  "smart-systems" //CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  5 // CONFIG_ESP_MAXIMUM_RETRY
#define UART_NUM  UART_NUM_1
#define BUF_SIZE (1024)
#define RX       (16)
#define TX       (17)
#define BUF_SIZE (1024)
#define ESP_INTR_FLAG_DEFAULT 0
#define TIMER_INTR_SEL TIMER_INTR_LEVEL  /*!< Timer level interrupt */
#define TIMER_GROUP    TIMER_GROUP_0     /*!< Test on timer group 0 */
#define TIMER_DIVIDER   80               /*!< Hardware timer clock divider, 80e6 to get 1MHz clock to timer */
#define TIMER_SCALE    (TIMER_BASE_CLK / TIMER_DIVIDER)  /*!< used to calculate counter value */
#define TIMER_INTERVAL0_SEC   (0.1)   /*!< test interval for timer 0 */
#define timer_idx        TIMER_0
#define LED (21)

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

// The event group allows multiple bits for each event, but we only care about one event
// *- are we connected to the AP with an IP?
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "wifi station";

static int s_retry_num = 0;

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

void get_request() 
{
   strcat(url,fob);
   esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handle, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    printf( "Client Configuration Complete\n");
    // get request to rpi - should use updated url
     esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
         printf( "CONNECTION FAILED\n");
    }
    esp_http_client_cleanup(client);
}

/*void read_data() 
{
    //client_config(); // config client
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE); // fob_id.
    const int rxBytes =  uart_read_bytes(UART_NUM, data, 2, 10 / portTICK_RATE_MS);
    if (rxBytes > 0) {
        //print data
        printf("data: %d \n", data[2]);
        if (fob[0] == 0) {
            if (data[2] == 255) {
                fob[0] = 3;
            } else if (data[2] == 254) {
                fob[0] = 2;
            } else if (data[2] == 253) {
                fob[0] = 1;
            }
        }

          printf("fob_id: %c \n", fob[0]);

        for(int i = 0; i < 2; i++) {
             printf("fob_id: %d\n", data[i]);
        }
        if (fob[0] == 0) {
            //convert data to char
            //update fob 
            get_request();

        }
        uart_flush(UART_NUM);  
        gpio_set_level(LED, 1);                 
    } else {
        // No incoming data
        fob[0] = 0;
        gpio_set_level(LED, 0);
       // printf("NO DATA \n");
    }

}*/

//timer isr - checks button state every 0.2 seconds
void IRAM_ATTR timer_isr(void *para){// timer group 0, ISR
    int timer_idx = (int) para;
    uint32_t intr_status = TIMERG0.int_st_timers.val; // access struct for interrupt status
    if((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        TIMERG0.hw_timer[timer_idx].update = 1;
        TIMERG0.int_clr_timers.t0 = 1;
        TIMERG0.hw_timer[timer_idx].config.alarm_en = 1;
        if (flag == false) {
           flag = true;
        }
    }
}

//timer intialization
static void timer_interrupt_init()
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


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "got ip:%s",
                     ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            //get_request(); // should read data first then call get_request
            timer_interrupt_init();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                s_retry_num++;
                ESP_LOGI(TAG,"retry to connect to the AP");
            }
            ESP_LOGI(TAG,"connect to the AP fail\n");
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

void wifi_init_sta()
{
    s_wifi_event_group = xEventGroupCreate();
    
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );
    
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

// Configure parameters of an UART driver/ communication pins
void uart_config() {
    uart_config_t uart_config = {
        .baud_rate = 2400,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TX, RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_line_inverse(UART_NUM, UART_INVERSE_RXD);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    printf("UART Configuration Complete\n");
}

void app_main()
{   
    gpio_pad_select_gpio(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    

    uart_config();
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE); // fob_id.
    while (true) {
       if (flag == true) {
            //read_data();
            const int rxBytes =  uart_read_bytes(UART_NUM, data, 2, 10 / portTICK_RATE_MS);
            if (rxBytes > 0) {
                for(int i = 0; i < 2; i++) {
                     printf("data: %d \n", data[2]);
                }
               
               /* if (fob[0] == '0') {
                    for(int i = 0; i < 2; i++) {
                        printf("data: %d \n", data[i]);
                        if (data[i] == 2) {
                            fob[0] = '2';
                        } else if (data[i] == 1) {
                            fob[0] = '1';
                        } else if (data[i] == 0) {
                            fob[0] = '0';
                        }
                    }
                }
                printf("fob_id: %c \n", fob[0]);
                //get_request();
                uart_flush(UART_NUM);  
                gpio_set_level(LED, 1);                 
            } else {
                // No incoming data
               // printf("NO DATA\n");
                fob[0] = '0';
                gpio_set_level(LED, 0);
               // printf("NO DATA \n");*/
            }
            flag = false;
        }
    }

}
