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

// Hàm giả lập đọc dữ liệu từ Pin qua giao thức OneWire (Bạn sẽ thay thế bằng code OneWire thực tế ở đây)
void readMakitaBatteryData() {
    // Kích hoạt chân ENABLE để bật BMS của pin Makita
    digitalWrite(MAKITA_EN_PIN, HIGH);
    delay(10); 
    
    // [Đoạn này dùng để viết code bắt tay OneWire truyền nhận với chân MAKITA_DATA_PIN]
    // Tạm thời giả lập dữ liệu thay đổi nhỏ để kiểm tra tính năng cập nhật màn hình:
    battery.packVoltage = 0;
    for(int i=0; i<5; i++) {
        battery.cells[i] = 3.3 + (random(-20, 20) / 1000.0); // Biến thiên nhẹ quanh 3.3V
        battery.packVoltage += battery.cells[i];
    }
    battery.tempCell = 28.0 + (random(0, 20) / 10.0);
    
    digitalWrite(MAKITA_EN_PIN, LOW); // Tắt enable sau khi đọc xong để tiết kiệm điện
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
    
    // Kiểm tra nút bấm vật lý vật lý trên bo mạch LilyGO để thực hiện Reset lỗi pin
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