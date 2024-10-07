#include <TinyGPSPlus.h>
#include <Wire.h>
#include <SPI.h>
#include <ArduinoBLE.h>

static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;

// BLE用変数
BLEAdvertisingData advData;

// BLE製造元データ
const uint8_t manufactData[4] = {0x01, 0x02, 0x03, 0x04};

// BLEデータフォーマット
byte serviceData[17] = {0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00};

void setup() {
  Serial.begin(115200); // デバッグ用シリアルモニタ
  Serial1.begin(GPSBaud); // ハードウェアシリアルポート1を使用

  pinMode(LED_BUILTIN, OUTPUT);

  if (!BLE.begin()) {
    Serial.println("BLEの初期化に失敗しました");
    while (1);
  }

  advData.setManufacturerData(0xFFFF, manufactData, sizeof(manufactData));
  BLE.setAdvertisingData(advData);
  BLE.advertise();
}

void loop() {
  while (Serial1.available() > 0) {
    if (gps.encode(Serial1.read())) {
      displayInfo();
    }
  }

  // GPSが見つからない場合のチェック
  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println(F("No GPS detected: check wiring."));
    while (true);
  }
  digitalWrite(LED_BUILTIN, LOW);
}

void displayInfo() {
  Serial.print(F("Location: "));
  if (gps.location.isValid()) {
    // 緯度と経度を整数に変換
    long lat = static_cast<long>(gps.location.lat() * 1e6);
    long lng = static_cast<long>(gps.location.lng() * 1e6);
    
    // 高度と速度を整数に変換
    long alt = static_cast<long>(gps.altitude.meters() * 100); // 高度を100倍して2桁まで整数に
    long spd = static_cast<long>(gps.speed.kmph() * 100);      // 速度を100倍して2桁まで整数に

    // 緯度をserviceDataに追加
    serviceData[1] = (lat >> 24) & 0xFF; // 緯度の上位バイト
    serviceData[2] = (lat >> 16) & 0xFF; // 緯度の次のバイト
    serviceData[3] = (lat >> 8) & 0xFF;  // 緯度の次のバイト
    serviceData[4] = lat & 0xFF;         // 緯度の下位バイト

    // 経度をserviceDataに追加
    serviceData[5] = (lng >> 24) & 0xFF; // 経度の上位バイト
    serviceData[6] = (lng >> 16) & 0xFF; // 経度の次のバイト
    serviceData[7] = (lng >> 8) & 0xFF;  // 経度の次のバイト
    serviceData[8] = lng & 0xFF;         // 経度の下位バイト

    // 高度をserviceDataに追加
    serviceData[9]  = (alt >> 24) & 0xFF; // 高度の上位バイト
    serviceData[10] = (alt >> 16) & 0xFF; // 高度の次のバイト
    serviceData[11] = (alt >> 8) & 0xFF;  // 高度の次のバイト
    serviceData[12] = alt & 0xFF;         // 高度の下位バイト

    // 速度をserviceDataに追加
    serviceData[13] = (spd >> 24) & 0xFF; // 速度の上位バイト
    serviceData[14] = (spd >> 16) & 0xFF; // 速度の次のバイト
    serviceData[15] = (spd >> 8) & 0xFF;  // 速度の次のバイト
    serviceData[16] = spd & 0xFF;         // 速度の下位バイト

    Serial.print(lat); // 緯度をシリアル出力
    Serial.print(F(","));
    Serial.print(lng); // 経度をシリアル出力
    Serial.print(F(","));
    Serial.print(alt); // 高度をシリアル出力
    Serial.print(F(","));
    Serial.println(spd); // 速度をシリアル出力
  } else {
    Serial.print(F("INVALID"));
  }
  Serial.println();

  // データをBLEでアドバタイズ
  advData.setAdvertisedServiceData(0xfcbe, serviceData, sizeof(serviceData));
  BLE.setAdvertisingData(advData);
  BLE.advertise();
}
