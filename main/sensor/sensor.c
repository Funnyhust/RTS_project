#include "sensor.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h"
#include "driver/gpio.h"

static const char *TAG = "SENSOR";

// Ngưỡng kích hoạt cho từng loại cảm biến
#define SMOKE_THRESHOLD 0.7f
#define TEMPERATURE_THRESHOLD 0.8f
#define IR_FLAME_THRESHOLD 0.6f
#define GAS_THRESHOLD 0.7f

// ADC handles
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool adc_initialized = false;

/**
 * @brief Calibration ADC
 */
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, 
                                 adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "ADC calibration scheme not supported");
    } else {
        ESP_LOGE(TAG, "ADC calibration failed");
    }

    return calibrated;
}

/**
 * @brief Khởi tạo ADC cho cảm biến analog
 */
static void sensor_adc_init(void)
{
    if (adc_initialized) {
        return;
    }
    
    // Cấu hình ADC1
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    
    // Calibration (sử dụng channel 0 làm mặc định, sẽ được cấu hình lại cho từng channel)
    adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_0, ADC_ATTEN_DB_12, &adc1_cali_handle);
    
    adc_initialized = true;
    ESP_LOGI(TAG, "ADC initialized");
}

int sensor_init(sensor_t *sensor, sensor_type_t type, uint8_t pin, bool is_analog)
{
    if (sensor == NULL) {
        return -1;
    }
    
    sensor->type = type;
    sensor->pin = pin;
    sensor->is_analog = is_analog;
    sensor->raw_value = 0;
    sensor->normalized_value = 0.0f;
    sensor->is_triggered = false;
    sensor->last_read_time = 0;
    
    if (is_analog) {
        // Khởi tạo ADC nếu chưa khởi tạo
        if (!adc_initialized) {
            sensor_adc_init();
        }
        
        // Cấu hình channel cho sensor này
        adc_oneshot_chan_cfg_t config = {
            .bitwidth = ADC_BITWIDTH_DEFAULT,
            .atten = ADC_ATTEN_DB_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, pin, &config));
    } else {
        // Cấu hình GPIO cho cảm biến digital
        gpio_set_direction(pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    }
    
    ESP_LOGI(TAG, "Sensor type %d initialized on pin %d (analog: %s)", 
             type, pin, is_analog ? "yes" : "no");
    
    return 0;
}

int sensor_read(sensor_t *sensor)
{
    if (sensor == NULL) {
        return -1;
    }
    
    if (sensor->is_analog) {
        // Đọc giá trị ADC
        int adc_reading = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, sensor->pin, &adc_reading));
        
        int voltage = 0;
        if (adc1_cali_handle != NULL) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_reading, &voltage));
        } else {
            // Nếu không có calibration, sử dụng giá trị raw (0-4095)
            voltage = adc_reading;
        }
        // Lưu giá trị raw (0-4095) thay vì voltage
        sensor->raw_value = (uint16_t)adc_reading;
    } else {
        // Đọc giá trị digital
        sensor->raw_value = gpio_get_level(sensor->pin) ? 0 : 4095; // Invert vì pull-up
    }
    
    // Chuẩn hóa giá trị
    sensor->normalized_value = sensor_normalize(sensor);
    
    // Kiểm tra ngưỡng kích hoạt
    float threshold = 0.0f;
    switch (sensor->type) {
        case SENSOR_TYPE_SMOKE:
            threshold = SMOKE_THRESHOLD;
            break;
        case SENSOR_TYPE_TEMPERATURE:
            threshold = TEMPERATURE_THRESHOLD;
            break;
        case SENSOR_TYPE_IR_FLAME:
            threshold = IR_FLAME_THRESHOLD;
            break;
        case SENSOR_TYPE_GAS:
            threshold = GAS_THRESHOLD;
            break;
    }
    
    sensor->is_triggered = (sensor->normalized_value >= threshold);
    sensor->last_read_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    return 0;
}

