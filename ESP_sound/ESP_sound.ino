#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include "secrets.h"

// ===== MQTT (HiveMQ Cloud TLS) =====
WiFiClientSecure espClient;
PubSubClient client(espClient);

// ===== DFPlayer =====
HardwareSerial mySerial(2);
DFRobotDFPlayerMini myDFPlayer;

// ===== MQTT 메시지 수신 =====
void callback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  // payload -> String 변환
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  message.trim();

  Serial.print("Received: ");
  Serial.println(message);

  // 문자열 -> 숫자
  int trackNumber = message.toInt();

  // 유효성 검사
  if (trackNumber <= 0) {

    Serial.println("❌ 잘못된 트랙 번호");
    return;
  }

  Serial.print("🔊 MP3 재생: ");
  Serial.println(trackNumber);

  // MP3 재생
  myDFPlayer.play(trackNumber);
}

// ===== MQTT 재연결 =====
void reconnectMQTT() {

  while (!client.connected()) {

    Serial.print("Connecting to MQTT...");

    String clientId =
      "ESP32-PLAYER-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {

      Serial.println("connected ✅");

      // 토픽 구독
      client.subscribe(MQTT_TOPIC);

      Serial.print("Subscribed to: ");
      Serial.println(MQTT_TOPIC);

    } else {

      Serial.print("failed, rc=");
      Serial.print(client.state());

      Serial.println(" retry in 2 sec");

      delay(2000);
    }
  }
}

void setup() {

  Serial.begin(115200);

  // ===== DFPlayer =====
  mySerial.begin(9600, SERIAL_8N1, 16, 17);

  if (!myDFPlayer.begin(mySerial)) {

    Serial.println("DFPlayer 연결 실패 ❌");

    while (true);
  }

  Serial.println("DFPlayer 연결 성공 ✅");

  myDFPlayer.volume(25);

  // ===== WiFi =====
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected ✅");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // ===== MQTT =====
  espClient.setInsecure();
  client.setServer(MQTT_SERVER, MQTT_PORT);

  client.setCallback(callback);

  Serial.println("MQTT Ready ✅");
}

void loop() {

  // MQTT 연결 확인
  if (!client.connected()) {

    reconnectMQTT();
  }

  // MQTT 메시지 처리
  client.loop();
}