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

typedef struct {
    uint8_t type; 
    uint8_t data[63];
} Message;

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
    printf("Wifi Initialized!\n");

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer)); // sets all bytes to 0 in the peer struct
    memcpy(peer.peer_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6); //copies the broadcast address into the peer address
    peer.channel = 0;  // the radio channel
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    printf("peer added!\n");

    while(true) {
        Message msg;
        msg.type = 0;

        memset(msg.data, 0, sizeof(msg.data));

        esp_now_send(peer.peer_addr, (uint8_t*)&msg, sizeof(msg));
        printf("Sent discovery message\n");
        
        vTaskDelay(pdMS_TO_TICKS(3000)); 
    }
}
