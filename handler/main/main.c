#include <stdio.h>
#include "freertos/FreeRTOS.h" // open-source real-0time OS on the ESP32
#include "freertos/task.h"

void app_main(void) {
    printf("Handler is starting...\n");

    while(1) {
        printf("Hello!\n");

        //sleeps for 2 seconds, converts the amount of ms to ticks the ESP32 understands
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
