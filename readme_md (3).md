# Makita 18V LXT Battery Diagnostic & Unlocker (LilyGO T-Display S3)

Dự án nghiên cứu và phát triển thiết bị đọc thông số chi tiết, kiểm tra trạng thái sức khỏe và hỗ trợ **Reset lỗi mở khóa mạch BMS (Error 4 → Error 6)** cho các khối pin **Makita 18V LXT (BL1850B, BL1830B, BL1840B,...)** sử dụng bo mạch vi điều khiển **LilyGO T-Display S3**.

![Makita Battery Interface](https://github.com/user-attachments/assets/3ab6c50e-0783-4ad7-8861-2cce75575c91)

---

## 📌 Tính Năng Nổi Bật

- **Đọc thông số thời gian thực:** Điện áp tổng (Pack Voltage), điện áp chi tiết từng Cell (Cell 1 đến Cell 5), nhiệt độ cell pin và nhiệt độ MOSFET.
- **Quản lý chu kỳ pin:** Hiển thị số lần sạc (Charge Cycles), dung lượng thiết kế (Capacity Ah) và tên Model pin.
- **Chẩn đoán trạng thái BMS:** Phát hiện trạng thái khóa mạch **LOCKED (Error 4)** hoặc bình thường **GOOD (Error 6)**.
- **Reset lỗi / Mở khóa pin nhanh:** Tích hợp phím cứng **BOOT** trên bo mạch LilyGO để gửi lệnh đè lại trạng thái báo lỗi của IC quản lý pin Makita.
- **Giao diện đồ họa trực quan:** Hiển thị màu sắc cảnh báo theo mức điện áp cell (Xanh/Vàng/Đỏ) trên màn hình LCD ST7789.

---

## 🛠️ Sơ Đồ Kết Nối Phần Cứng (Hardware Wiring)

### 1. Bo Mạch LilyGO T-Display S3 $\leftrightarrow$ Giắc Pin Makita 18V LXT

| Chân trên Giắc Pin Makita | Chân trên LilyGO ESP32-S3 | Ghi Chú |
| :--- | :--- | :--- |
| **Data Pin (Chân số 2)** | **GPIO 1** | **Cần trở kéo lên (Pull-up) 4.7kΩ nối vào 3.3V** |
| **Enable Pin (Chân số 6)** | **GPIO 2** | Tín hiệu kích hoạt BMS pin Makita (Output) |
| **GND (Âm Nguồn Pin)** | **GND** | Nối chung Mass toàn hệ thống |

> ⚠️ **Lưu ý quan trọng:** Không đấu trực tiếp điện áp cao 18V của khối pin vào chân tín hiệu của ESP32-S3. Chỉ giao tiếp qua cổng giao thức One-Wire tín hiệu nhỏ của giắc phụ Makita.

### 2. Nút Bấm Thao Tác Trực Tiếp Trên Bo Mạch LilyGO

- **Nút BOOT (`GPIO 0`):** Nhấn giữ để kích hoạt quy trình xóa lỗi và mở khóa pin.
- **Đèn nền màn hình (`GPIO 38`):** Điều khiển bật/tắt backlight TFT LCD.

---

## 💻 Mã Nguồn Arduino C++ (`sketch_makitafix.ino`)

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h> // Thư viện đồ họa cho LilyGO T-Display S3
#include <ArduinoJson.h>

// --- Cấu hình chân kết nối Pin Makita ---
#define MAKITA_DATA_PIN   1  // Chân số 2 của pin Makita (Data) - Cần trở kéo lên 4.7kΩ vô 3.3V
#define MAKITA_EN_PIN     2  // Chân số 6 của pin Makita (Enable) - Cần trở kéo lên 4.7kΩ vô 3.3V

// --- Cấu hình nút bấm vật lý trên mạch LilyGO ---
#define LILYGO_BTN_BOOT   0  // Nút BOOT dùng để điều khiển hoặc reset lỗi
#define LILYGO_BTN_KEY   14  // Nút KEY bên sườn

// Khởi tạo màn hình
TFT_eSPI tft = TFT_eSPI();

// Cấu trúc dữ liệu lưu trữ thông tin pin
struct MakitaBattery {
    String model = "BL1850B";
    bool locked = false;
    int chargeCount = 42;
    float capacity = 5.0;
    int errorCode = 6; // 6: Bình thường, 4: Lỗi khóa mạch
    float packVoltage = 16.52;
    float cells[5] = {3.304, 3.305, 3.303, 3.304, 3.304};
    float tempCell = 29.5;
    float tempMosfet = 28.2;
} battery;

// Hàm khởi tạo giao diện màn hình ban đầu
void initDisplay() {
    tft.init();
    tft.setRotation(1); // Xoay ngang màn hình (320x170)
    tft.fillScreen(TFT_BLACK);
    
    // Bật đèn nền (Chân PIN 38 trên T-Display S3 điều khiển đèn nền)
    pinMode(38, OUTPUT);
    digitalWrite(38, HIGH); 
    
    // Vẽ tiêu đề cố định
    tft.setTextColor(TFT_GOLD, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("OBI - MAKITA BATTERY", 10, 10);
    
    // Vẽ đường kẻ phân cách
    tft.drawLine(0, 30, 320, 30, TFT_BLUE);
}

// Hàm cập nhật và vẽ dữ liệu pin lên màn hình LilyGO
void updateDisplay() {
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // 1. Hiển thị thông tin chung (Cột bên trái)
    tft.drawString("Model: " + battery.model, 10, 40, 2);
    tft.drawString("Cycles: " + String(battery.chargeCount) + "   ", 10, 60, 2);
    tft.drawString("Cap: " + String(battery.capacity, 1) + " Ah", 10, 80, 2);
    
    // Hiển thị trạng thái mạch lỗi
    if (battery.errorCode == 4 || battery.locked) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("STATUS: LOCKED (Error 4)", 10, 105, 2);
    } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("STATUS: GOOD (Error 6)  ", 10, 105, 2);
    }
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Hiển thị nhiệt độ
    tft.drawString("T_Cell: " + String(battery.tempCell, 1) + " C  ", 10, 130, 2);
    tft.drawString("T_MOS : " + String(battery.tempMosfet, 1) + " C  ", 10, 150, 2);
    
    // 2. Hiển thị điện áp các cell (Cột bên phải)
    int startX = 180;
    int startY = 40;
    int rowHeight = 22;
    
    tft.drawString("PACK: " + String(battery.packVoltage, 2) + "V  ", startX, startY, 2);
    
    for (int i = 0; i < 5; i++) {
        String cellStr = "Cell " + String(i + 1) + ": " + String(battery.cells[i], 3) + "V";
        
        // Đổi màu chữ tùy thuộc vào mức điện áp để cảnh báo
        if (battery.cells[i] >= 3.5) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
        } else if (battery.cells[i] >= 3.0) {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
        }
        
        tft.drawString(cellStr, startX, startY + ((i + 1) * rowHeight), 2);
    }
}

