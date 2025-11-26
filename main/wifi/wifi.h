#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Event bits cho WiFi
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Cấu trúc quản lý WiFi
typedef struct {
    char ssid[32];
    char password[64];
    bool is_connected;
    int retry_count;
    int max_retry;
    EventGroupHandle_t event_group;
} wifi_manager_t;

/**
 * @brief Khởi tạo WiFi
 * @param config Con trỏ đến cấu trúc cấu hình WiFi
 * @param ssid Tên mạng WiFi
 * @param password Mật khẩu WiFi
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int wifi_init(wifi_manager_t *manager, const char *ssid, const char *password);

/**
 * @brief Kết nối WiFi
 * @param manager Con trỏ đến cấu trúc quản lý WiFi
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int wifi_connect(wifi_manager_t *manager);

/**
 * @brief Ngắt kết nối WiFi
 * @param manager Con trỏ đến cấu trúc quản lý WiFi
 * @return 0 nếu thành công
 */
int wifi_disconnect(wifi_manager_t *manager);

/**
 * @brief Kiểm tra trạng thái kết nối WiFi
 * @param manager Con trỏ đến cấu trúc quản lý WiFi
 * @return true nếu đã kết nối
 */
bool wifi_is_connected(wifi_manager_t *manager);

/**
 * @brief Lấy địa chỉ IP của thiết bị
 * @param ip_str Buffer để lưu địa chỉ IP (tối thiểu 16 bytes)
 * @return 0 nếu thành công
 */
int wifi_get_ip_address(char *ip_str);

/**
 * @brief Event handler cho WiFi
 * @param arg Tham số event handler
 * @param event_base Base của event
 * @param event_id ID của event
 * @param event_data Dữ liệu event
 */
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                       int32_t event_id, void* event_data);

#endif // WIFI_H

