#include "mqtt.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "MQTT";

// Topics mặc định
#define TOPIC_SENSOR_DATA "fire_system/sensor/data"
#define TOPIC_ALERT "fire_system/alert"
#define TOPIC_STATUS "fire_system/status"
#define TOPIC_CONTROL "fire_system/control"

void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                       int32_t event_id, void *event_data)
{
    mqtt_config_t *config = (mqtt_config_t *)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        config->is_connected = true;
        
        // Subscribe topic điều khiển
        esp_mqtt_client_subscribe(client, TOPIC_CONTROL, MQTT_QOS_1);
        ESP_LOGI(TAG, "Subscribed to topic: %s", TOPIC_CONTROL);
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT Disconnected");
        config->is_connected = false;
        break;
        
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT unsubscribed, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT published, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT message received");
        ESP_LOGI(TAG, "Topic: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "Data: %.*s", event->data_len, event->data);
        
        // Thêm message vào queue
        if (config->message_queue != NULL) {
            mqtt_message_t msg;
            memset(&msg, 0, sizeof(msg));
            
            int topic_len = event->topic_len < (MQTT_TOPIC_MAX_LEN - 1) ? 
                           event->topic_len : (MQTT_TOPIC_MAX_LEN - 1);
            int data_len = event->data_len < (MQTT_PAYLOAD_MAX_LEN - 1) ? 
                          event->data_len : (MQTT_PAYLOAD_MAX_LEN - 1);
            
            strncpy(msg.topic, event->topic, topic_len);
            msg.topic[topic_len] = '\0';
            
            strncpy(msg.payload, event->data, data_len);
            msg.payload[data_len] = '\0';
            
            msg.qos = event->qos;
            msg.retain = 0;
            
            if (xQueueSend(config->message_queue, &msg, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Message queue full, dropping message");
            }
        }
        break;
        
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Transport error: %s", 
                     esp_err_to_name(event->error_handle->esp_transport_sock_errno));
        }
        break;
        
    default:
        ESP_LOGI(TAG, "Other MQTT event id:%d", event->event_id);
        break;
    }
}

int mqtt_init(mqtt_config_t *config, const char *uri, const char *username,
              const char *password, const char *client_id, bool use_tls)
{
    if (config == NULL || uri == NULL || client_id == NULL) {
        return -1;
    }
    
    memset(config, 0, sizeof(mqtt_config_t));
    
    strncpy(config->uri, uri, sizeof(config->uri) - 1);
    if (username != NULL) {
        strncpy(config->username, username, sizeof(config->username) - 1);
    }
    if (password != NULL) {
        strncpy(config->password, password, sizeof(config->password) - 1);
    }
    strncpy(config->client_id, client_id, sizeof(config->client_id) - 1);
    config->use_tls = use_tls;
    config->is_connected = false;
    
    // Tạo message queue
    config->message_queue = xQueueCreate(10, sizeof(mqtt_message_t));
    if (config->message_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create message queue");
        return -1;
    }
    
    // Cấu hình MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {0};
    
    // Parse URI để lấy hostname và port
    // Format: mqtt://hostname:port hoặc mqtts://hostname:port
    char *uri_copy = strdup(config->uri);
    char *protocol = strtok(uri_copy, "://");
    char *host_port = strtok(NULL, "://");
    
    if (host_port == NULL) {
        // Nếu không có protocol, sử dụng trực tiếp
        host_port = strdup(config->uri);
        free(uri_copy);
        uri_copy = host_port;
    }
    
    // Tách hostname và port
    char *hostname = host_port;
    char *port_str = strchr(host_port, ':');
    int port = use_tls ? 8883 : 1883;
    
    if (port_str != NULL) {
        *port_str = '\0';
        port_str++;
        port = atoi(port_str);
    }
    
    // Xác định transport từ protocol hoặc use_tls
    esp_mqtt_transport_t transport = MQTT_TRANSPORT_OVER_TCP;
    if (use_tls || (protocol != NULL && strstr(protocol, "mqtts") != NULL)) {
        transport = MQTT_TRANSPORT_OVER_SSL;
    }
    
    // Cấu hình broker
    mqtt_cfg.broker.address.hostname = hostname;
    mqtt_cfg.broker.address.port = port;
    mqtt_cfg.broker.address.transport = transport;
    
    // Cấu hình client ID
    mqtt_cfg.credentials.client_id = config->client_id;
    
    // Cấu hình authentication
    if (strlen(config->username) > 0) {
        mqtt_cfg.credentials.username = config->username;
    }
    if (strlen(config->password) > 0) {
        mqtt_cfg.credentials.authentication.password = config->password;
    }
    
    config->client = esp_mqtt_client_init(&mqtt_cfg);
    
    free(uri_copy);
    if (config->client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return -1;
    }
    
    // Đăng ký event handler
    esp_mqtt_client_register_event(config->client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, config);
    
    ESP_LOGI(TAG, "MQTT client initialized");
    ESP_LOGI(TAG, "URI: %s", uri);
    ESP_LOGI(TAG, "Client ID: %s", client_id);
    ESP_LOGI(TAG, "TLS: %s", use_tls ? "enabled" : "disabled");
    
    return 0;
}

