idf_component_register(SRCS "main.c"
                            "sensor/sensor.c"
                            "buzzer/buzzer.c"
                            "wifi/wifi.c"
                            "mqtt/mqtt.c"
                    INCLUDE_DIRS "."
                                 "sensor"
                                 "buzzer"
                                 "wifi"
                                 "mqtt"

                    PRIV_REQUIRES driver esp_wifi esp_netif nvs_flash mqtt freertos esp_adc json)
                    target_add_binary_data(${COMPONENT_LIB} "hivemq_ca.pem" TEXT)