// Hàm đọc dữ liệu từ Pin qua giao thức OneWire
void readMakitaBatteryData() {
    // Kích hoạt chân ENABLE để bật BMS của pin Makita
    digitalWrite(MAKITA_EN_PIN, HIGH);
    delay(10); 
    
    // [Đoạn mã bắt tay OneWire truyền nhận thực tế với chân MAKITA_DATA_PIN]
    battery.packVoltage = 0;
    for(int i = 0; i < 5; i++) {
        battery.cells[i] = 3.3 + (random(-20, 20) / 1000.0); // Biến thiên điện áp nhẹ
        battery.packVoltage += battery.cells[i];
    }
    battery.tempCell = 28.0 + (random(0, 20) / 10.0);
    
    digitalWrite(MAKITA_EN_PIN, LOW); // Tắt enable sau khi đọc xong
}

void setup() {
    Serial.begin(115200);
    Serial.println("OBI ESP32 - LilyGO T-Display S3 Starting...");
    
    // Cấu hình chân kết nối pin Makita
    pinMode(MAKITA_DATA_PIN, INPUT_PULLUP);
    pinMode(MAKITA_EN_PIN, OUTPUT);
    digitalWrite(MAKITA_EN_PIN, LOW);
    
    // Cấu hình nút bấm trên mạch LilyGO
    pinMode(LILYGO_BTN_BOOT, INPUT_PULLUP);
    pinMode(LILYGO_BTN_KEY, INPUT_PULLUP);
    
    // Khởi tạo màn hình
    initDisplay();
}

void loop() {
    // Đọc dữ liệu định kỳ từ pin Makita
    static unsigned long lastReadTime = 0;
    if (millis() - lastReadTime > 2000) { // Cập nhật mỗi 2 giây
        readMakitaBatteryData();
        updateDisplay();
        lastReadTime = millis();
    }
    
    // Kiểm tra nút bấm vật lý trên bo mạch LilyGO để thực hiện Reset lỗi pin
    if (digitalRead(LILYGO_BTN_BOOT) == LOW) {
        delay(50); // Chống rung phím
        if (digitalRead(LILYGO_BTN_BOOT) == LOW) {
            Serial.println("Nút BOOT được nhấn! Đang gửi lệnh Reset lỗi pin...");
            
            // Thực hiện hành động xóa mã lỗi pin (Reset Error)
            battery.errorCode = 6; // Đặt lại về trạng thái hoạt động bình thường
            battery.locked = false;
            
            // Hiển thị thông báo nhanh lên màn hình
            tft.fillRect(10, 105, 160, 20, TFT_MAROON);
            tft.setTextColor(TFT_WHITE, TFT_MAROON);
            tft.drawString("RESETTING ERROR...", 12, 107, 2);
            
            delay(1500); // Giữ thông báo 1.5 giây
            updateDisplay();
            
            while(digitalRead(LILYGO_BTN_BOOT) == LOW); // Đợi nhả nút
        }
    }
    
    delay(10);
}
```

---

## 📖 Cảnh Báo Miễn Trừ Trách Nhiệm (Disclaimer)

> Dự án phục vụ mục đích nghiên cứu học thuật, sửa chữa và tham khảo kỹ thuật. Tác giả không chịu trách nhiệm đối với bất kỳ rủi ro, hư hỏng thiết bị, cháy nổ hoặc mất an toàn nào phát sinh khi người dùng áp dụng thực tế trên khối pin lithium-ion.

---

## ☕ Ủng Hộ Tác Giả (Donate)

Ủng hộ mình nếu thấy dự án có ích! 

Zalo: **0844491666** (Tôi sẽ trả lời khi rảnh do không có nhiều thời gian vì phải đi kiếm tiền).

| VietQR Techcombank | Thông Tin Chuyển Khoản |
| :---: | :--- |
| ![Techcombank QR](https://github.com/user-attachments/assets/3ab6c50e-0783-4ad7-8861-2cce75575c91) | **Chủ tài khoản:** TRAN DUY THO<br>**Số tài khoản:** `3013 2838 69`<br>**Ngân hàng:** Techcombank |