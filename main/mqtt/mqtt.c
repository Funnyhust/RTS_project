#include "mqtt.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

// ==== TLS CA (HiveMQ Cloud) ====
extern const uint8_t hivemq_ca_pem_start[] asm("_binary_hivemq_ca_pem_start");
extern const uint8_t hivemq_ca_pem_end[]   asm("_binary_hivemq_ca_pem_end");

static const char *TAG = "MQTT";

// Topics mặc định
#define TOPIC_SENSOR_DATA "fire_system/sensor/data"
#define TOPIC_ALERT       "fire_system/alert"
#define TOPIC_STATUS      "fire_system/status"
#define TOPIC_CONTROL     "fire_system/control"

// ===============================
// MQTT EVENT HANDLER
// ===============================
void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                        int32_t event_id, void *event_data)
{
    mqtt_config_t *config = (mqtt_config_t *)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        config->is_connected = true;
        esp_mqtt_client_subscribe(event->client, TOPIC_CONTROL, MQTT_QOS_1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT Disconnected");
        config->is_connected = false;
        break;

    case MQTT_EVENT_DATA: {
        ESP_LOGI(TAG, "MQTT data received");

        if (config->message_queue) {
            mqtt_message_t msg;
            memset(&msg, 0, sizeof(mqtt_message_t));

            int tlen = event->topic_len;
            if (tlen >= MQTT_TOPIC_MAX_LEN) tlen = MQTT_TOPIC_MAX_LEN - 1;
            memcpy(msg.topic, event->topic, tlen);
            msg.topic[tlen] = '\0';

            int dlen = event->data_len;
            if (dlen >= MQTT_PAYLOAD_MAX_LEN) dlen = MQTT_PAYLOAD_MAX_LEN - 1;
            memcpy(msg.payload, event->data, dlen);
            msg.payload[dlen] = '\0';

            msg.qos = event->qos;
            msg.retain = 0;

            xQueueSend(config->message_queue, &msg, 0);
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

// ===============================
// MQTT INIT
// ===============================
int mqtt_init(mqtt_config_t *config,
              const char *uri,
              const char *username,
              const char *password,
              const char *client_id,
              bool use_tls)
{
    if (!config || !uri || !client_id) return -1;

    memset(config, 0, sizeof(mqtt_config_t));

    strncpy(config->uri, uri, sizeof(config->uri) - 1);
    strncpy(config->client_id, client_id, sizeof(config->client_id) - 1);
    config->use_tls = use_tls;

    if (username) strncpy(config->username, username, sizeof(config->username) - 1);
    if (password) strncpy(config->password, password, sizeof(config->password) - 1);

    config->message_queue = xQueueCreate(10, sizeof(mqtt_message_t));
    if (!config->message_queue) return -1;

    esp_mqtt_client_config_t mqtt_cfg = {0};

    // Cấu hình URI chuẩn ESP-IDF
    mqtt_cfg.broker.address.uri = config->uri;

    mqtt_cfg.credentials.client_id = config->client_id;
    mqtt_cfg.credentials.username  = config->username;
    mqtt_cfg.credentials.authentication.password = config->password;

    // TLS nếu bật
    if (use_tls) {
        mqtt_cfg.broker.verification.certificate = (const char *)hivemq_ca_pem_start;
    }

    config->client = esp_mqtt_client_init(&mqtt_cfg);
    if (!config->client) return -1;

    esp_mqtt_client_register_event(config->client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, config);

    ESP_LOGI(TAG, "MQTT initialized: %s", config->uri);
    return 0;
}

// ===============================
// CONNECT
// ===============================
int mqtt_connect(mqtt_config_t *config)
{
    if (!config || !config->client) return -1;
    ESP_LOGI(TAG, "MQTT starting...");
    return (esp_mqtt_client_start(config->client) == ESP_OK) ? 0 : -1;
}

// ===============================
int mqtt_disconnect(mqtt_config_t *config)
{
    if (!config || !config->client) return -1;
    esp_mqtt_client_stop(config->client);
    config->is_connected = false;
    return 0;
}

bool mqtt_is_connected(mqtt_config_t *config)
{
    return (config != NULL) && (config->is_connected);
}

// ===============================
int mqtt_subscribe(mqtt_config_t *config, const char *topic, int qos)
{
    if (!config || !topic || !config->is_connected) return -1;
    return (esp_mqtt_client_subscribe(config->client, topic, qos) >= 0) ? 0 : -1;
}

int mqtt_unsubscribe(mqtt_config_t *config, const char *topic)
{
    if (!config || !topic || !config->is_connected) return -1;
    return (esp_mqtt_client_unsubscribe(config->client, topic) >= 0) ? 0 : -1;
}

// ===============================
int mqtt_publish(mqtt_config_t *config, const char *topic,
                 const char *payload, int qos, int retain)
{
    if (!config || !topic || !payload || !config->is_connected) return -1;

    int id = esp_mqtt_client_publish(config->client, topic, payload,
                                     strlen(payload), qos, retain);
    return (id >= 0) ? id : -1;
}

// ===============================
int mqtt_publish_sensor_data(mqtt_config_t *config, const char *sensor_data)
{
    return mqtt_publish(config, TOPIC_SENSOR_DATA, sensor_data, MQTT_QOS_1, 0);
}

int mqtt_publish_alert(mqtt_config_t *config, const char *alert_data)
{
    return mqtt_publish(config, TOPIC_ALERT, alert_data, MQTT_QOS_2, 1);
}

// ===============================
bool mqtt_receive_message(mqtt_config_t *config,
                          mqtt_message_t *message,
                          uint32_t timeout_ms)
{
    if (!config || !message || !config->message_queue) return false;

    TickType_t t = timeout_ms ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;
    return xQueueReceive(config->message_queue, message, t) == pdTRUE;
}

// ===============================
void mqtt_task(void *pvParameters)
{
    mqtt_config_t *config = (mqtt_config_t *)pvParameters;
    if (!config) vTaskDelete(NULL);

    while (1) {
        if (config->is_connected) {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "status", "online");
            cJSON_AddNumberToObject(root, "uptime", esp_log_timestamp());

            char *buf = cJSON_Print(root);
            if (buf) {
                mqtt_publish(config, TOPIC_STATUS, buf, MQTT_QOS_0, 0);
                free(buf);
            }
            cJSON_Delete(root);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
