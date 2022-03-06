//Example code pulled from the ESP-IDF from https://github.com/espressif/esp-idf
//Modified to make the servo rotate through its full motion

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"

#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "driver/gpio.h"
#include "sdkconfig.h"


//You can get these value from the datasheet of servo you use, in general pulse width varies between 1000 to 2000 mocrosecond
#define SERVO_MIN_PULSEWIDTH 400
#define SERVO_MIN_PULSEWIDTH_MIN 495 //Minimum pulse width in microsecond
#define SERVO_MIN_PULSEWIDTH_SEC 526 //Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2300 //Maximum pulse width in microsecond
#define SERVO_MAX_DEGREE 59 //Maximum angle in degree upto which servo can rotate

uint32_t angle, count, counts, countm, angleOther, secondTracker, minuteCount;
bool sec_down = true;  // tracks whether the second hand is moving down or up; true means moving down

static void initialize_console()
{
    /* Disable buffering on stdin and stdout */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install(CONFIG_CONSOLE_UART_NUM,
            256, 0, 0, NULL, 0) );

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_CONSOLE_UART_NUM);

    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
            .hint_color = atoi(LOG_COLOR_CYAN)
#endif
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

#if CONFIG_STORE_HISTORY
    /* Load command history from filesystem */
    linenoiseHistoryLoad(HISTORY_PATH);
#endif
}

static void mcpwm_example_gpio_initialize()
{
    printf("initializing mcpwm servo control gpio......\n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, 15); //Set GPIO 15 as PWM0A (second servo)
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, 14); //set GPIO 14 as PWM1A (minute servo)
}

static void config_servos()
{
    mcpwm_example_gpio_initialize();
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 50;    //frequency = 50Hz, i.e. for every servo motor time period should be 20ms
    pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A & PWM0B with above settings
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);
}

/**
 * @brief Use this function to calcute pulse width for per degree rotation
 *
 * @param  degree_of_rotation the angle in degree to which servo has to rotate
 *
 * @return
 *     - calculated pulse width
 */
// Not used
static uint32_t servo_per_degree_init(uint32_t degree_of_rotation)
{
    uint32_t cal_pulsewidth = 0;
    cal_pulsewidth = (SERVO_MIN_PULSEWIDTH + (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * (degree_of_rotation)) / (SERVO_MAX_DEGREE)));
    return cal_pulsewidth;
}
// Calc minute angle
static uint32_t servo_per_degree_init_min(uint32_t degree_of_rotation)
{
    uint32_t cal_pulsewidth = 0;
    cal_pulsewidth = (SERVO_MAX_PULSEWIDTH - (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH_MIN) * (degree_of_rotation)) / (SERVO_MAX_DEGREE)));
    return cal_pulsewidth;
}
// Calc second angle
static uint32_t servo_per_degree_init_sec(uint32_t degree_of_rotation)
{
    uint32_t cal_pulsewidth = 0;
    cal_pulsewidth = (SERVO_MAX_PULSEWIDTH - (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH_SEC) * (degree_of_rotation)) / (SERVO_MAX_DEGREE)));
    return cal_pulsewidth;
}

/**
 * @brief Configure MCPWM module
 */
