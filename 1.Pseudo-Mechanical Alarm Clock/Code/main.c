/* Quest 1: Alarm Clock
*/
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "esp_spi_flash.h"
#include "esp_types.h"
#include "driver/i2c.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "esp_vfs_dev.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
/* CONSOLE ADDITIONS */
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "sdkconfig.h"
int INPUTN = 1;
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
/* END CONSOLE ADDITIONS */

// GLOBAL VARIABLES
uint16_t time_seconds=0, time_minutes=0, time_hours=0, alarm_minutes=0, alarm_hours=0;
bool alarm_set=false; // true when alarm has been set - far right-hand decimal is lit
bool alarm_now=false; // true when alarm is going off until dismissed bu button press
bool flash_leds=false; // cycles between true and false to flash alarm LEDs on and off
uint16_t displaybuffer[4]; // holds values to send to alphaquad display
static const uint16_t alphadigittable[] =  { // array of digit values to send to quadalpha display
0b0000110000111111, // 0
0b0000000000000110, // 1
0b0000000011011011, // 2
0b0000000010001111, // 3
0b0000000011100110, // 4
0b0010000001101001, // 5
0b0000000011111101, // 6
0b0000000000000111, // 7
0b0000000011111111, // 8
0b0000000011101111, // 9
0b0000000000000000, // 10th value = [blank]
};
// i2c variables
#define HT16K33_BLINK_CMD 0x80
#define HT16K33_BLINK_DISPLAYON 0x01
#define HT16K33_BLINK_OFF 0
#define HT16K33_BLINK_2HZ 1
#define HT16K33_BLINK_1HZ 2
#define HT16K33_BLINK_HALFHZ 3
#define HT16K33_CMD_BRIGHTNESS 0xE0
#define DATA_LENGTH 512 /*!<Data buffer length for test buffer*/
#define RW_TEST_LENGTH 129 /*!<Data length for r/w test, any value from 0-DATA_LENGTH*/
#define I2C_EXAMPLE_MASTER_SCL_IO 22 /*!< gpio number for I2C master clock */
#define I2C_EXAMPLE_MASTER_SDA_IO 23 /*!< gpio number for I2C master data  */
#define I2C_EXAMPLE_MASTER_NUM I2C_NUM_1 /*!< I2C port number for master dev */
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_FREQ_HZ 100000 /*!< I2C master clock frequency */
#define SEG14_ADDR 0x70 /*!< slave address for 14-seg display */
#define WRITE_BIT I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ /*!< I2C master read */
#define ACK_CHECK_EN 0x1 /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0 /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0 /*!< I2C ack value */
#define NACK_VAL 0x1 /*!< I2C nack value */
// timer interrupt variables
#define TIMER_INTR_SEL TIMER_INTR_LEVEL  /*!< Timer level interrupt */
#define TIMER_GROUP    TIMER_GROUP_0     /*!< Test on timer group 0 */
#define TIMER_DIVIDER   80               /*!< Hardware timer clock divider, 80e6 to get 1MHz clock to timer */
#define TIMER_SCALE    (TIMER_BASE_CLK / TIMER_DIVIDER)  /*!< used to calculate counter value */
#define TIMER_INTERVAL0_SEC   (1)   /*!< test interval for timer 0 */
#define timer_idx        TIMER_0
volatile int vol_seconds = 0;
// servo variables
#define SERVO_MIN_PULSEWIDTH 400
#define SERVO_MIN_PULSEWIDTH_MIN 495 //Minimum pulse width in microsecond
#define SERVO_MIN_PULSEWIDTH_SEC 526 //Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2300 //Maximum pulse width in microsecond
#define SERVO_MAX_DEGREE 59 //Maximum angle in degree upto which servo can rotate
uint32_t count, counts, countm;
// END GLOBAL VARIABLES

// QUADALPHA FUNCTIONS
static void i2c_master_init() // i2c master initialization
{
    int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                       I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
}

void setBrightness(uint8_t b) {
  if (b > 15) b = 15;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SEG14_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, (HT16K33_CMD_BRIGHTNESS | b), ACK_CHECK_EN);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
}

void blinkRate(uint8_t b) {
  if (b > 3) b = 0; // turn off if not sure
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SEG14_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, (HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | (b << 1)), ACK_CHECK_EN);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
}

void dispBegin() {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SEG14_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, 0x21, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  blinkRate(HT16K33_BLINK_OFF);
  setBrightness(15); // max brightness
}

void dispWrite(void) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SEG14_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, 0x00, ACK_CHECK_EN);
  uint8_t i;
  for (i=0; i<4; i++) {
    i2c_master_write_byte(cmd,(displaybuffer[i] & 0xFF), ACK_CHECK_EN);
    i2c_master_write_byte(cmd,(displaybuffer[i] >> 8), ACK_CHECK_EN);
  }
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
}

void writeDigitAscii(uint8_t n, uint8_t a, bool decimalpoint) {
  uint16_t alphadigit = alphadigittable[a];
  if (decimalpoint) alphadigit += 0x4000; // display decimal point for current digit
  displaybuffer[n] = alphadigit;
}
// END QUADALPHA FUNCTIONS

// SERVO FUNCTIONS
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
// END SERVO FUNCTIONS

// MAIN CONTROL FUNCTIONS
void led_on()
{
  gpio_set_level(26, (flash_leds ? 1 : 0));
}

