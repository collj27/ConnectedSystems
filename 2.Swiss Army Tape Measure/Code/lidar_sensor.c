//
//  LIDAR.c
//  
//
//  Created by James Coll on 9/28/18.

#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"

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
        int num = 1;
        int val = bool_array[i];
        int exp = 15 - i;
        for (int j = 0; j < exp; j++) {
            num = num * 2;
        }
        sum = sum + num * val;
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

void app_main() {
    // Configure uart and buffer
    uart_config();
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    while (1) {
        const int rxBytes =  uart_read_bytes(UART_NUM, data, 9, 10 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            int low_byte = data[2];  // low byte distance (automatically converts hex to dec), cm
            int high_byte = data[3]; // high byte distance, cm
            double distance;
            if (high_byte == 0) {
                distance = low_byte;
                printf("distance =  %f m \n", (distance / 100));
            } else {
                //convert high byte distance from decimal to boolean and solve for proper distance
                int num = dec_to_bool(high_byte);
                distance = low_byte + num;
                printf("distance =  %f m \n", (distance / 100));
            }
            uart_flush(UART_NUM);                   // flush buffer
            vTaskDelay(1000 / portTICK_RATE_MS);
        } else {
            printf("NO INCOMING DATA");
        }
    }
}
