#ifndef MQTT_H
#define MQTT_H

#include <stdbool.h>
#include <stdint.h>
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Cấu hình MQTT
#define MQTT_URI_MAX_LEN 128
#define MQTT_USERNAME_MAX_LEN 64
#define MQTT_PASSWORD_MAX_LEN 64
#define MQTT_CLIENT_ID_MAX_LEN 32
#define MQTT_TOPIC_MAX_LEN 128
#define MQTT_PAYLOAD_MAX_LEN 512

// QoS levels
#define MQTT_QOS_0 0
#define MQTT_QOS_1 1
#define MQTT_QOS_2 2

// Cấu trúc cấu hình MQTT
typedef struct {
    char uri[MQTT_URI_MAX_LEN];
    char username[MQTT_USERNAME_MAX_LEN];
    char password[MQTT_PASSWORD_MAX_LEN];
    char client_id[MQTT_CLIENT_ID_MAX_LEN];
    bool use_tls;
    bool is_connected;
    esp_mqtt_client_handle_t client;
    QueueHandle_t message_queue;
} mqtt_config_t;

// Cấu trúc message MQTT
typedef struct {
    char topic[MQTT_TOPIC_MAX_LEN];
    char payload[MQTT_PAYLOAD_MAX_LEN];
    int qos;
    int retain;
} mqtt_message_t;

/**
 * @brief Khởi tạo MQTT client
 * @param config Con trỏ đến cấu trúc cấu hình MQTT
 * @param uri URI của MQTT broker (ví dụ: mqtts://broker.example.com:8883)
 * @param username Tên người dùng (có thể NULL)
 * @param password Mật khẩu (có thể NULL)
 * @param client_id Client ID
 * @param use_tls Sử dụng TLS
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int mqtt_init(mqtt_config_t *config, const char *uri, const char *username,
              const char *password, const char *client_id, bool use_tls);

/**
 * @brief Kết nối MQTT broker
 * @param config Con trỏ đến cấu trúc cấu hình MQTT
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int mqtt_connect(mqtt_config_t *config);

/**
 * @brief Ngắt kết nối MQTT broker
 * @param config Con trỏ đến cấu trúc cấu hình MQTT
 * @return 0 nếu thành công
 */
int mqtt_disconnect(mqtt_config_t *config);

/**
 * @brief Kiểm tra trạng thái kết nối MQTT
 * @param config Con trỏ đến cấu trúc cấu hình MQTT
 * @return true nếu đã kết nối
 */
bool mqtt_is_connected(mqtt_config_t *config);

/**
 * @brief Đăng ký subscribe topic
 * @param config Con trỏ đến cấu trúc cấu hình MQTT
 * @param topic Topic cần subscribe
 * @param qos QoS level (0, 1, hoặc 2)
 * @return 0 nếu thành công
 */
int mqtt_subscribe(mqtt_config_t *config, const char *topic, int qos);

/**
 * @brief Hủy subscribe topic
 * @param config Con trỏ đến cấu trúc cấu hình MQTT
 * @param topic Topic cần unsubscribe
 * @return 0 nếu thành công
 */
int mqtt_unsubscribe(mqtt_config_t *config, const char *topic);

/**
 * @brief Gửi message lên MQTT broker
 * @param config Con trỏ đến cấu trúc cấu hình MQTT
 * @param topic Topic để publish
 * @param payload Nội dung message
 * @param qos QoS level (0, 1, hoặc 2)
 * @param retain Retain flag
 * @return Message ID nếu thành công, -1 nếu lỗi
 */
int mqtt_publish(mqtt_config_t *config, const char *topic, const char *payload,
                 int qos, int retain);

/**
 * @brief Gửi dữ liệu cảm biến lên MQTT
 * @param config Con trỏ đến cấu trúc cấu hình MQTT
 * @param sensor_data JSON string chứa dữ liệu cảm biến
 * @return 0 nếu thành công
 */
int mqtt_publish_sensor_data(mqtt_config_t *config, const char *sensor_data);

/**
 * @brief Gửi cảnh báo cháy lên MQTT
 * @param config Con trỏ đến cấu trúc cấu hình MQTT
 * @param alert_data JSON string chứa thông tin cảnh báo
 * @return 0 nếu thành công
 */
int mqtt_publish_alert(mqtt_config_t *config, const char *alert_data);

/**
 * @brief Nhận message từ queue
 * @param config Con trỏ đến cấu trúc cấu hình MQTT
 * @param message Con trỏ đến cấu trúc message
 * @param timeout_ms Timeout (ms)
 * @return true nếu nhận được message
 */
bool mqtt_receive_message(mqtt_config_t *config, mqtt_message_t *message, 
                         uint32_t timeout_ms);

/**
 * @brief Event handler cho MQTT
 * @param handler_args Tham số handler
 * @param base Base của event
 * @param event_id ID của event
 * @param event_data Dữ liệu event
 */
void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                       int32_t event_id, void *event_data);

/**
 * @brief Task FreeRTOS để gửi dữ liệu định kỳ lên MQTT
 * @param pvParameters Tham số task (mqtt_config_t*)
 */
void mqtt_task(void *pvParameters);

#endif // MQTT_H

