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
        espressif__mdns
        # OTA de este mismo chip: esp_ota_* y la vuelta atras automatica viven
        # en app_update; esp_partition da la ranura que se esta ejecutando.
        app_update
        esp_partition
)