float sensor_normalize(sensor_t *sensor)
{
    if (sensor == NULL) {
        return 0.0f;
    }
    
    // Chuẩn hóa từ 0-4095 về 0.0-1.0
    return (float)sensor->raw_value / 4095.0f;
}

bool sensor_is_triggered(sensor_t *sensor)
{
    if (sensor == NULL) {
        return false;
    }
    
    return sensor->is_triggered;
}

int sensor_system_init(sensor_status_t *status)
{
    if (status == NULL) {
        return -1;
    }
    
    // Khởi tạo cảm biến khói (ví dụ: GPIO 34 - ADC_CHANNEL_6)
    sensor_init(&status->smoke, SENSOR_TYPE_SMOKE, ADC_CHANNEL_6, true);
    
    // Khởi tạo cảm biến nhiệt độ (ví dụ: GPIO 35 - ADC_CHANNEL_7)
    sensor_init(&status->temperature, SENSOR_TYPE_TEMPERATURE, ADC_CHANNEL_7, true);
    
    // Khởi tạo cảm biến IR flame (ví dụ: GPIO 32 - digital)
    sensor_init(&status->ir_flame, SENSOR_TYPE_IR_FLAME, GPIO_NUM_32, false);
    
    // Khởi tạo cảm biến khí gas (ví dụ: GPIO 33 - ADC_CHANNEL_5)
    sensor_init(&status->gas, SENSOR_TYPE_GAS, ADC_CHANNEL_5, true);
    
    status->fire_detected = false;
    status->detection_timestamp = 0;
    
    ESP_LOGI(TAG, "Sensor system initialized");
    
    return 0;
}

int sensor_system_read_all(sensor_status_t *status)
{
    if (status == NULL) {
        return -1;
    }
    
    sensor_read(&status->smoke);
    sensor_read(&status->temperature);
    sensor_read(&status->ir_flame);
    sensor_read(&status->gas);
    
    // Phát hiện cháy
    status->fire_detected = sensor_detect_fire(status);
    if (status->fire_detected) {
        status->detection_timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    
    return 0;
}

bool sensor_detect_fire(sensor_status_t *status)
{
    if (status == NULL) {
        return false;
    }
    
    // Logic phát hiện cháy: nếu có ít nhất 2 cảm biến kích hoạt
    int triggered_count = 0;
    
    if (status->smoke.is_triggered) triggered_count++;
    if (status->temperature.is_triggered) triggered_count++;
    if (status->ir_flame.is_triggered) triggered_count++;
    if (status->gas.is_triggered) triggered_count++;
    
    // Nếu IR flame kích hoạt, coi như cháy ngay lập tức
    if (status->ir_flame.is_triggered) {
        return true;
    }
    
    // Nếu có ít nhất 2 cảm biến khác kích hoạt
    return (triggered_count >= 2);
}

void sensor_task(void *pvParameters)
{
    sensor_status_t *status = (sensor_status_t *)pvParameters;
    
    if (status == NULL) {
        ESP_LOGE(TAG, "Sensor task: Invalid parameters");
        vTaskDelete(NULL);
        return;
    }
    
    const TickType_t delay = pdMS_TO_TICKS(500); // 500ms chu kỳ
    
    ESP_LOGI(TAG, "Sensor task started");
    
    while (1) {
        // Đọc tất cả cảm biến
        sensor_system_read_all(status);
        
        // Log thông tin cảm biến
        ESP_LOGD(TAG, "Smoke: %.2f, Temp: %.2f, IR: %d, Gas: %.2f, Fire: %s",
                 status->smoke.normalized_value,
                 status->temperature.normalized_value,
                 status->ir_flame.is_triggered,
                 status->gas.normalized_value,
                 status->fire_detected ? "YES" : "NO");
        
        if (status->fire_detected) {
            ESP_LOGW(TAG, "FIRE DETECTED! Timestamp: %lu", status->detection_timestamp);
        }
        
        vTaskDelay(delay);
    }
}

