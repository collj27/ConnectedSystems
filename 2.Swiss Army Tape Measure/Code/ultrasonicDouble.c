/* 
Alexander Trinh

Ultrasonic example code taken from: https://github.com/UncleRus/esp-idf-lib

Displays the measurements from the ultrasonic sensor to the console
*/

#include <stdio.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ultrasonic.h>

#define MAX_DISTANCE_CM 500 // 5m max
#define TRIGGER_GPIO 17 
#define ECHO_GPIO 16

void ultrasonic_test(void *pvParamters)
{
    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO
    };

    ultrasonic_init(&sensor);

    while (true)
    {
        int errorReturn = 9999;
        uint32_t distance;
        esp_err_t res = ultrasonic_measure_cm(&sensor, MAX_DISTANCE_CM, &distance);
        if (res != ESP_OK)
        {
            printf("%d\n", errorReturn);
        }
        else {
            if (distance > 400) {
                printf("%d\n", errorReturn);
            } else {
                printf("Distance: %0.2f m\n", distance / 100.0);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    xTaskCreate(ultrasonic_test, "ultrasonic_test", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
}

