idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash esp_wifi nvs_flash esp_netif esp_event
                       INCLUDE_DIRS "")
