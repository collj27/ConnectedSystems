//Example code pulled from the ESP-IDF from https://github.com/espressif/esp-idf
//Modified to make the servo rotate through its full motion 

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"

#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

//You can get these value from the datasheet of servo you use, in general pulse width varies between 1000 to 2000 mocrosecond
#define SERVO_MIN_PULSEWIDTH 400 //Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2300 //Maximum pulse width in microsecond
#define SERVO_MAX_DEGREE 30 //Maximum angle in degree upto which servo can rotate

static void mcpwm_example_gpio_initialize()
{
    printf("initializing mcpwm servo control gpio......\n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, 15); //Set GPIO 15 as PWM0A (second servo)
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, 14); //set GPIO 14 as PWM1A (minute servo)
}

/**
 * @brief Use this function to calcute pulse width for per degree rotation
 *
 * @param  degree_of_rotation the angle in degree to which servo has to rotate
 *
 * @return
 *     - calculated pulse width
 */
static uint32_t servo_per_degree_init(uint32_t degree_of_rotation)
{
    uint32_t cal_pulsewidth = 0;
    cal_pulsewidth = (SERVO_MIN_PULSEWIDTH + (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * (degree_of_rotation)) / (SERVO_MAX_DEGREE)));
    return cal_pulsewidth;
}

/**
 * @brief Configure MCPWM module
 */
void mcpwm_example_servo_control(void *arg)
{
    uint32_t angle, count, angleOther, secondTracker, minuteCount;
    minuteCount = SERVO_MAX_DEGREE;
    secondTracker = 1;
    //1. mcpwm gpio initializationcd 
    mcpwm_example_gpio_initialize();

    //2. initial mcpwm configuration
    printf("Configuring Initial Parameters of mcpwm......\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 50;    //frequency = 50Hz, i.e. for every servo motor time period should be 20ms
    pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A & PWM0B with above settings
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config); 
    while (1) {
        // initializes the minute servo to its starting position
        printf("\n");
        printf("Minute Servo Moved\n");
        printf("Angle of rotation: %d\n", minuteCount);
        angleOther = servo_per_degree_init(minuteCount);
        printf("pulse width: %dus\n", angleOther);
        printf("\n");
        mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, angleOther);

        // moves the servo in clockwise direction
        for (count = SERVO_MAX_DEGREE; count > 0; count--) {
            secondTracker++;
            printf("Angle of rotation: %d\n", count);
            angle = servo_per_degree_init(count);
            printf("pulse width: %dus\n", angle);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
            vTaskDelay(10);     //Add delay, since it takes time for servo to rotate, generally 100ms/60degree rotation at 5V
            
            // moves the minute hand after 60 seconds
            if (secondTracker >= 59) {
                minuteCount--;
                secondTracker = 1;
                printf("\n");
                printf("Minute Servo Moved\n");
                printf("Angle of rotation: %d\n", minuteCount);
                angleOther = servo_per_degree_init(minuteCount);
                printf("pulse width: %dus\n", angleOther);
                printf("\n");
                mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, angleOther);

                // after 30 minutes, the servo is set to the top 
                if (minuteCount <= 1) {
                    minuteCount = SERVO_MAX_DEGREE;
                    printf("\n");
                    printf("Minute Servo Moved\n");
                    printf("Angle of rotation: %d\n", minuteCount);
                    angleOther = servo_per_degree_init(minuteCount);
                    printf("pulse width: %dus\n", angleOther);
                    printf("\n");
                    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, angleOther);
                    }
                }

            // moves the servo in counterclockwise direction  
            // it hits 30 seconds at the bottom
            if (count == 1) {
                for (count = 1; count < SERVO_MAX_DEGREE; count++) {
                secondTracker++;
                printf("Angle of rotation: %d\n", count);
                angle = servo_per_degree_init(count);
                printf("pulse width: %dus\n", angle);
                mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
                vTaskDelay(10);     //Add delay, since it takes time for servo to rotate, generally 100ms/60degree rotation at 5V
                }
                secondTracker++;
            }
        }
    }
}

void app_main()
{
    printf("Testing servo motor.......\n");
    xTaskCreate(mcpwm_example_servo_control, "mcpwm_example_servo_control", 4096, NULL, 5, NULL);
}
