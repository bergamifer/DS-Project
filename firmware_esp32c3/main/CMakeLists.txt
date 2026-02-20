idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        freertos
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        nvs_flash
        json
)
