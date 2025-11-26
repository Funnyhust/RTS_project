#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

#include "sensor/sensor.h"
#include "buzzer/buzzer.h"
#include "wifi/wifi.h"
#include "mqtt/mqtt.h"

static const char *TAG = "MAIN";

// Cấu hình hệ thống
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define MQTT_BROKER_URI "mqtt://broker.example.com"  // Hoặc mqtts:// cho TLS
#define MQTT_USERNAME NULL  // Hoặc "username" nếu cần
#define MQTT_PASSWORD NULL  // Hoặc "password" nếu cần
#define MQTT_CLIENT_ID "fire_system_esp32"
#define MQTT_USE_TLS false

#define BUZZER_GPIO_PIN GPIO_NUM_25  // Thay đổi theo GPIO bạn sử dụng

// Biến toàn cục
static sensor_status_t g_sensor_status;
static buzzer_t g_buzzer;
static wifi_manager_t g_wifi_manager;
static mqtt_config_t g_mqtt_config;

/**
 * @brief Task cảnh báo - xử lý khi phát hiện cháy
 */
void warning_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Warning task started");
    
    bool last_fire_state = false;
    
    while (1) {
        // Kiểm tra trạng thái cháy
        if (g_sensor_status.fire_detected) {
            if (!last_fire_state) {
                // Cháy mới được phát hiện
                ESP_LOGW(TAG, "FIRE DETECTED! Activating alarm...");
                
                // Kích hoạt buzzer ở chế độ báo động
                buzzer_set_mode(&g_buzzer, BUZZER_ALARM);
                
                // Gửi cảnh báo qua MQTT
                if (mqtt_is_connected(&g_mqtt_config)) {
                    cJSON *alert = cJSON_CreateObject();
                    cJSON_AddStringToObject(alert, "type", "fire_alert");
                    cJSON_AddBoolToObject(alert, "detected", true);
                    cJSON_AddNumberToObject(alert, "timestamp", 
                                          (double)g_sensor_status.detection_timestamp);
                    cJSON_AddNumberToObject(alert, "smoke", g_sensor_status.smoke.normalized_value);
                    cJSON_AddNumberToObject(alert, "temperature", g_sensor_status.temperature.normalized_value);
                    cJSON_AddBoolToObject(alert, "ir_flame", g_sensor_status.ir_flame.is_triggered);
                    cJSON_AddNumberToObject(alert, "gas", g_sensor_status.gas.normalized_value);
                    
                    char *alert_json = cJSON_Print(alert);
                    if (alert_json != NULL) {
                        mqtt_publish_alert(&g_mqtt_config, alert_json);
                        free(alert_json);
                    }
                    cJSON_Delete(alert);
                }
            }
            last_fire_state = true;
        } else {
            if (last_fire_state) {
                // Cháy đã được dập tắt
                ESP_LOGI(TAG, "Fire extinguished. Deactivating alarm...");
                buzzer_set_mode(&g_buzzer, BUZZER_OFF);
                last_fire_state = false;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Kiểm tra mỗi 100ms
    }
}

/**
 * @brief Task gửi dữ liệu cảm biến lên MQTT
 */
void mqtt_sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "MQTT sensor task started");
    
    const TickType_t delay = pdMS_TO_TICKS(5000); // Gửi mỗi 5 giây
    
    while (1) {
        if (mqtt_is_connected(&g_mqtt_config)) {
            // Tạo JSON chứa dữ liệu cảm biến
            cJSON *json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "timestamp", 
                                  (double)(xTaskGetTickCount() * portTICK_PERIOD_MS));
            cJSON_AddNumberToObject(json, "smoke", g_sensor_status.smoke.normalized_value);
            cJSON_AddNumberToObject(json, "temperature", g_sensor_status.temperature.normalized_value);
            cJSON_AddBoolToObject(json, "ir_flame", g_sensor_status.ir_flame.is_triggered);
            cJSON_AddNumberToObject(json, "gas", g_sensor_status.gas.normalized_value);
            cJSON_AddBoolToObject(json, "fire_detected", g_sensor_status.fire_detected);
            
            char *json_string = cJSON_Print(json);
            if (json_string != NULL) {
                mqtt_publish_sensor_data(&g_mqtt_config, json_string);
                free(json_string);
            }
            cJSON_Delete(json);
        } else {
            ESP_LOGW(TAG, "MQTT not connected, skipping sensor data publish");
        }
        
        vTaskDelay(delay);
    }
}

/**
 * @brief Task xử lý message MQTT nhận được
 */
