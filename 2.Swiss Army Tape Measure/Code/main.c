/* Quest 2: Measurements */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_types.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "soc/gpio_sig_map.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include <ultrasonic.h>

/* TIMER Below */
#define TIMER_INTR_SEL TIMER_INTR_LEVEL  /*!< Timer level interrupt */
#define TIMER_GROUP    TIMER_GROUP_0     /*!< Test on timer group 0 */
#define TIMER_DIVIDER   80               /*!< Hardware timer clock divider, 80e6 to get 1MHz clock to timer */
#define TIMER_SCALE    (TIMER_BASE_CLK / TIMER_DIVIDER)  /*!< used to calculate counter value */
#define TIMER_INTERVAL0_SEC   (0.5)   /*!< timer set to half second */
#define timer_idx        TIMER_0
volatile int vol_seconds = 0;
uint16_t time_seconds = 0;
int16_t pcnt_count = 0;

void IRAM_ATTR second_isr(void *para){// timer group 0, ISR
    int timer_idx = (int) para;
    uint32_t intr_status = TIMERG0.int_st_timers.val; // access struct for interrupt status
    if((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        TIMERG0.hw_timer[timer_idx].update = 1;
        TIMERG0.int_clr_timers.t0 = 1;
        TIMERG0.hw_timer[timer_idx].config.alarm_en = 1;
        vol_seconds++;
    }
}
static void second_interrupt_init()
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
    timer_isr_register(timer_group, timer_idx, second_isr, (void*) timer_idx, ESP_INTR_FLAG_IRAM, NULL);
    /*Start timer counter*/
    timer_start(timer_group, timer_idx);
    //printf("TIMER INITIALIZED \n");
}
/* TIMER Above */

/* PCNT Below */
#define PCNT_TEST_UNIT      PCNT_UNIT_0
#define PCNT_H_LIM_VAL      1000
#define PCNT_L_LIM_VAL     -10
#define PCNT_THRESH1_VAL    5
#define PCNT_THRESH0_VAL   -5
#define PCNT_INPUT_SIG_IO   39  // Pulse Input GPIO
#define PCNT_INPUT_CTRL_IO  5  // Control GPIO HIGH=count up, LOW=count down
xQueueHandle pcnt_evt_queue;   // A queue to handle pulse counter events
pcnt_isr_handle_t user_isr_handle = NULL; //user's ISR service handle

static void pcnt_init(void)
{
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = PCNT_INPUT_SIG_IO,
        .ctrl_gpio_num = PCNT_INPUT_CTRL_IO,
        .channel = PCNT_CHANNEL_0,
        .unit = PCNT_TEST_UNIT,
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
    /* Initialize PCNT unit */
    pcnt_unit_config(&pcnt_config);
    /* Configure and enable the input filter */
    pcnt_set_filter_value(PCNT_TEST_UNIT, 100);
    pcnt_filter_enable(PCNT_TEST_UNIT);
    /* Initialize PCNT's counter */
    pcnt_counter_pause(PCNT_TEST_UNIT);
    pcnt_counter_clear(PCNT_TEST_UNIT);
    /* Everything is set up, now go to counting */
    pcnt_counter_resume(PCNT_TEST_UNIT);
}
/* PCNT Above */

/* ADC Common Code Below */
#define DEFAULT_VREF    1100
#define NO_OF_SAMPLES   64          //Multisampling
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34
static const adc_channel_t channel2 = ADC2_CHANNEL_7;   //GPIO27
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_atten_t atten2 = ADC_ATTEN_0db;
static const adc_unit_t unit = ADC_UNIT_1;
/* ADC Common Code Above */

/* LIDAR Below */
#define UART_NUM  UART_NUM_0
#define BUF_SIZE (1024)
#define RX       (16)
#define TX       (17)
// converts 8 bit high byte distance to bool and outputs distance in cm
int dec_to_bool(int num) {
    static int bool_array[8] = {0};
    int rem = 0;
    int count = 7;
    while (num > 0){
        rem = num % 2;
        bool_array[count] = rem;
        count = count - 1;
        num = num / 2;
    }
    int sum = 0;
    for (int i = 0; i < 8; i++) {
        int snum = 1;
        int val = bool_array[i];
        int exp = 15 - i;
        for (int j = 0; j < exp; j++) {
            snum = snum * 2;
        }
        sum = sum + snum * val;
    }
    return sum;
}

//Configure parameters of an UART driver/ communication pins
void uart_config() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TX, RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
}
/* LIDAR Above */