int mqtt_connect(mqtt_config_t *config)
{
    if (config == NULL || config->client == NULL) {
        return -1;
    }
    
    ESP_LOGI(TAG, "Connecting to MQTT broker...");
    
    esp_err_t err = esp_mqtt_client_start(config->client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return -1;
    }
    
    return 0;
}

int mqtt_disconnect(mqtt_config_t *config)
{
    if (config == NULL || config->client == NULL) {
        return -1;
    }
    
    ESP_LOGI(TAG, "Disconnecting from MQTT broker...");
    
    esp_mqtt_client_stop(config->client);
    config->is_connected = false;
    
    return 0;
}

bool mqtt_is_connected(mqtt_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    
    return config->is_connected;
}

int mqtt_subscribe(mqtt_config_t *config, const char *topic, int qos)
{
    if (config == NULL || config->client == NULL || topic == NULL) {
        return -1;
    }
    
    if (!config->is_connected) {
        ESP_LOGW(TAG, "MQTT not connected, cannot subscribe");
        return -1;
    }
    
    int msg_id = esp_mqtt_client_subscribe(config->client, topic, qos);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
        return -1;
    }
    
    ESP_LOGI(TAG, "Subscribed to topic: %s (QoS: %d)", topic, qos);
    
    return 0;
}

int mqtt_unsubscribe(mqtt_config_t *config, const char *topic)
{
    if (config == NULL || config->client == NULL || topic == NULL) {
        return -1;
    }
    
    if (!config->is_connected) {
        ESP_LOGW(TAG, "MQTT not connected, cannot unsubscribe");
        return -1;
    }
    
    int msg_id = esp_mqtt_client_unsubscribe(config->client, topic);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to unsubscribe from topic: %s", topic);
        return -1;
    }
    
    ESP_LOGI(TAG, "Unsubscribed from topic: %s", topic);
    
    return 0;
}

int mqtt_publish(mqtt_config_t *config, const char *topic, const char *payload,
                 int qos, int retain)
{
    if (config == NULL || config->client == NULL || topic == NULL || payload == NULL) {
        return -1;
    }
    
    if (!config->is_connected) {
        ESP_LOGW(TAG, "MQTT not connected, cannot publish");
        return -1;
    }
    
    int msg_id = esp_mqtt_client_publish(config->client, topic, payload, 
                                        strlen(payload), qos, retain);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish to topic: %s", topic);
        return -1;
    }
    
    ESP_LOGD(TAG, "Published to topic: %s, msg_id: %d", topic, msg_id);
    
    return msg_id;
}

int mqtt_publish_sensor_data(mqtt_config_t *config, const char *sensor_data)
{
    return mqtt_publish(config, TOPIC_SENSOR_DATA, sensor_data, MQTT_QOS_1, 0);
}

int mqtt_publish_alert(mqtt_config_t *config, const char *alert_data)
{
    return mqtt_publish(config, TOPIC_ALERT, alert_data, MQTT_QOS_2, 1);
}

bool mqtt_receive_message(mqtt_config_t *config, mqtt_message_t *message, 
                         uint32_t timeout_ms)
{
    if (config == NULL || message == NULL || config->message_queue == NULL) {
        return false;
    }
    
    TickType_t timeout = timeout_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    
    if (xQueueReceive(config->message_queue, message, timeout) == pdTRUE) {
        return true;
    }
    
    return false;
}

void mqtt_task(void *pvParameters)
{
    mqtt_config_t *config = (mqtt_config_t *)pvParameters;
    
    if (config == NULL) {
        ESP_LOGE(TAG, "MQTT task: Invalid parameters");
        vTaskDelete(NULL);
        return;
    }
    
    const TickType_t delay = pdMS_TO_TICKS(5000); // 5 giây chu kỳ
    
    ESP_LOGI(TAG, "MQTT task started");
    
    while (1) {
        if (config->is_connected) {
            // Gửi status message định kỳ
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "status", "online");
            cJSON_AddNumberToObject(json, "timestamp", 
                                   (double)(xTaskGetTickCount() * portTICK_PERIOD_MS));
            
            char *json_string = cJSON_Print(json);
            if (json_string != NULL) {
                mqtt_publish(config, TOPIC_STATUS, json_string, MQTT_QOS_0, 0);
                free(json_string);
            }
            cJSON_Delete(json);
        }
        
        vTaskDelay(delay);
    }
}

