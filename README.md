# XROSS IoT Firmware

무인점포 미결제 탐지 시스템의 무게 센서와 경고음 장치 펌웨어입니다.

## 주요 내용

- `ESP_weight`: HX711 무게 센서 값을 보정해 MQTT over TLS로 전송
- `ESP_sound`: 미결제 감지 메시지를 받아 매장 경고음 출력
- Wi-Fi·MQTT 접속 정보는 `secrets.example.h`를 복사해 채우는 방식

## 기술

ESP32 · HX711 · MQTT over TLS · Arduino


> 블라인드 채용 제출용 사본입니다. 이름·기관명·비밀값은 치환하거나 제외했고, 커밋 기록은 새로 만들었습니다. 치환된 식별자 때문에 그대로 빌드되지 않을 수 있습니다.
