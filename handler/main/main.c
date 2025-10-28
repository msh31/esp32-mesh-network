#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h" // open-source real-0time OS on the ESP32
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"

// 64 bytes, 1 for the type, 3 for the data sent alongside
// typedef is just an alias to we can use 'Message' instead of 'struct Message' 
typedef struct {
    uint8_t type; 
    uint8_t data[63];
} Message;

void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const Message *msg = (const Message *)data;

    if(len != sizeof(Message)) {
        printf("Received invalid message.");
        return;
    }

    printf("Type: %u, first data byte: %u\n", msg->type, msg->data[0]);
    printf("Received from MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
       info->src_addr[0], info->src_addr[1], info->src_addr[2],
       info->src_addr[3], info->src_addr[4], info->src_addr[5]);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    ESP_ERROR_CHECK(esp_now_init());

    esp_now_register_recv_cb(on_data_recv);

    printf("Wifi Initialized!\n");
}
