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

const char *discovery_secret = "TkFLRURfU05BS0U=";
bool is_connected = false;
bool peer_upgraded = false;
uint8_t pmk[16] = {
    0xF3, 0x26, 0xA5, 0xC3, 0x9C, 0xC0, 0x8E, 0xC0,
    0x15, 0xAB, 0x90, 0x69, 0x8C, 0x7E, 0x6F, 0x8C
};
uint8_t handlerMac[6];

void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const Message *msg = (const Message *)data;

    if(len != sizeof(Message)) {
        printf("Received invalid message.");
        return;
    }

    if(msg->type == 0) {
        // printf("well, this shouldn't have happened. closing the connection.");
        return;
    }

    if(msg->type == 1) {
        if(memcmp(msg->data, discovery_secret, strlen(discovery_secret)) != 0) {
            printf("Invalid secret, rejecting agent\n");
            return;
        }

        printf("Acknowledgment received from coordinator!\n");
        memcpy(handlerMac, info->src_addr, 6);
        is_connected = true;
    }
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
    ESP_ERROR_CHECK(esp_now_set_pmk(pmk));

    esp_now_register_recv_cb(on_data_recv);
    printf("Wifi Initialized!\n");

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer)); // sets all bytes to 0 in the peer struct
    memcpy(peer.peer_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6); //copies the broadcast address into the peer address
    memcpy(peer.lmk, pmk, 16);
    peer.channel = 0;  // the radio channel
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    printf("peer added!\n");

    while(!is_connected) {
        Message msg;
        msg.type = 0;
        memcpy(msg.data, discovery_secret, strlen(discovery_secret));
        // printf("Sending secret: %.*s (length: %zu)\n", (int)strlen(discovery_secret), msg.data, strlen(discovery_secret));

        esp_now_send(peer.peer_addr, (uint8_t*)&msg, sizeof(msg));
        printf("Sent discovery message\n");

        memset(msg.data, 0, sizeof(msg.data));
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    printf("connection with handler established! Waiting 5 seconds before sending heartbeat.\n");
    vTaskDelay(pdMS_TO_TICKS(1500));

    esp_now_peer_info_t handlerPeer;
    memset(&handlerPeer, 0, sizeof(handlerPeer));
    memcpy(handlerPeer.peer_addr, handlerMac, 6);
    handlerPeer.channel = 0;
    handlerPeer.encrypt = false;
    esp_now_add_peer(&handlerPeer);

    while(true) {
        Message msg = {0};
        msg.type = 2;

        if(!peer_upgraded) {
            esp_now_send(handlerPeer.peer_addr, (uint8_t*)&msg, sizeof(msg));
            printf("Sent unencrypted heartbeat\n");

            esp_now_del_peer(handlerPeer.peer_addr);

            esp_now_peer_info_t encHandlerPeer;
            memset(&encHandlerPeer, 0, sizeof(encHandlerPeer));
            memcpy(encHandlerPeer.peer_addr, handlerMac, 6);
            memcpy(encHandlerPeer.lmk, pmk, 16);
            encHandlerPeer.channel = 0;
            encHandlerPeer.encrypt = true;
            esp_now_add_peer(&encHandlerPeer);

            peer_upgraded = true;
        } else {
            esp_now_send(handlerMac, (uint8_t*)&msg, sizeof(msg));
            printf("Sent encrypted heartbeat\n");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
