#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_now.h>

// ==== OLED Display Settings ====
#define DISPLAY_I2C_PIN_RST 16
#define ESP_SDA_PIN 4
#define ESP_SCL_PIN 15
#define DISPLAY_I2C_ADDR 0x3C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, DISPLAY_I2C_PIN_RST, ESP_SCL_PIN, ESP_SDA_PIN);

// ==== Serial Communication Settings ====
HardwareSerial mySerial(2);  // Use UART2
#define MY_SERIAL_RX_PIN 12
#define MY_SERIAL_TX_PIN 13

// ==== ESP-NOW Data Structure ====
typedef struct __attribute__((packed)) struct_message {
  int windDir;
  float windSpeed;
  float windGust;
  float temperature;
  float humidity;
  float batVoltage;
} struct_message;

struct_message dataToSend;

// ==== MAC Addresses of Receivers ====
uint8_t receiverMAC1[] = {0x3C, 0x71, 0xBF, 0xAB, 0x5B, 0x78};  // Receiver 1
uint8_t receiverMAC2[] = {0x84, 0xFC, 0xE6, 0x6C, 0xCA, 0x40};  // Receiver 2 (ePaper)

// ==== Serial Buffer ====
String serialBuffer = "";

void setup() {
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, MY_SERIAL_RX_PIN, MY_SERIAL_TX_PIN);

  // WiFi Init
  WiFi.mode(WIFI_STA);
  Serial.println("📡 WiFi initialized in station mode");

  // ESP-NOW Init
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW initialization failed");
    return;
  }

  // Add receiver 1
  esp_now_peer_info_t peerInfo1 = {};
  memcpy(peerInfo1.peer_addr, receiverMAC1, 6);
  peerInfo1.channel = 0;
  peerInfo1.encrypt = false;
  if (esp_now_add_peer(&peerInfo1) != ESP_OK) {
    Serial.println("❌ Failed to add peer 1");
  } else {
    Serial.println("✅ Peer 1 added");
  }

  // Add receiver 2
  esp_now_peer_info_t peerInfo2 = {};
  memcpy(peerInfo2.peer_addr, receiverMAC2, 6);
  peerInfo2.channel = 0;
  peerInfo2.encrypt = false;
  if (esp_now_add_peer(&peerInfo2) != ESP_OK) {
    Serial.println("❌ Failed to add peer 2");
  } else {
    Serial.println("✅ Peer 2 added");
  }

  // OLED Init
  Wire.begin(ESP_SDA_PIN, ESP_SCL_PIN);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 20, "ESP32 Sender");
  u8g2.drawStr(10, 40, "Waiting for Data...");
  u8g2.sendBuffer();
}

void loop() {
  while (mySerial.available()) {
    char incomingByte = mySerial.read();
    serialBuffer += incomingByte;
  }

  if (serialBuffer.indexOf("SHT30") != -1) {
    Serial.println("\n✅ Valid data received:");
    Serial.println(serialBuffer);

    parseData(serialBuffer);
    serialBuffer = "";

    sendESPNowData();
    displayData();
  }

  delay(500);
}

// ==== Send ESP-NOW Data to All Receivers ====
void sendESPNowData() {
  Serial.println("\n📤 Sending data via ESP-NOW...");

  esp_err_t result;

  result = esp_now_send(receiverMAC1, (uint8_t *)&dataToSend, sizeof(dataToSend));
  Serial.println(result == ESP_OK ? "✅ Data sent to receiver 1" : "❌ Failed to send to receiver 1");

  result = esp_now_send(receiverMAC2, (uint8_t *)&dataToSend, sizeof(dataToSend));
  Serial.println(result == ESP_OK ? "✅ Data sent to receiver 2" : "❌ Failed to send to receiver 2");
}

// ==== Parse Incoming Serial Data ====
void parseData(String data) {
  data.trim();

  if (data.indexOf("WindDir") != -1) dataToSend.windDir = getValue(data, "WindDir");
  if (data.indexOf("WindSpeed") != -1) dataToSend.windSpeed = getValue(data, "WindSpeed") * 1.94384;
  if (data.indexOf("WindGust") != -1) dataToSend.windGust = getValue(data, "WindGust") * 1.94384;
  if (data.indexOf("Temperature") != -1) dataToSend.temperature = getValue(data, "Temperature");
  if (data.indexOf("Humi") != -1) dataToSend.humidity = getValue(data, "Humi");
  if (data.indexOf("BatVoltage") != -1) dataToSend.batVoltage = getValue(data, "BatVoltage");

  // Print parsed values
  Serial.println("\n📊 Parsed Data:");
  Serial.printf("🌬 WindDir: %d°\n", dataToSend.windDir);
  Serial.printf("💨 WindSpeed: %.1f knots\n", dataToSend.windSpeed);
  Serial.printf("🌪 WindGust: %.1f knots\n", dataToSend.windGust);
  Serial.printf("🌡 Temp: %.1f C\n", dataToSend.temperature);
  Serial.printf("💧 Humi: %.1f %%\n", dataToSend.humidity);
  Serial.printf("🔋 BatVoltage: %.2f V\n", dataToSend.batVoltage);
}

// ==== Extract Values from Serial String ====
float getValue(String data, String key) {
  int keyPos = data.indexOf(key);
  if (keyPos != -1) {
    int startPos = data.indexOf("=", keyPos) + 1;
    int endPos = data.indexOf("\n", startPos);
    String valueString = data.substring(startPos, endPos);
    valueString.trim();
    return valueString.toFloat();
  }
  return 0.0;
}

// ==== Display Parsed Data on OLED ====
void displayData() {
  Serial.println("\n🖥 Displaying data on OLED...");

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, ("WindDir: " + String(dataToSend.windDir) + " ").c_str());
  u8g2.drawStr(0, 20, ("Wind   : " + String(dataToSend.windSpeed) + "  knots").c_str());
  u8g2.drawStr(0, 30, ("Gust.  : " + String(dataToSend.windGust) + "  knots").c_str());
  u8g2.drawStr(0, 40, ("Temp.  : " + String(dataToSend.temperature) + "  C").c_str());
  u8g2.drawStr(0, 50, ("Humi.  : " + String(dataToSend.humidity) + "  %").c_str());
  u8g2.drawStr(0, 60, ("BatVolt: " + String(dataToSend.batVoltage) + "   V").c_str());
  u8g2.sendBuffer();
}
