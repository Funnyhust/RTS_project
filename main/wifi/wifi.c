#include "wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "WIFI";

static wifi_manager_t *g_wifi_manager = NULL;

void wifi_event_handler(void* arg, esp_event_base_t event_base,
                       int32_t event_id, void* event_data)
{
    if (g_wifi_manager == NULL) {
        return;
    }
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi station started, connecting...");
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (g_wifi_manager->retry_count < g_wifi_manager->max_retry) {
            esp_wifi_connect();
            g_wifi_manager->retry_count++;
            ESP_LOGI(TAG, "Retry to connect to WiFi (%d/%d)", 
                     g_wifi_manager->retry_count, g_wifi_manager->max_retry);
        } else {
            xEventGroupSetBits(g_wifi_manager->event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect to WiFi after %d retries", 
                     g_wifi_manager->max_retry);
        }
        g_wifi_manager->is_connected = false;
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        g_wifi_manager->retry_count = 0;
        g_wifi_manager->is_connected = true;
        xEventGroupSetBits(g_wifi_manager->event_group, WIFI_CONNECTED_BIT);
    }
}

int wifi_init(wifi_manager_t *manager, const char *ssid, const char *password)
{
    if (manager == NULL || ssid == NULL || password == NULL) {
        return -1;
    }
    
    g_wifi_manager = manager;
    
    // Khởi tạo NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Khởi tạo network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    // Cấu hình WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Đăng ký event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    // Cấu hình station mode
    wifi_config_t wifi_sta_config = {0};
    strncpy((char*)wifi_sta_config.sta.ssid, ssid, sizeof(wifi_sta_config.sta.ssid) - 1);
    strncpy((char*)wifi_sta_config.sta.password, password, sizeof(wifi_sta_config.sta.password) - 1);
    wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_sta_config.sta.pmf_cfg.capable = true;
    wifi_sta_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    
    // Lưu cấu hình
    strncpy(manager->ssid, ssid, sizeof(manager->ssid) - 1);
    strncpy(manager->password, password, sizeof(manager->password) - 1);
    manager->is_connected = false;
    manager->retry_count = 0;
    manager->max_retry = 5;
    manager->event_group = xEventGroupCreate();
    
    ESP_LOGI(TAG, "WiFi initialized with SSID: %s", ssid);
    
    return 0;
}

int wifi_connect(wifi_manager_t *manager)
{
    if (manager == NULL) {
        return -1;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", manager->ssid);
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Chờ kết nối
    EventBits_t bits = xEventGroupWaitBits(manager->event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi SSID: %s", manager->ssid);
        return 0;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi SSID: %s", manager->ssid);
        return -1;
    } else {
        ESP_LOGE(TAG, "Unexpected event");
        return -1;
    }
}

int wifi_disconnect(wifi_manager_t *manager)
{
    if (manager == NULL) {
        return -1;
    }
    
    ESP_LOGI(TAG, "Disconnecting from WiFi");
    
    esp_wifi_disconnect();
    esp_wifi_stop();
    
    manager->is_connected = false;
    xEventGroupClearBits(manager->event_group, WIFI_CONNECTED_BIT);
    
    return 0;
}

bool wifi_is_connected(wifi_manager_t *manager)
{
    if (manager == NULL) {
        return false;
    }
    
    return manager->is_connected;
}

int wifi_get_ip_address(char *ip_str)
{
    if (ip_str == NULL) {
        return -1;
    }
    
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return -1;
    }
    
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return -1;
    }
    
    sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
    
    return 0;
}