void set_led_hours(int inputh)
{
  bool pm = false; // true between noon and 11:59pm - far left-hand decimal is lit
  int disp;
  if (inputh == 0) {
    disp = 12;
  } else if (inputh > 12) {
    disp = inputh - 12;
    pm = true;
  } else {
    disp = inputh;
  }
  bool lo = (disp/10 > 0); // lo = "leading one"
  writeDigitAscii(1, (disp % 10), false);
  writeDigitAscii(0, (lo ? 1 : 10), (pm ? true : false));
  dispWrite();
}

void set_led_minutes(int inputm) // set the minutes on the quadalpha display
{
  writeDigitAscii(3, (inputm % 10), (alarm_set ? true : false));
  writeDigitAscii(2, (inputm / 10 > 0 ? inputm / 10 : 0), false);
  dispWrite();
}

void set_servo_minutes()
{
  mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, servo_per_degree_init_min(time_minutes));
}

void set_servo_seconds()
{
  mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, servo_per_degree_init_sec(time_seconds));
}

void update_time_hours()
{
  time_hours = (time_hours == 24 ? 0 : time_hours + 1);
  set_led_hours(time_hours);
}

void update_time_minutes()
{
  if (time_minutes == 59) {
    time_minutes = 0;
    update_time_hours();
  } else {
    time_minutes++;
  }
  set_servo_minutes();
  set_led_minutes(time_minutes);
  if (time_hours == alarm_hours && time_minutes == alarm_minutes) {
    alarm_now = true;
  }
}

void update_time_seconds()
{
  if (time_seconds == 0) {
    update_time_minutes();
  }
  set_servo_seconds();
}

void second()
{
  update_time_seconds();
  if (alarm_now) {
    flash_leds = !flash_leds;
    led_on();
  }
}
// END MAIN CONTROL FUNCTIONS

void update_seconds(){
  if (vol_seconds == 60) {
    vol_seconds = 0;
  }
}

// TIMER INTERRUPT FUNCTIONS
void IRAM_ATTR second_isr(void *para){// timer group 0, ISR
    int timer_idx = (int) para;
    uint32_t intr_status = TIMERG0.int_st_timers.val; // access struct for interrupt status
    if((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        TIMERG0.hw_timer[timer_idx].update = 1;
        TIMERG0.int_clr_timers.t0 = 1;
        TIMERG0.hw_timer[timer_idx].config.alarm_en = 1;
        vol_seconds++;
        update_seconds();
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
// END TIMER INTERRUPT FUNCTIONS

void app_main()
{
  // init LED for alarm
  gpio_pad_select_gpio(26);
  gpio_set_direction(26, GPIO_MODE_OUTPUT);
  gpio_set_level(26,0);
  gpio_pulldown_en(26);
  // init various supporting pieces
  i2c_master_init();
  dispBegin();
  config_servos();
  second_interrupt_init();

  /* CONSOLE ADDITIONS */
  initialize_console();
  const char* prompt = LOG_COLOR_I "esp32> " LOG_RESET_COLOR;
  /* Get a line using linenoise.
   * The line is returned when ENTER is pressed.
   */
   printf("SET TIME: Enter hours as integer from 0-23: \n");
   char* line = linenoise(prompt);
   INPUTN = atoi( line );
   // set quadalpha hours to INPUTN
   time_hours = INPUTN;
   set_led_hours(time_hours);
   /* Add the command to the history */
   linenoiseHistoryAdd(line);
   /* linenoise allocates line buffer on the heap, so need to free it */
   linenoiseFree(line);

   printf("SET TIME: Enter minutes as integer from 0-59: \n");
   line = linenoise(prompt);
   INPUTN = atoi( line );
   // set quadalpha minutes to INPUTN
   time_minutes = INPUTN;
   set_led_minutes(time_minutes);
   /* Add the command to the history */
   linenoiseHistoryAdd(line);
   /* linenoise allocates line buffer on the heap, so need to free it */
   linenoiseFree(line);

   printf("TIME SET! Now set the alarm.\n");
   printf("SET ALARM: Enter hours as integer from 0-23: \n");
   line = linenoise(prompt);
   INPUTN = atoi( line );
   // set quadalpha hours to INPUTN
   alarm_hours = INPUTN;
   /* Add the command to the history */
   linenoiseHistoryAdd(line);
   /* linenoise allocates line buffer on the heap, so need to free it */
   linenoiseFree(line);

   printf("SET ALARM: Enter minutes as integer from 0-59: \n");
   line = linenoise(prompt);
   INPUTN = atoi( line );
   // set quadalpha minutes to INPUTN
   alarm_minutes = INPUTN;
   /* Add the command to the history */
   linenoiseHistoryAdd(line);
   /* linenoise allocates line buffer on the heap, so need to free it */
   linenoiseFree(line);

   while (true) {
     if (time_seconds != vol_seconds) {
       time_seconds = vol_seconds;
       second();
       if (alarm_now) {
         printf("!! Hit enter key to silence alarm !!\n");
         char* line = linenoise(prompt);
         flash_leds = false;
         alarm_now = false;
         led_on();
         /* linenoise allocates line buffer on the heap, so need to free it */
         linenoiseFree(line);
         printf("Alarm silenced until tomorrow...\n");
       }
     }
   }

}
// END MAIN CONTROL FUNCTIONS