void mqtt_control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "MQTT control task started");
    
    mqtt_message_t message;
    
    while (1) {
        if (mqtt_receive_message(&g_mqtt_config, &message, 1000)) {
            ESP_LOGI(TAG, "Received MQTT message - Topic: %s, Payload: %s", 
                     message.topic, message.payload);
            
            // Xử lý message điều khiển
            if (strstr(message.topic, "control") != NULL) {
                cJSON *json = cJSON_Parse(message.payload);
                if (json != NULL) {
                    cJSON *cmd = cJSON_GetObjectItem(json, "command");
                    if (cmd != NULL && cJSON_IsString(cmd)) {
                        const char *command = cJSON_GetStringValue(cmd);
                        
                        if (strcmp(command, "buzzer_on") == 0) {
                            buzzer_set_mode(&g_buzzer, BUZZER_NORMAL);
                            ESP_LOGI(TAG, "Buzzer turned on via MQTT");
                        } else if (strcmp(command, "buzzer_off") == 0) {
                            buzzer_set_mode(&g_buzzer, BUZZER_OFF);
                            ESP_LOGI(TAG, "Buzzer turned off via MQTT");
                        } else if (strcmp(command, "test_alarm") == 0) {
                            buzzer_set_mode(&g_buzzer, BUZZER_ALARM);
                            vTaskDelay(pdMS_TO_TICKS(3000));
                            buzzer_set_mode(&g_buzzer, BUZZER_OFF);
                            ESP_LOGI(TAG, "Test alarm executed via MQTT");
                        }
                    }
                    cJSON_Delete(json);
                }
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Hệ thống báo cháy ESP32 khởi động ===");
    
    // 1. Khởi tạo cảm biến
    ESP_LOGI(TAG, "Initializing sensors...");
    if (sensor_system_init(&g_sensor_status) != 0) {
        ESP_LOGE(TAG, "Failed to initialize sensors");
        return;
    }
    ESP_LOGI(TAG, "Sensors initialized successfully");
    
    // 2. Khởi tạo buzzer
    ESP_LOGI(TAG, "Initializing buzzer...");
    if (buzzer_init(&g_buzzer, BUZZER_GPIO_PIN) != 0) {
        ESP_LOGE(TAG, "Failed to initialize buzzer");
        return;
    }
    buzzer_set_mode(&g_buzzer, BUZZER_OFF);
    ESP_LOGI(TAG, "Buzzer initialized successfully");
    
    // 3. Khởi tạo và kết nối WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    if (wifi_init(&g_wifi_manager, WIFI_SSID, WIFI_PASSWORD) != 0) {
        ESP_LOGE(TAG, "Failed to initialize WiFi");
        return;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", WIFI_SSID);
    if (wifi_connect(&g_wifi_manager) != 0) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return;
    }
    ESP_LOGI(TAG, "WiFi connected successfully");
    
    // Hiển thị địa chỉ IP
    char ip_str[16];
    if (wifi_get_ip_address(ip_str) == 0) {
        ESP_LOGI(TAG, "IP Address: %s", ip_str);
    }
    
    // 4. Khởi tạo và kết nối MQTT
    ESP_LOGI(TAG, "Initializing MQTT...");
    if (mqtt_init(&g_mqtt_config, MQTT_BROKER_URI, MQTT_USERNAME, 
                  MQTT_PASSWORD, MQTT_CLIENT_ID, MQTT_USE_TLS) != 0) {
        ESP_LOGE(TAG, "Failed to initialize MQTT");
        return;
    }
    
    ESP_LOGI(TAG, "Connecting to MQTT broker...");
    if (mqtt_connect(&g_mqtt_config) != 0) {
        ESP_LOGE(TAG, "Failed to connect to MQTT broker");
        return;
    }
    
    // Đợi một chút để MQTT kết nối
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    if (mqtt_is_connected(&g_mqtt_config)) {
        ESP_LOGI(TAG, "MQTT connected successfully");
    } else {
        ESP_LOGW(TAG, "MQTT connection pending...");
    }
    
    // 5. Tạo các FreeRTOS tasks
    ESP_LOGI(TAG, "Creating FreeRTOS tasks...");
    
    // Task đọc cảm biến (ưu tiên cao, chu kỳ 500ms)
    xTaskCreate(sensor_task, "sensor_task", 4096, &g_sensor_status, 
                configMAX_PRIORITIES - 1, NULL);
    
    // Task điều khiển buzzer (ưu tiên cao)
    xTaskCreate(buzzer_task, "buzzer_task", 2048, &g_buzzer, 
                configMAX_PRIORITIES - 2, NULL);
    
    // Task cảnh báo (ưu tiên cao, xử lý khi phát hiện cháy)
    xTaskCreate(warning_task, "warning_task", 4096, NULL, 
                configMAX_PRIORITIES - 1, NULL);
    
    // Task gửi dữ liệu cảm biến lên MQTT (ưu tiên trung bình, chu kỳ 5s)
    xTaskCreate(mqtt_sensor_task, "mqtt_sensor_task", 4096, NULL, 
                configMAX_PRIORITIES - 3, NULL);
    
    // Task xử lý message MQTT (ưu tiên trung bình)
    xTaskCreate(mqtt_control_task, "mqtt_control_task", 4096, NULL, 
                configMAX_PRIORITIES - 3, NULL);
    
    // Task MQTT (ưu tiên thấp, chu kỳ 30s)
    xTaskCreate(mqtt_task, "mqtt_task", 4096, &g_mqtt_config, 
                configMAX_PRIORITIES - 4, NULL);
    
    ESP_LOGI(TAG, "=== Hệ thống đã sẵn sàng ===");
    ESP_LOGI(TAG, "All tasks started. System is running...");
    
    // Main task có thể làm việc khác hoặc đợi
    while (1) {
        // Hiển thị trạng thái hệ thống định kỳ
        ESP_LOGI(TAG, "System Status - WiFi: %s, MQTT: %s, Fire: %s",
                 wifi_is_connected(&g_wifi_manager) ? "Connected" : "Disconnected",
                 mqtt_is_connected(&g_mqtt_config) ? "Connected" : "Disconnected",
                 g_sensor_status.fire_detected ? "DETECTED" : "Normal");
        
        vTaskDelay(pdMS_TO_TICKS(30000)); // Log mỗi 30 giây
    }
}
