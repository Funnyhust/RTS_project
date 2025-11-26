#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Cấu hình buzzer
#define BUZZER_DEFAULT_FREQ 2000  // Tần số mặc định (Hz)
#define BUZZER_DEFAULT_CHANNEL LEDC_CHANNEL_0
#define BUZZER_DEFAULT_TIMER LEDC_TIMER_0
#define BUZZER_DEFAULT_MODE LEDC_LOW_SPEED_MODE
#define BUZZER_DEFAULT_RESOLUTION LEDC_TIMER_13_BIT

// Các chế độ cảnh báo
typedef enum {
    BUZZER_OFF = 0,           // Tắt
    BUZZER_NORMAL,            // Cảnh báo bình thường
    BUZZER_URGENT,            // Cảnh báo khẩn cấp
    BUZZER_ALARM              // Báo động cháy
} buzzer_mode_t;

// Cấu trúc cấu hình buzzer
typedef struct {
    uint8_t gpio_pin;
    ledc_channel_t channel;
    ledc_timer_t timer;
    uint32_t frequency;
    bool is_active;
    buzzer_mode_t current_mode;
} buzzer_t;

/**
 * @brief Khởi tạo buzzer
 * @param buzzer Con trỏ đến cấu trúc buzzer
 * @param gpio_pin GPIO pin kết nối buzzer
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int buzzer_init(buzzer_t *buzzer, uint8_t gpio_pin);

/**
 * @brief Bật buzzer với tần số và duty cycle
 * @param buzzer Con trỏ đến cấu trúc buzzer
 * @param frequency Tần số (Hz)
 * @param duty_percent Duty cycle (0-100%)
 * @return 0 nếu thành công
 */
int buzzer_on(buzzer_t *buzzer, uint32_t frequency, uint8_t duty_percent);

/**
 * @brief Tắt buzzer
 * @param buzzer Con trỏ đến cấu trúc buzzer
 * @return 0 nếu thành công
 */
int buzzer_off(buzzer_t *buzzer);

/**
 * @brief Đặt chế độ cảnh báo
 * @param buzzer Con trỏ đến cấu trúc buzzer
 * @param mode Chế độ cảnh báo
 * @return 0 nếu thành công
 */
int buzzer_set_mode(buzzer_t *buzzer, buzzer_mode_t mode);

/**
 * @brief Phát âm thanh cảnh báo theo pattern
 * @param buzzer Con trỏ đến cấu trúc buzzer
 * @param beep_count Số lần beep
 * @param beep_duration_ms Thời gian mỗi beep (ms)
 * @param pause_duration_ms Thời gian nghỉ giữa các beep (ms)
 * @return 0 nếu thành công
 */
int buzzer_beep_pattern(buzzer_t *buzzer, uint8_t beep_count, 
                       uint32_t beep_duration_ms, uint32_t pause_duration_ms);

/**
 * @brief Task FreeRTOS để điều khiển buzzer theo chế độ
 * @param pvParameters Tham số task (buzzer_t*)
 */
void buzzer_task(void *pvParameters);

#endif // BUZZER_H

