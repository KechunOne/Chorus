#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <Adafruit_NeoPixel.h>

// ==========================================
// Wi-Fi configuration
// ==========================================

const char* ssid = "GL-AXT1800-fb5";
const char* password = "2TD9YAPC5S";

// ==========================================
// OSC configuration
// ==========================================

const int listenPort = 9000;

WiFiUDP udp;

// ==========================================
// LED Strip configuration
// ==========================================

#define LED_PIN    16
#define NUM_LEDS   41
#define BRIGHTNESS 80
#define SEGMENT_LEN 15

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

uint8_t colorR = 0;
uint8_t colorG = 0;
uint8_t colorB = 255;
int targetHead = 0;
float currentHead = 0;
bool flowing = false;

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
  } else {
    Serial.println("Wi-Fi connection failed");
  }
}

// ==========================================
// OSC handlers
// ==========================================

// /led/color r g b
void handleColor(OSCMessage &msg) {
  colorR = msg.getInt(0);
  colorG = msg.getInt(1);
  colorB = msg.getInt(2);
}

// /led/pos n — 光段位置 (0 ~ NUM_LEDS-1)
void handlePos(OSCMessage &msg) {
  targetHead = msg.getInt(0);
  if (targetHead < 0) targetHead = 0;
  if (targetHead >= NUM_LEDS) targetHead = NUM_LEDS - 1;
  flowing = false;
}

// /led/flow n — 1=开始自动流动, 0=停止
void handleFlow(OSCMessage &msg) {
  flowing = msg.getInt(0) > 0;
}

// /led/brightness n
void handleBrightness(OSCMessage &msg) {
  strip.setBrightness(msg.getInt(0));
}

// ==========================================
// sin8 替代
// ==========================================

uint8_t sin8_C(int j, int len) {
  float angle = (float)j / (float)len * 3.14159;
  return (uint8_t)(sin(angle) * 255);
}

// ==========================================
// Setup & Loop
// ==========================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  connectWiFi();
  udp.begin(listenPort);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(1000);
    return;
  }

  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    OSCMessage msg;
    while (udp.available() > 0) {
      msg.fill(udp.read());
    }
    if (!msg.hasError()) {
      msg.dispatch("/led/color", handleColor);
      msg.dispatch("/led/pos", handlePos);
      msg.dispatch("/led/flow", handleFlow);
      msg.dispatch("/led/brightness", handleBrightness);
    }
  }

  if (flowing) {
    currentHead += 0.5;
    if (currentHead >= NUM_LEDS) currentHead = 0;
  } else {
    // 平滑过渡到目标位置
    currentHead += ((float)targetHead - currentHead) * 0.7;
  }

  int head = (int)currentHead;

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }

  for (int j = 0; j < SEGMENT_LEN; j++) {
    int pos = head + j;
    if (pos >= NUM_LEDS) pos -= NUM_LEDS;
    uint8_t fade = sin8_C(j, SEGMENT_LEN);
    uint8_t r = (uint16_t)colorR * fade / 255;
    uint8_t g = (uint16_t)colorG * fade / 255;
    uint8_t b = (uint16_t)colorB * fade / 255;
    strip.setPixelColor(pos, strip.Color(r, g, b));
  }

  strip.show();
  delay(30);
}
