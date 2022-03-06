/* Alexander Trinh
Displays current ambient temperature and resistance of thermistor 
Example code taken from: https://github.com/espressif/esp-idf/tree/master/examples/peripherals/adc2
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp_system.h"
#include "esp_adc_cal.h"

#define DAC_EXAMPLE_CHANNEL     CONFIG_DAC_EXAMPLE_CHANNEL
#define ADC2_EXAMPLE_CHANNEL    CONFIG_ADC2_EXAMPLE_CHANNEL
#define CIRCUIT_RESISTANCE 10000.0 //Equivalent resistance of resistors other than the thermistor

void app_main(void)
{
    uint8_t output_data=0;
    int read_raw;
    double resistance, temperature;
    esp_err_t r;

    gpio_num_t adc_gpio_num, dac_gpio_num;

    r = adc2_pad_get_io_num( ADC2_EXAMPLE_CHANNEL, &adc_gpio_num );
    assert( r == ESP_OK );
    r = dac_pad_get_io_num( DAC_EXAMPLE_CHANNEL, &dac_gpio_num );
    assert( r == ESP_OK );

    printf("ADC channel %d @ GPIO %d, DAC channel %d @ GPIO %d.\n", ADC2_EXAMPLE_CHANNEL, adc_gpio_num,
                DAC_EXAMPLE_CHANNEL, dac_gpio_num );

    dac_output_enable( DAC_EXAMPLE_CHANNEL );

    //be sure to do the init before using adc2. 
    printf("adc2_init...\n");
    adc2_config_channel_atten( ADC2_EXAMPLE_CHANNEL, ADC_ATTEN_0db );

    vTaskDelay(2 * portTICK_PERIOD_MS);

    printf("start conversion.\n");
    // Obtains the voltage through the thermistor to determine the thermistor's current 
    // resistance --> determines the temperature based on the current resistance
    while(1) {
        dac_output_voltage( DAC_EXAMPLE_CHANNEL, output_data++ );
        r = adc2_get_raw( ADC2_EXAMPLE_CHANNEL, ADC_WIDTH_12Bit, &read_raw);
        if ( r == ESP_OK ) {
            // calculates the temperature and resistance of the thermistor using the read voltage 
            read_raw -= 950;
            // equation derived from voltage divider formula
            resistance = (-1.0)*(CIRCUIT_RESISTANCE*(read_raw/1000.0))/((read_raw/1000.0)-5.0);
            // equation derived from thermistor datasheet and graphing
            temperature = (650.0/pow((resistance/1000.0),1.4));
            printf("%d: %d\n", output_data, read_raw);
            printf("\nTemp(C): %0.2f Res: %0.2f\n", temperature, resistance);
        vTaskDelay( 2 * portTICK_PERIOD_MS );
        }
    }
}
