#include "buzzer.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "BUZZER";

// Cấu hình cho từng chế độ
static const struct {
    uint32_t frequency;
    uint8_t duty_percent;
    uint32_t on_duration_ms;
    uint32_t off_duration_ms;
} buzzer_mode_config[] = {
    [BUZZER_OFF] = {0, 0, 0, 0},
    [BUZZER_NORMAL] = {1000, 50, 200, 300},
    [BUZZER_URGENT] = {2000, 70, 150, 150},
    [BUZZER_ALARM] = {3000, 80, 100, 50}
};

int buzzer_init(buzzer_t *buzzer, uint8_t gpio_pin)
{
    if (buzzer == NULL) {
        return -1;
    }
    
    buzzer->gpio_pin = gpio_pin;
    buzzer->channel = BUZZER_DEFAULT_CHANNEL;
    buzzer->timer = BUZZER_DEFAULT_TIMER;
    buzzer->frequency = BUZZER_DEFAULT_FREQ;
    buzzer->is_active = false;
    buzzer->current_mode = BUZZER_OFF;
    
    // Cấu hình LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = BUZZER_DEFAULT_MODE,
        .timer_num = buzzer->timer,
        .duty_resolution = BUZZER_DEFAULT_RESOLUTION,
        .freq_hz = buzzer->frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    
    // Cấu hình LEDC channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode = BUZZER_DEFAULT_MODE,
        .channel = buzzer->channel,
        .timer_sel = buzzer->timer,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = buzzer->gpio_pin,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
    
    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d", gpio_pin);
    
    return 0;
}

int buzzer_on(buzzer_t *buzzer, uint32_t frequency, uint8_t duty_percent)
{
    if (buzzer == NULL) {
        return -1;
    }
    
    // Cập nhật tần số nếu thay đổi
    if (buzzer->frequency != frequency) {
        ledc_set_freq(BUZZER_DEFAULT_MODE, buzzer->timer, frequency);
        buzzer->frequency = frequency;
    }
    
    // Tính toán duty cycle
    uint32_t duty = (8191 * duty_percent) / 100; // 13-bit resolution
    
    ledc_set_duty(BUZZER_DEFAULT_MODE, buzzer->channel, duty);
    ledc_update_duty(BUZZER_DEFAULT_MODE, buzzer->channel);
    
    buzzer->is_active = true;
    
    return 0;
}

int buzzer_off(buzzer_t *buzzer)
{
    if (buzzer == NULL) {
        return -1;
    }
    
    ledc_set_duty(BUZZER_DEFAULT_MODE, buzzer->channel, 0);
    ledc_update_duty(BUZZER_DEFAULT_MODE, buzzer->channel);
    
    buzzer->is_active = false;
    
    return 0;
}

int buzzer_set_mode(buzzer_t *buzzer, buzzer_mode_t mode)
{
    if (buzzer == NULL || mode >= sizeof(buzzer_mode_config) / sizeof(buzzer_mode_config[0])) {
        return -1;
    }
    
    buzzer->current_mode = mode;
    
    if (mode == BUZZER_OFF) {
        buzzer_off(buzzer);
    } else {
        buzzer_on(buzzer, buzzer_mode_config[mode].frequency, buzzer_mode_config[mode].duty_percent);
    }
    
    ESP_LOGI(TAG, "Buzzer mode set to %d", mode);
    
    return 0;
}

int buzzer_beep_pattern(buzzer_t *buzzer, uint8_t beep_count, 
                       uint32_t beep_duration_ms, uint32_t pause_duration_ms)
{
    if (buzzer == NULL) {
        return -1;
    }
    
    for (uint8_t i = 0; i < beep_count; i++) {
        buzzer_on(buzzer, BUZZER_DEFAULT_FREQ, 50);
        vTaskDelay(pdMS_TO_TICKS(beep_duration_ms));
        buzzer_off(buzzer);
        
        if (i < beep_count - 1) {
            vTaskDelay(pdMS_TO_TICKS(pause_duration_ms));
        }
    }
    
    return 0;
}

void buzzer_task(void *pvParameters)
{
    buzzer_t *buzzer = (buzzer_t *)pvParameters;
    
    if (buzzer == NULL) {
        ESP_LOGE(TAG, "Buzzer task: Invalid parameters");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Buzzer task started");
    
    while (1) {
        if (buzzer->current_mode == BUZZER_OFF) {
            buzzer_off(buzzer);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        const uint32_t freq = buzzer_mode_config[buzzer->current_mode].frequency;
        const uint8_t duty = buzzer_mode_config[buzzer->current_mode].duty_percent;
        const uint32_t on_ms = buzzer_mode_config[buzzer->current_mode].on_duration_ms;
        const uint32_t off_ms = buzzer_mode_config[buzzer->current_mode].off_duration_ms;
        
        // Bật buzzer
        buzzer_on(buzzer, freq, duty);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        
        // Tắt buzzer
        buzzer_off(buzzer);
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}