/* Sonic - Double Below */
#define MAX_DISTANCE_CM 500 // 5m max
#define TRIGGER_GPIO 25
#define ECHO_GPIO 26
/* Sonic - Double Above */

void app_main()
{
    /* Init Items */
    pcnt_init();
    second_interrupt_init();
    int16_t prev_count = 0;
    int16_t curr_count = 0;
    float wheel_ms = 0.0;
    float irrange = 0.0;
    float lidar = 0.0;
    float sonic_single_m = 0.0;
    float sonic_double_m = 0.0;
    double adc_reading_sonicsingle;
    uint32_t adc_reading_irrange;
    int read_raw = 0;
    float distance_lidar;
    int rxBytes = 0;

    /* UART and Buffer Init and Config */
    uart_config();
    uint8_t* uartdata = (uint8_t*) malloc(BUF_SIZE);

    /* ADC Init and Config */
    //Configure ADC - Sonic Single
    adc2_config_channel_atten(channel2, atten2);
    //Configure ADC - IR Range Sensor
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(channel, atten);

    /* Init Sonic - Double */
    float errorReturn = 9999;
    uint32_t distance_sonic;
    ultrasonic_sensor_t sensor = {
      .trigger_pin = TRIGGER_GPIO,
      .echo_pin = ECHO_GPIO
    };
    ultrasonic_init(&sensor);

    while (true) {
      if (time_seconds != vol_seconds) {  // Every half second, read and output values
        time_seconds = vol_seconds;

        /* Wheel Speed */
        pcnt_get_counter_value(PCNT_TEST_UNIT, &pcnt_count);
        curr_count = pcnt_count;
         // M/s = Distance / Time
         // Time = 0.5 seconds
         // Distance = number of ticks * 2piR/10
         // -- R = radius of disk = 0.5 inch = 0.0127 m (1.27 cm); 2(3.14)(0.0127) ~ 0.07979645
         // -- 10 is the number of ticks over the entire circumference; 0.07979645/10 = 0.007979645
        wheel_ms = (curr_count - prev_count) * (0.007979645) / 0.5; //wheel speed

        /* IR Range Sensor */
        adc_reading_irrange = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
          adc_reading_irrange += adc1_get_raw((adc1_channel_t)channel);
        }
        adc_reading_irrange /= NO_OF_SAMPLES; // takes average of 64 samples
        double v = adc_reading_irrange / 3.225;
        double y = -0.935;
        double d = 10655.08 * pow(v,y);  //cm
        irrange = d / 100;  //m

        /* LIDAR Sensor */
        rxBytes =  uart_read_bytes(UART_NUM, uartdata, 9, 10 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            int low_byte = uartdata[2];  // low byte distance (automatically converts hex to dec), cm
            int high_byte = uartdata[3]; // high byte distance, cm
            if (high_byte == 0) {
                distance_lidar = low_byte;
                //printf("distance =  %f m \n", (distance / 100));
            } else {
                //convert high byte distance from decimal to boolean and solve for proper distance
                int num = dec_to_bool(high_byte);
                distance_lidar = low_byte + num;
                //printf("distance =  %f m \n", (distance / 100));
            }
            uart_flush(UART_NUM);                   // flush buffer
        } else {
            distance_lidar = 0.0;
            //printf("NO INCOMING DATA");
        }
        lidar = distance_lidar / 100;

        /* Sonic Single */
        adc_reading_sonicsingle = 0;
        esp_err_t r = adc2_get_raw( channel2, ADC_WIDTH_12Bit, &read_raw);
        adc_reading_sonicsingle = read_raw;
        sonic_single_m = (adc_reading_sonicsingle*(5/4))/1000;
        /* Sonic Double */
        esp_err_t res = ultrasonic_measure_cm(&sensor, MAX_DISTANCE_CM, &distance_sonic);
        if (res != ESP_OK)
        {
          sonic_double_m = errorReturn;
        }
        else {
          if (distance_sonic > 400) {
            sonic_double_m = errorReturn;
          } else {
            sonic_double_m = distance_sonic / 100.0;
          }
        }

        /* Output all values */
        printf("%f,%f,%f,%f,%f\n", wheel_ms,irrange,lidar,sonic_single_m,sonic_double_m);
        prev_count = curr_count;
      }
    }

    if(user_isr_handle) {
        //Free the ISR service handle.
        esp_intr_free(user_isr_handle);
        user_isr_handle = NULL;
    }
}
