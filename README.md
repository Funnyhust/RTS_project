# Hệ Thống Báo Cháy Thông Minh ESP32

Hệ thống báo cháy thông minh sử dụng ESP32 với các cảm biến đa dạng, kết nối WiFi và giao tiếp MQTT để giám sát và cảnh báo cháy nổ trong thời gian thực.

## 📋 Mục Lục

- [Tổng Quan](#tổng-quan)
- [Kiến Trúc Hệ Thống](#kiến-trúc-hệ-thống)
- [Tính Năng](#tính-năng)
- [Phần Cứng](#phần-cứng)
- [Cài Đặt](#cài-đặt)
- [Cấu Hình](#cấu-hình)
- [Sử Dụng](#sử-dụng)
- [Cấu Trúc Dự Án](#cấu-trúc-dự-án)
- [API](#api)
- [Troubleshooting](#troubleshooting)

## 🎯 Tổng Quan

Hệ thống báo cháy này được thiết kế với kiến trúc 3 tầng:

1. **Tầng Thiết Bị (Device Layer)**: ESP32 với các cảm biến và actuator
2. **Tầng Mạng (Network Layer)**: WiFi và MQTT với TLS
3. **Tầng Ứng Dụng (Application Layer)**: Server, database và dashboard

Hệ thống sử dụng FreeRTOS để xử lý đa nhiệm với các task ưu tiên khác nhau, đảm bảo phản ứng nhanh khi phát hiện cháy.

## 🏗️ Kiến Trúc Hệ Thống

```
┌─────────────────────────────────────────┐
│      Application Layer                  │
│  (Server, Database, Dashboard)          │
└─────────────────┬───────────────────────┘
                  │ MQTT/TLS
┌─────────────────▼───────────────────────┐
│      Network Layer                      │
│  (WiFi, MQTT Broker, TLS)               │
└─────────────────┬───────────────────────┘
                  │ WiFi
┌─────────────────▼───────────────────────┐
│      Device Layer (ESP32)               │
│  ┌──────────┐  ┌──────────┐            │
│  │ Sensors  │  │ Actuator │            │
│  │ - Smoke  │  │ - Buzzer │            │
│  │ - Temp   │  │ - LED    │            │
│  │ - IR     │  │          │            │
│  │ - Gas    │  │          │            │
│  └──────────┘  └──────────┘            │
│         FreeRTOS Tasks                  │
└─────────────────────────────────────────┘
```

## ✨ Tính Năng

### Cảm Biến
- ✅ Cảm biến khói (Smoke Sensor)
- ✅ Cảm biến nhiệt độ (Temperature Sensor)
- ✅ Cảm biến tia lửa hồng ngoại (IR Flame Sensor)
- ✅ Cảm biến khí gas (Gas Sensor)
- ✅ Phát hiện cháy thông minh (kết hợp nhiều cảm biến)

### Điều Khiển
- ✅ Còi báo (Buzzer) với nhiều chế độ cảnh báo
- ✅ Điều khiển từ xa qua MQTT

### Kết Nối
- ✅ WiFi với tự động kết nối lại
- ✅ MQTT với hỗ trợ TLS
- ✅ Gửi dữ liệu cảm biến định kỳ
- ✅ Nhận lệnh điều khiển từ server

### Xử Lý Thời Gian Thực
- ✅ FreeRTOS với lập lịch ưu tiên cố định
- ✅ Task cảm biến: 500ms chu kỳ (ưu tiên cao)
- ✅ Task cảnh báo: Phản ứng ngay khi phát hiện cháy
- ✅ Task MQTT: Gửi dữ liệu mỗi 5 giây

## 🔧 Phần Cứng

### Yêu Cầu
- ESP32 Development Board
- Cảm biến khói (MQ-2 hoặc tương tự)
- Cảm biến nhiệt độ (LM35, DS18B20 hoặc tương tự)
- Cảm biến IR flame
- Cảm biến khí gas (MQ-5, MQ-9 hoặc tương tự)
- Buzzer (5V active buzzer)
- Điện trở và dây nối

### Kết Nối

#### Cảm Biến Analog (ADC)
- Cảm biến khói: GPIO 34 (ADC1_CHANNEL_6)
- Cảm biến nhiệt độ: GPIO 35 (ADC1_CHANNEL_7)
- Cảm biến khí gas: GPIO 33 (ADC1_CHANNEL_5)

#### Cảm Biến Digital
- Cảm biến IR flame: GPIO 32

#### Actuator
- Buzzer: GPIO 25 (có thể thay đổi trong `main.c`)

**Lưu ý**: Các GPIO có thể được thay đổi trong file `main/sensor/sensor.c` và `main/main.c`

## 📦 Cài Đặt

### Yêu Cầu
- ESP-IDF v5.5.1 hoặc mới hơn
- Python 3.11+
- CMake 3.16+
- Git

### Các Bước Cài Đặt

1. **Clone repository** (nếu có) hoặc tải project

2. **Cài đặt ESP-IDF** (nếu chưa có):
```bash
# Trên Windows
cd %userprofile%\esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
install.bat
export.bat
```

3. **Cấu hình project**:
```bash
cd He_thong_bao_chay
idf.py set-target esp32
idf.py menuconfig
```

4. **Build project**:
```bash
idf.py build
```

5. **Flash và monitor**:
```bash
idf.py -p COMx flash monitor
```
(Thay `COMx` bằng cổng COM của ESP32 trên máy bạn)

## ⚙️ Cấu Hình

### 1. Cấu Hình WiFi

Mở file `main/main.c` và thay đổi:
```c
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

### 2. Cấu Hình MQTT

Trong file `main/main.c`:
```c
#define MQTT_BROKER_URI "mqtt://broker.example.com"  // Hoặc mqtts:// cho TLS
#define MQTT_USERNAME NULL  // Hoặc "username" nếu cần
#define MQTT_PASSWORD NULL  // Hoặc "password" nếu cần
#define MQTT_CLIENT_ID "fire_system_esp32"
#define MQTT_USE_TLS false  // true nếu dùng TLS
```

### 3. Cấu Hình GPIO

Thay đổi GPIO cho buzzer trong `main/main.c`:
```c
#define BUZZER_GPIO_PIN GPIO_NUM_25
```

Thay đổi GPIO cho cảm biến trong `main/sensor/sensor.c`:
```c
// Trong hàm sensor_system_init()
sensor_init(&status->smoke, SENSOR_TYPE_SMOKE, ADC_CHANNEL_6, true);
// ...
```

### 4. Cấu Hình Ngưỡng Cảm Biến

Trong file `main/sensor/sensor.c`:
```c
#define SMOKE_THRESHOLD 0.7f
#define TEMPERATURE_THRESHOLD 0.8f
#define IR_FLAME_THRESHOLD 0.6f
#define GAS_THRESHOLD 0.7f
```

## 🚀 Sử Dụng

### Khởi Động Hệ Thống

1. Kết nối phần cứng theo sơ đồ
2. Cấu hình WiFi và MQTT trong `main.c`
3. Build và flash firmware lên ESP32
4. Mở serial monitor để xem log

### MQTT Topics

Hệ thống sử dụng các MQTT topics sau:

- **Publish**:
  - `fire_system/sensor/data`: Dữ liệu cảm biến (QoS 1, mỗi 5 giây)
  - `fire_system/alert`: Cảnh báo cháy (QoS 2, retain, khi phát hiện cháy)
  - `fire_system/status`: Trạng thái hệ thống (QoS 0, mỗi 5 giây)

- **Subscribe**:
  - `fire_system/control`: Nhận lệnh điều khiển

### Lệnh Điều Khiển MQTT

Gửi JSON message đến topic `fire_system/control`:

```json
{
  "command": "buzzer_on"
}
```

```json
{
  "command": "buzzer_off"
}
```

```json
{
  "command": "test_alarm"
}
```

### Định Dạng Dữ Liệu Cảm Biến

```json
{
  "timestamp": 1234567890,
  "smoke": 0.75,
  "temperature": 0.82,
  "ir_flame": false,
  "gas": 0.65,
  "fire_detected": false
}
```

### Định Dạng Cảnh Báo Cháy

```json
{
  "type": "fire_alert",
  "detected": true,
  "timestamp": 1234567890,
  "smoke": 0.85,
  "temperature": 0.90,
  "ir_flame": true,
  "gas": 0.80
}
```

## 📁 Cấu Trúc Dự Án

```
He_thong_bao_chay/
├── main/
│   ├── main.c              # File chính, tích hợp tất cả module
│   ├── CMakeLists.txt      # Cấu hình build
│   ├── sensor/
│   │   ├── sensor.h        # Header cảm biến
│   │   └── sensor.c        # Implementation cảm biến
│   ├── buzzer/
│   │   ├── buzzer.h        # Header buzzer
│   │   └── buzzer.c        # Implementation buzzer
│   ├── wifi/
│   │   ├── wifi.h          # Header WiFi
│   │   └── wifi.c          # Implementation WiFi
│   └── mqtt/
│       ├── mqtt.h          # Header MQTT
│       └── mqtt.c          # Implementation MQTT
├── CMakeLists.txt          # Root CMakeLists
├── sdkconfig               # Cấu hình ESP-IDF
└── README.md               # File này
```

## 📚 API

### Sensor API

```c
// Khởi tạo hệ thống cảm biến
int sensor_system_init(sensor_status_t *status);

// Đọc tất cả cảm biến
int sensor_system_read_all(sensor_status_t *status);

// Phát hiện cháy
bool sensor_detect_fire(sensor_status_t *status);
```

### Buzzer API

```c
// Khởi tạo buzzer
int buzzer_init(buzzer_t *buzzer, uint8_t gpio_pin);

// Đặt chế độ cảnh báo
int buzzer_set_mode(buzzer_t *buzzer, buzzer_mode_t mode);
```

### WiFi API

```c
// Khởi tạo WiFi
int wifi_init(wifi_manager_t *manager, const char *ssid, const char *password);

// Kết nối WiFi
int wifi_connect(wifi_manager_t *manager);

// Kiểm tra trạng thái
bool wifi_is_connected(wifi_manager_t *manager);
```

### MQTT API

```c
// Khởi tạo MQTT
int mqtt_init(mqtt_config_t *config, const char *uri, const char *username,
              const char *password, const char *client_id, bool use_tls);

// Kết nối MQTT
int mqtt_connect(mqtt_config_t *config);

// Gửi dữ liệu cảm biến
int mqtt_publish_sensor_data(mqtt_config_t *config, const char *sensor_data);

// Gửi cảnh báo
int mqtt_publish_alert(mqtt_config_t *config, const char *alert_data);
```

## 🔍 Troubleshooting

### WiFi không kết nối được
- Kiểm tra SSID và password
- Kiểm tra tín hiệu WiFi
- Xem log serial để biết lỗi cụ thể

### MQTT không kết nối
- Kiểm tra URI broker
- Kiểm tra username/password nếu cần
- Kiểm tra firewall/network
- Thử dùng MQTT client để test broker

### Cảm biến không đọc được
- Kiểm tra kết nối GPIO
- Kiểm tra nguồn cấp cho cảm biến
- Kiểm tra ADC channel mapping
- Xem log để biết giá trị đọc được

### Buzzer không hoạt động
- Kiểm tra GPIO pin
- Kiểm tra nguồn cấp (5V)
- Kiểm tra kết nối
- Thử test với `buzzer_beep_pattern()`

### Build lỗi
- Đảm bảo ESP-IDF v5.5.1
- Chạy `idf.py fullclean` và build lại
- Kiểm tra các component dependencies trong CMakeLists.txt

## 📝 License

Dự án này được phát triển cho mục đích giáo dục và nghiên cứu.

## 👥 Tác Giả

Phát triển bởi RTS Lab

## 📞 Liên Hệ

Nếu có vấn đề hoặc câu hỏi, vui lòng tạo issue hoặc liên hệ trực tiếp.

---

**Lưu ý**: Đây là hệ thống báo cháy thử nghiệm. Để sử dụng trong môi trường thực tế, cần kiểm tra và chứng nhận an toàn.

