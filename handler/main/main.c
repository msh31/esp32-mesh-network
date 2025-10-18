#include <stdio.h>
#include "freertos/FreeRTOS.h" // open-source real-0time OS on the ESP32
#include "freertos/task.h"

void test_task(void *pvParameters) {
    printf("Running on core %d\n", xPortGetCoreID());
    while (1) {
        printf("The Test Task executed!\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void another_task(void *pvParameters) {
    printf("Running on core %d\n", xPortGetCoreID());
    while (1) {
        printf("Another Task has executed!\n");
        vTaskDelay(1500 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    printf("Executing tasks...\n");

    xTaskCreate(test_task, "TestTask", 2048, NULL, 5, NULL);
    xTaskCreate(another_task, "AnotherTask", 2048, NULL, 5, NULL);
}
