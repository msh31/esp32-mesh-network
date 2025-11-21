#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log_level.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h" // open-source real-0time OS on the ESP32
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

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
    bool is_encrypted;
};

struct Agent agents[2];
int agent_count = 0;

const char *discovery_secret = "TkFLRURfU05BS0U=";
uint8_t pmk[16] = {
    0xF3, 0x26, 0xA5, 0xC3, 0x9C, 0xC0, 0x8E, 0xC0,
    0x15, 0xAB, 0x90, 0x69, 0x8C, 0x7E, 0x6F, 0x8C
};

enum CommandType {
    CMD_LED_TOGGLE = 0,
    CMD_REBOOT = 1,
    CMD_SYSTEM_INFO = 2,
    CMD_WIFI_SCAN = 3
};

void monitor_task(void *pvParameters) {
     printf("Running monitoring task on core: %d\n", xPortGetCoreID());

    while(true) {
        for(int i = 0; i < agent_count; i++) {
            uint32_t elapsed_ticks = xTaskGetTickCount() - agents[i].last_seen;
            uint32_t elapsed_seconds = elapsed_ticks / configTICK_RATE_HZ;

            if(agents[i].last_seen == 0) {
                continue;
            }

            //allow 3 missed beats before declaring deadd
            if(elapsed_ticks > pdMS_TO_TICKS(15000) && agents[i].is_alive) {
                ESP_LOGI("MONITOR", "(%02X:%02X:%02X:%02X:%02X:%02X) has passed on, may they rest in peace..\n",
                    agents[i].mac[0], agents[i].mac[1], agents[i].mac[2],
                    agents[i].mac[3], agents[i].mac[4], agents[i].mac[5]
                );

                if(elapsed_seconds >= 3600) {
                    uint32_t hours = elapsed_seconds / 3600;
                    ESP_LOGW("MONITOR", "Agent died after %lu hours\n", hours);
                } else if(elapsed_seconds >= 60) {
                    uint32_t minutes = elapsed_seconds / 60;
                    ESP_LOGW("MONITOR", "Agent died after %lu minutes\n", minutes);
                } else {
                    ESP_LOGW("MONITOR", "Agent died after %lu seconds\n", elapsed_seconds);
                }

                //ld = long int
                // printf("They were last seen at: %ld", agents[i].last_seen);
                agents[i].is_alive = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void cli_task(void *pvParameters) {
    ESP_LOGI("CLI", "Running CLI task on core: %d\n", xPortGetCoreID());
    ESP_LOGI("CLI", "CLI ready. Type commands:\n");

    char input_buffer[64];
    int pos = 0;

    while(true) {
        int c = getchar();

        if(c != EOF) {
            if(c == '\n' || c == '\r') {  // enter pressed
                input_buffer[pos] = '\0';  // null terminate

                char command[32];
                int agent_id = -1;
                sscanf(input_buffer, "%s", command);

                Message cmd_msg;
                cmd_msg.type = 3;

                if(strcmp(command, "reboot") == 0 || strcmp(command, "led") == 0) {
                    int parsed = sscanf(input_buffer, "%s %d", command, &agent_id);

                    if(parsed != 2) {
                        ESP_LOGI("CLI", "Usage: %s <agent_id>\n", command);
                        pos = 0;
                        continue;
                    }

                    if(agent_id >= agent_count) {
                        ESP_LOGE("CLI", "agent %d does not exist\n", agent_id);
                        continue; //skip to next interation
                    }

                    if(!agents[agent_id].is_alive) {
                        ESP_LOGE("CLI", "Agent %d is dead..\n", agent_id);
                        continue;
                    }

                    if(agents[agent_id].is_encrypted) {
                        cmd_msg.data[0] = CMD_REBOOT;
                        esp_now_send(agents[agent_id].mac, (uint8_t*)&cmd_msg, sizeof(cmd_msg));

                        agents[agent_id].is_alive = false;
                        agents[agent_id].is_encrypted = false;
                        esp_now_del_peer(agents[agent_id].mac); //insurnce
                    } else {
                        ESP_LOGE("CLI", "agent %d does not have an encrypted connection...\n", agent_id);
                        continue;
                    }
                } else if(strcmp(command, "list") == 0) {
                    if (agent_count == 0) {
                        ESP_LOGW("CLI", "\nNo agents found!\n");
                    } else {
                        ESP_LOGI("CLI", "\nConnected agents:\n");
                        for(int i = 0; i < agent_count; i++) {
                            ESP_LOGI("CLI", "Agent %d: %02X:%02X:%02X:%02X:%02X:%02X - %s\n",
                                i,
                                agents[i].mac[0], agents[i].mac[1], agents[i].mac[2],
                                agents[i].mac[3], agents[i].mac[4], agents[i].mac[5],
                                agents[i].is_alive ? "ALIVE" : "DEAD"
                            );
                        }
                    }
                } else if (strcmp(command, "help") == 0) {
                    ESP_LOGI("CLI", "\n\n==LIST OF COMMANDS==\n\n");
                    ESP_LOGI("CLI", "1. List - lists the conneccted agents\n");
                    ESP_LOGI("CLI", "2. reboot {1/2} - reboot an agent with the agent ID as a paramater\n");
                    ESP_LOGI("CLI", "3. led {1/2} - toggle the built-in LED of an agent with the agent ID as a param.\n\n");
                } else {
                    ESP_LOGW("CLI", "Unknown command\n");
                }

                pos = 0;  // reset for next command
            } else if(pos < 63) {
                input_buffer[pos++] = c;
                printf("%c", c);
                fflush(stdout);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool add_agent(const uint8_t *mac) {
    for(int i = 0; i < agent_count; i++) {
        if(memcmp(agents[i].mac, mac, 6) == 0) {
            if(!agents[i].is_alive) {
                agents[i].is_alive = true;
                agents[i].last_seen = 0;
                agents[i].is_encrypted = false;
                ESP_LOGW("MONITOR", "Agent (%02X:%02X:%02X:%02X:%02X:%02X) has been revived!\n",
                    agents[i].mac[0], agents[i].mac[1], agents[i].mac[2],
                    agents[i].mac[3], agents[i].mac[4], agents[i].mac[5]
                );
                return true;
            } else {
                ESP_LOGI("MONITOR", "This agent already exists and is alive\n");
                return false;
            }
        }
    }

    if(agent_count >= 2) {
        ESP_LOGI("MONITOR", "Max number of agents reached!\n");
        return false;
    }

    memcpy(agents[agent_count].mac, mac, 6);
    agents[agent_count].is_encrypted = false;
    agents[agent_count].is_alive = true;
    agents[agent_count].last_seen = xTaskGetTickCount();

    ESP_LOGW("MONITOR", "New agent added! Mac: %02X:%02X:%02X:%02X:%02X:%02X\n",
        agents[agent_count].mac[0], agents[agent_count].mac[1],
        agents[agent_count].mac[2], agents[agent_count].mac[3],
        agents[agent_count].mac[4], agents[agent_count].mac[5]
    );

    agents[agent_count].last_seen = 0;
    agent_count++;
    return true;
}

void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const Message *msg = (const Message *)data;

    if(len != sizeof(Message)) {
        //printf("Received invalid message.");
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
            esp_now_del_peer(info->src_addr);

            esp_now_peer_info_t peer;
            memset(&peer, 0, sizeof(peer));
            memcpy(peer.peer_addr, info->src_addr, 6);
            peer.channel = 0;
            peer.encrypt = false;
            esp_now_add_peer(&peer);

            Message ack_msg;
            ack_msg.type = 1;
            memcpy(ack_msg.data, discovery_secret, strlen(discovery_secret));

            esp_now_send(info->src_addr, (uint8_t*)&ack_msg, sizeof(ack_msg));
            ESP_LOGW("MONITOR", "Sent acknowledgment\n");
        }
    }

    // handlers should NOT bbe able to RECEIVE a type1 message..
    if(msg->type == 1) {
        // printf("What the flip? A type1 message was *received*.... source: %02X:%02X:%02X:%02X:%02X:%02X\n",
        //     info->src_addr[0], info->src_addr[1], info->src_addr[2],
        //     info->src_addr[3], info->src_addr[4], info->src_addr[5]
        // );
        return;
    }

    if(msg->type == 2) {
        bool found = false;

        if(agent_count == 0) {
            ESP_LOGE("MONITOR", "A type2 message was received, but the agent count is at: %d\n", agent_count);
            return;
        }

        for(int i = 0; i < agent_count; i++) {
            if(memcmp(agents[i].mac, info->src_addr, 6) == 0) {
                if(!agents[i].is_encrypted) {
                    esp_now_del_peer(agents[i].mac);

                    esp_now_peer_info_t agentPeer;
                    memset(&agentPeer, 0, sizeof(agentPeer));
                    memcpy(agentPeer.peer_addr, info->src_addr, 6);
                    memcpy(agentPeer.lmk, pmk, 16);

                    agentPeer.channel = 0;
                    agentPeer.encrypt = true;
                    agents[i].is_encrypted = true;

                    esp_now_add_peer(&agentPeer);
                }

                agents[i].last_seen = xTaskGetTickCount();
                agents[i].is_alive = true;
                found = true;
                // printf("Agent (%02X:%02X:%02X:%02X:%02X:%02X) is alive!\n",
                //     info->src_addr[0], info->src_addr[1], info->src_addr[2],
                //     info->src_addr[3], info->src_addr[4], info->src_addr[5]
                // );
                break;
            }
        }

        if(!found) {
            ESP_LOGW("MONITOR", "Unknown agent sent heartbeat, source address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                info->src_addr[0], info->src_addr[1], info->src_addr[2],
                info->src_addr[3], info->src_addr[4], info->src_addr[5]
            );
        }
        return;
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

    esp_log_level_set("MONITOR", ESP_LOG_WARN);
    esp_log_level_set("CLI", ESP_LOG_INFO);

    xTaskCreatePinnedToCore(monitor_task, "MonitorTask", 2048, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(cli_task, "CommandLineTask", 2048, NULL, 5, NULL, 1);
}
