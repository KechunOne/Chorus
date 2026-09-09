#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <Wire.h>

// ==========================================
// Wi-Fi configuration
// ==========================================

const char* ssid = "GL-AXT1800-fb5";
const char* password = "2TD9YAPC5S";

// ==========================================
// Max/MSP configuration
// ==========================================

// 运行 Max 的电脑的局域网 IP
const char* host = "192.168.8.143";

// Max端 [udpreceive] 监听的端口
const int port = 8000;

WiFiUDP udp;

// ==========================================
// MPU6050 configuration
// ==========================================
// 接线: VCC->3V3, GND->GND, SCL->GP5, SDA->GP4

const uint8_t MPU_ADDR = 0x68;
const uint8_t PWR_MGMT_1 = 0x6B;
const uint8_t ACCEL_XOUT_H = 0x3B;
const uint8_t GYRO_XOUT_H = 0x43;

const float ACCEL_SCALE = 16384.0f;  // LSB/g (±2g)
const float GYRO_SCALE = 131.0f;     // LSB/(deg/s) (±250 dps)

const unsigned long sendInterval = 25;  // ms, ~40Hz
unsigned long previousSendTime = 0;

// ==========================================
// Wi-Fi connection
// ==========================================

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  unsigned long startTime = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < 20000
  ) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected!");

    Serial.print("Pico W IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Sending OSC to: ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);
  } else {
    Serial.println("Wi-Fi connection failed");
  }
}

// ==========================================
// MPU6050 helpers
// ==========================================

void mpuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

int16_t readWord(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)MPU_ADDR, 2);
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  return (int16_t)((hi << 8) | lo);
}

void readAccelGyro(float out[6]) {
  out[0] = readWord(ACCEL_XOUT_H) / ACCEL_SCALE;
  out[1] = readWord(ACCEL_XOUT_H + 2) / ACCEL_SCALE;
  out[2] = readWord(ACCEL_XOUT_H + 4) / ACCEL_SCALE;
  out[3] = readWord(GYRO_XOUT_H) / GYRO_SCALE;
  out[4] = readWord(GYRO_XOUT_H + 2) / GYRO_SCALE;
  out[5] = readWord(GYRO_XOUT_H + 4) / GYRO_SCALE;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin();  // I2C0: SDA=GP4, SCL=GP5
  mpuWrite(PWR_MGMT_1, 0x00);  // 唤醒MPU6050
  delay(100);

  connectWiFi();
}

void loop() {
  // Wi-Fi断了自动重连
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(1000);
    return;
  }

  unsigned long currentTime = millis();

  if (currentTime - previousSendTime >= sendInterval) {
    previousSendTime = currentTime;

    float v[6];
    readAccelGyro(v);

    Serial.print("ax="); Serial.print(v[0], 2);
    Serial.print(" ay="); Serial.print(v[1], 2);
    Serial.print(" az="); Serial.print(v[2], 2);
    Serial.print(" gx="); Serial.print(v[3], 1);
    Serial.print(" gy="); Serial.print(v[4], 1);
    Serial.print(" gz="); Serial.println(v[5], 1);

    // ======================================
    // Send OSC: /accel ax ay az, /gyro gx gy gz
    // ======================================

    OSCMessage accelMsg("/accel");
    accelMsg.add(v[0]);
    accelMsg.add(v[1]);
    accelMsg.add(v[2]);

    udp.beginPacket(host, port);
    accelMsg.send(udp);
    udp.endPacket();
    accelMsg.empty();

    OSCMessage gyroMsg("/gyro");
    gyroMsg.add(v[3]);
    gyroMsg.add(v[4]);
    gyroMsg.add(v[5]);

    udp.beginPacket(host, port);
    gyroMsg.send(udp);
    udp.endPacket();
    gyroMsg.empty();
  }
}
