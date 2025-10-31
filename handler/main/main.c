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

struct Agent {
    uint8_t mac[6];
    uint32_t last_seen;
    bool is_alive;
};

struct Agent agents[2];
int agent_count = 0;
const char *discovery_secret = "TkFLRURfU05BS0U=";

bool add_agent(const uint8_t *mac) {
    if(agent_count >= 2) {
        printf("Max number of agents has been reached!\n");
        return false;
    }

    for(int i = 0; i < agent_count; i++) {
        if(memcmp(agents[i].mac, mac, 6) == 0) {
            printf("This agent's mac address already exists!\n");
            return false;
        }
    }
    
    memcpy(agents[agent_count].mac, mac, 6);
    agent_count += 1;
    return true;
}

void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const Message *msg = (const Message *)data;

    if(len != sizeof(Message)) {
        printf("Received invalid message.");
        return;
    }

    if(msg->type == 0) {
        // printf("Received data: %.*s (length: %d)\n", (int)strlen(discovery_secret), msg->data, (int)strlen(discovery_secret));
        // printf("Expected secret: %s (length: %zu)\n", discovery_secret, strlen(discovery_secret));

        if(memcmp(msg->data, discovery_secret, strlen(discovery_secret)) != 0) {
            printf("Invalid secret, rejecting agent\n");
            return;
        }

        if(add_agent(info->src_addr)) {
            printf("New agent added! Mac Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
    info->src_addr[0], info->src_addr[1], info->src_addr[2],
    info->src_addr[3], info->src_addr[4], info->src_addr[5]
            );
            // TODO: send acknowledgment
        }
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

    esp_now_register_recv_cb(on_data_recv);

    printf("Wifi Initialized!\n");
}
