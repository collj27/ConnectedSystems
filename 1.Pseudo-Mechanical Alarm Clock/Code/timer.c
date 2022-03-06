//
//  timer.c
//  
//
//  Created by James Coll on 9/23/18.
//

//
//  timer_interrupt.c
//
//
//  Created by James Coll on 9/20/18.
//

#include "timer.h"

/*
 with TIMER_FINE_ADJ factor of 1.4 , the pulse width is 0.9981 ms (500.9Hz)
 By making TIMER_FINE_ADJ 0, we brought down the error, now it is 499.9Hz
 */
#include <stdio.h>
#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/gpio.h"

#define TIMER_INTR_SEL TIMER_INTR_LEVEL  /*!< Timer level interrupt */
#define TIMER_GROUP    TIMER_GROUP_0     /*!< Test on timer group 0 */
#define TIMER_DIVIDER   80               /*!< Hardware timer clock divider, 80e6 to get 1MHz clock to timer */
#define TIMER_SCALE    (TIMER_BASE_CLK / TIMER_DIVIDER)  /*!< used to calculate counter value */
//#define TIMER_FINE_ADJ   (0*(TIMER_BASE_CLK / TIMER_DIVIDER)/1000000) /*!< used to compensate alarm value */
#define TIMER_INTERVAL0_SEC   (1)   /*!< test interval for timer 0 */
#define timer_idx        TIMER_0
volatile int time_seconds = 0;
volatile bool alarm_now = 0;
volatile int alarm = 5;


/*void alarm(){
    if (time_seconds == alarm){
         gpio_set_level(26,1);
    }
}
*/

//flash LED every second
void flash_LED(){
    if (time_seconds % 2 == 0) {
     gpio_set_level(26,0);
     } else {
     gpio_set_level(26,1);
     }
}

void IRAM_ATTR second_isr(void *para){// timer group 0, ISR
    int timer_idx = (int) para;
    uint32_t intr_status = TIMERG0.int_st_timers.val; // access struct for interrupt status
    if((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        TIMERG0.hw_timer[timer_idx].update = 1;
        TIMERG0.int_clr_timers.t0 = 1;
        TIMERG0.hw_timer[timer_idx].config.alarm_en = 1;
        time_seconds++;
        flash_LED();
        //alarm();
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
    printf("TIMER INITIALIZED \n");
    
}

void app_main()
{
    
    gpio_pad_select_gpio(26);
    gpio_set_direction(26, GPIO_MODE_OUTPUT);
    gpio_set_level(26,0);
    gpio_pulldown_en(26);
    printf("BEGIN DEMO \n");
    second_interrupt_init();
    
}
