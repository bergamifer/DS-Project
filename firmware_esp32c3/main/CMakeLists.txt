idf_component_register(
    SRCS "main.c" "mic.c" "mic_dsp.c"
    INCLUDE_DIRS "."
    REQUIRES
        # Desde ESP-IDF 6.1 el componente generico "driver" esta partido
        # en uno por periferico: GPIO y UART.
        esp_driver_gpio
        esp_driver_uart
        esp_driver_i2s
        esp_timer
        freertos
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        nvs_flash
        espressif__cjson
        espressif__mdns
        # OTA de este mismo chip: esp_ota_* y la vuelta atras automatica viven
        # en app_update; esp_partition da la ranura que se esta ejecutando.
        app_update
        esp_partition
)
