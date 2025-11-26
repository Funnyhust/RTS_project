#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Định nghĩa các loại cảm biến
typedef enum {
    SENSOR_TYPE_SMOKE = 0,      // Cảm biến khói
    SENSOR_TYPE_TEMPERATURE,    // Cảm biến nhiệt độ
    SENSOR_TYPE_IR_FLAME,       // Cảm biến tia lửa hồng ngoại
    SENSOR_TYPE_GAS             // Cảm biến khí gas
} sensor_type_t;

// Cấu trúc dữ liệu cảm biến
typedef struct {
    sensor_type_t type;
    uint8_t pin;
    bool is_analog;
    uint16_t raw_value;
    float normalized_value;
    bool is_triggered;
    uint32_t last_read_time;
} sensor_t;

// Cấu trúc trạng thái tổng hợp cảm biến
typedef struct {
    sensor_t smoke;
    sensor_t temperature;
    sensor_t ir_flame;
    sensor_t gas;
    bool fire_detected;
    uint32_t detection_timestamp;
} sensor_status_t;

/**
 * @brief Khởi tạo cảm biến
 * @param sensor Con trỏ đến cấu trúc cảm biến
 * @param type Loại cảm biến
 * @param pin GPIO pin hoặc ADC channel
 * @param is_analog true nếu là cảm biến analog, false nếu digital
 * @return ESP_OK nếu thành công
 */
int sensor_init(sensor_t *sensor, sensor_type_t type, uint8_t pin, bool is_analog);

/**
 * @brief Đọc giá trị từ cảm biến
 * @param sensor Con trỏ đến cấu trúc cảm biến
 * @return ESP_OK nếu thành công
 */
int sensor_read(sensor_t *sensor);

/**
 * @brief Chuẩn hóa giá trị cảm biến (0.0 - 1.0)
 * @param sensor Con trỏ đến cấu trúc cảm biến
 * @return Giá trị đã chuẩn hóa
 */
float sensor_normalize(sensor_t *sensor);

/**
 * @brief Kiểm tra cảm biến có bị kích hoạt không
 * @param sensor Con trỏ đến cấu trúc cảm biến
 * @return true nếu bị kích hoạt
 */
bool sensor_is_triggered(sensor_t *sensor);

/**
 * @brief Khởi tạo tất cả cảm biến
 * @param status Con trỏ đến cấu trúc trạng thái cảm biến
 * @return ESP_OK nếu thành công
 */
int sensor_system_init(sensor_status_t *status);

/**
 * @brief Đọc tất cả cảm biến
 * @param status Con trỏ đến cấu trúc trạng thái cảm biến
 * @return ESP_OK nếu thành công
 */
int sensor_system_read_all(sensor_status_t *status);

/**
 * @brief Phát hiện cháy dựa trên dữ liệu cảm biến
 * @param status Con trỏ đến cấu trúc trạng thái cảm biến
 * @return true nếu phát hiện cháy
 */
bool sensor_detect_fire(sensor_status_t *status);

/**
 * @brief Task FreeRTOS để đọc cảm biến định kỳ
 * @param pvParameters Tham số task (sensor_status_t*)
 */
void sensor_task(void *pvParameters);

#endif // SENSOR_H

