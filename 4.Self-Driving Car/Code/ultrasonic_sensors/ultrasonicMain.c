/* 
Alexander Trinh

Ultrasonic example code taken from: https://github.com/UncleRus/esp-idf-lib

Returns values based on relative distance from wall/object using ultrasonic sensors 
*/

#include <stdio.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ultrasonic.h>

#define TRIGGER_GPIO 17 //(TX)
#define ECHO_GPIO 16 // (RX)
#define TRIGGER_GPIO2 26 // may need to change if required for motors (A0)
#define ECHO_GPIO2 34 // (A2)

#define FRONT_RANGE 20 // car must be > 20 cm from front wall
#define SIDE_RANGE 20 // 20 cm
#define SIDE_ERROR_RANGE 10 // car can be within 10 cm of the SIDE_RANGE

void ultrasonic_setup(void *pvParamters) {

    ultrasonic_sensor_t2 sensor2 = {
        .trigger_pin2 = TRIGGER_GPIO2,
        .echo_pin2 = ECHO_GPIO2
    };

    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO
    };

    ultrasonic_init(&sensor);
    ultrasonic_init2(&sensor2);

}

int ultrasonic_front(void *pvParamters) {
    uint32_t distance;
    esp_err_t res = ultrasonic_measure_cm(&sensor, MAX_DISTANCE_CM, &distance);
    // detects if car is too close to the wall in front
    if (distance < FRONT_RANGE) {
        return 0;
    }
    // car is within optimal range by not being too close to front wall
    else {
        return 1;
    }
    
}

int ultrasonic_side(void *pvParamters) {
    uint32_t distance2;
    esp_err_t res2 = ultrasonic_measure_cm2(&sensor2, MAX_DISTANCE_CM, &distance2);
    // detects if car is too close to the side wall
    if (distance2 < (SIDE_RANGE - SIDE_ERROR_RANGE)) {
        return 0;
    }
    // detects if car is too far from the side wall
    else if (distance2 > (SIDE_RANGE + SIDE_ERROR_RANGE)) {
        return 2;
    }
    // detects if car is within optimal range of the side wall
    else {
        return 1;
    }
}

void app_main()
{
    xTaskCreate(ultrasonic_setup, "ultrasonic_setup", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    xTaskCreate(ultrasonic_front, "ultrasonic_test", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    xTaskCreate(ultrasonic_side, "ultrasonic_test", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
}

