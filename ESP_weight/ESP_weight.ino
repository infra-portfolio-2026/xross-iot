#include <Arduino.h>
#include "HX711.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.h"

// ===== HX711 핀 =====
#define DT 5
#define SCK 18
HX711 scale;

// ===== 보정값 (나중에 캘리브레이션 추천) =====
long offset = 400017;
float scaleFactor = 100000;

// ===== 필터 =====
#define FILTER_SIZE 3
float weightBuffer[FILTER_SIZE];
int filterIndex = 0;

WiFiClientSecure espClient;
PubSubClient client(espClient);

// ===== 변화 감지 & 빠른 전송 (0.7초 이내) =====
float lastWeight = 0;
float lastSentWeight = -1;
int stableCount = 0;
bool initialized = false;

bool changePending = false;
unsigned long changeDetectedAt = 0;

const float CHANGE_THRESHOLD = 0.02;        // 20g 이상 변화 감지
const float STABLE_THRESHOLD = 0.02;        // 20g 이내 = 미세 안정
const int QUICK_STABLE_REQUIRED = 2;        // 짧은 안정 확인 (2회 연속)
const unsigned long SEND_MAX_WAIT_MS = 700; // 변화 감지 후 최대 0.7초 내 전송

// ===== MQTT 재연결 =====
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry...");
      delay(2000);
    }
  }
}

// ===== MQTT 전송 =====
void sendWeightMQTT(float w) {
  String payload = "{\"weight\":" + String(w, 2) + "}";

  Serial.print("Publishing: ");
  Serial.println(payload);

  client.publish(MQTT_TOPIC, payload.c_str());
}

void setup() {
  Serial.begin(115200);
  scale.begin(DT, SCK);

  scale.set_gain(128);
  delay(500);

  for (int i = 0; i < FILTER_SIZE; i++) weightBuffer[i] = 0;

  Serial.println("HX711 Fast Change Send (max 0.7s)");

  // WiFi 연결
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.println(WiFi.localIP());

  // MQTT (HiveMQ Cloud TLS)
  espClient.setInsecure();
  client.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // ===== HX711 읽기 (빠른 응답을 위해 2회 평균) =====
  long raw = scale.read_average(2);
  float weight = (raw - offset) / scaleFactor;
  if (weight < 0) weight = 0;

  // ===== 이동 평균 =====
  weightBuffer[filterIndex] = weight;
  filterIndex = (filterIndex + 1) % FILTER_SIZE;

  float sum = 0;
  for (int i = 0; i < FILTER_SIZE; i++) sum += weightBuffer[i];
  float weightAvg = sum / FILTER_SIZE;

  // ===== 출력 =====
  Serial.print("Raw: ");
  Serial.print(raw);
  Serial.print("  Weight: ");
  Serial.println(weightAvg, 3);

  // 부팅 직후 기준값만 잡고 전송하지 않음
  if (!initialized) {
    lastSentWeight = weightAvg;
    lastWeight = weightAvg;
    initialized = true;
    return;
  }

  // ===== 미세 안정 판단 =====
  if (abs(weightAvg - lastWeight) < STABLE_THRESHOLD) {
    stableCount++;
  } else {
    stableCount = 0;
  }

  // ===== 마지막 전송값 대비 변화 감지 =====
  if (abs(weightAvg - lastSentWeight) > CHANGE_THRESHOLD) {
    if (!changePending) {
      changePending = true;
      changeDetectedAt = millis();
      Serial.println("=== CHANGE DETECTED ===");
    }
  }

  // ===== 짧게 안정되면 즉시 전송, 아니면 0.7초 후 전송 =====
  if (changePending) {
    unsigned long elapsed = millis() - changeDetectedAt;
    bool quickStable = stableCount >= QUICK_STABLE_REQUIRED;
    bool timedOut = elapsed >= SEND_MAX_WAIT_MS;

    if (quickStable || timedOut) {
      sendWeightMQTT(weightAvg);
      lastSentWeight = weightAvg;
      changePending = false;
      stableCount = 0;

      Serial.println(quickStable ? "=== SENT (QUICK STABLE) ==="
                                 : "=== SENT (0.7s TIMEOUT) ===");
    }
  }

  lastWeight = weightAvg;
}