void mcpwm_example_servo_control(void *arg)
{
    //uint32_t angle, count, angleOther, secondTracker, minuteCount;
    //minuteCount = SERVO_MAX_DEGREE;
    //secondTracker = 0;
    //1. mcpwm gpio initializationcd
    //mcpwm_example_gpio_initialize();

    //2. initial mcpwm configuration
    // printf("Configuring Initial Parameters of mcpwm......\n");
    // mcpwm_config_t pwm_config;
    // pwm_config.frequency = 50;    //frequency = 50Hz, i.e. for every servo motor time period should be 20ms
    // pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
    // pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
    // pwm_config.counter_mode = MCPWM_UP_COUNTER;
    // pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    // mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A & PWM0B with above settings
    // mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);
    // angleOther = servo_per_degree_init_min(59);
    // mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, angleOther);
    // angle = servo_per_degree_init_sec(59);
    // mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
    while (1) {
        if (sec_down) {
          for (counts = secondTracker; counts <= 59; counts++) {
            secondTracker++;
            angle = servo_per_degree_init_sec(counts);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
            if (counts == 59) {
              minuteCount++;
              if(minuteCount == 60)
              {
                angleOther = servo_per_degree_init_min(0);
              }
              else
              {
                angleOther = servo_per_degree_init_min(minuteCount);
              }
              mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, angleOther);
              sec_down = false;
              secondTracker--;
            }

            vTaskDelay(100);
          }
        }
        else {
          for (counts = secondTracker; counts >= 0; counts--) { // ** CODE CHANGE UNTESTED was "counts > 0"
            secondTracker--;
            angle = servo_per_degree_init_sec(counts);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
            if (counts == 0) { // ** CODE CHANGE UNTESTED was "== 1"
              minuteCount++;
              if(minuteCount == 60)
              {
                angleOther = servo_per_degree_init_min(0);
              }
              else
              {
                angleOther = servo_per_degree_init_min(minuteCount);
              }
              mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, angleOther);
              sec_down = true;
            }

            vTaskDelay(100);
          }
        }
        // initializes the minute servo to its starting position
        // printf("\n");
        // printf("Minute Servo Moved\n");
        // printf("Angle of rotation: %d\n", minuteCount);
        // angleOther = servo_per_degree_init(minuteCount);
        // printf("pulse width: %dus\n", angleOther);
        // printf("\n");
        // mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, angleOther);

        // moves the servo in clockwise direction
        // for (count = SERVO_MAX_DEGREE; count > 0; count--) {
        //     secondTracker++;
        //     printf("Angle of rotation: %d\n", count);
        //     angle = servo_per_degree_init(count);
        //     printf("pulse width: %dus\n", angle);
        //     mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
        //     vTaskDelay(10);     //Add delay, since it takes time for servo to rotate, generally 100ms/60degree rotation at 5V
        //
        //     // moves the minute hand after 60 seconds
        //     if (secondTracker >= 59) {
        //         minuteCount--;
        //         secondTracker = 0;
        //         printf("\n");
        //         printf("Minute Servo Moved\n");
        //         printf("Angle of rotation: %d\n", minuteCount);
        //         angleOther = servo_per_degree_init(minuteCount);
        //         printf("pulse width: %dus\n", angleOther);
        //         printf("\n");
        //         mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, angleOther);
        //
        //         // after 30 minutes, the servo is set to the top
        //         if (minuteCount <= 1) {
        //             minuteCount = SERVO_MAX_DEGREE;
        //             printf("\n");
        //             printf("Minute Servo Moved\n");
        //             printf("Angle of rotation: %d\n", minuteCount);
        //             angleOther = servo_per_degree_init(minuteCount);
        //             printf("pulse width: %dus\n", angleOther);
        //             printf("\n");
        //             mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, angleOther);
        //           }
        //       }
        //
        //     // moves the servo in counterclockwise direction
        //     // it hits 30 seconds at the bottom
        //     if (count == 1) {
        //         for (count = 1; count < SERVO_MAX_DEGREE; count++) {
        //         secondTracker++;
        //         printf("Angle of rotation: %d\n", count);
        //         angle = servo_per_degree_init(count);
        //         printf("pulse width: %dus\n", angle);
        //         mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
        //         vTaskDelay(10);     //Add delay, since it takes time for servo to rotate, generally 100ms/60degree rotation at 5V
        //         }
        //         secondTracker++;
        //     }
        // }
    }
}

void app_main()
{
  initialize_console();
  const char* prompt = LOG_COLOR_I "esp32> " LOG_RESET_COLOR;

   /* Main loop */
   while(true) {
       /* Get a line using linenoise.
        * The line is returned when ENTER is pressed.
        */
    printf("\nEnter minutes as integer from 0-59:\n");
      char* line = linenoise(prompt);

      minuteCount = atoi( line );
      // set minutes variable to input value

      /* Add the command to the history */
      linenoiseHistoryAdd(line);

      /* linenoise allocates line buffer on the heap, so need to free it */
      linenoiseFree(line);

      printf("\nEnter seconds as integer from 0-59:\n");
      line = linenoise(prompt);
      secondTracker = atoi( line );
      // set seconds variable to input value

      break; //exit while loop
   }
    printf("Testing servo motor.......\n");

    config_servos();
    printf("servos config'd, setting angles\n");

    angleOther = servo_per_degree_init_min(minuteCount);
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, angleOther);
    angle = servo_per_degree_init_sec(secondTracker);
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);

    xTaskCreate(mcpwm_example_servo_control, "mcpwm_example_servo_control", 4096, NULL, 5, NULL);
}
