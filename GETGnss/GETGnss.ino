#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <Arduino.h>
#include <TFT_eSPI.h> // TFTライブラリのインクルード

// TFTインスタンスの作成
TFT_eSPI tft = TFT_eSPI(); // TFTオブジェクトの作成

// Manufacturer Dataのマッチングするデータ
const uint8_t targetManufacturerData[] = {0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04};
const size_t manufacturerDataLength = sizeof(targetManufacturerData);

// BLEスキャンの時間
const int scanTime = 5; // スキャン時間（秒）

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Manufacturer Dataが存在するか確認
    if (advertisedDevice.haveManufacturerData()) {
      std::string manufacturerData = advertisedDevice.getManufacturerData();

      // Manufacturer Dataのサイズが一致しているか確認
      if (manufacturerData.length() == manufacturerDataLength) {
        // Manufacturer Dataがターゲットのデータと一致するか確認
        bool match = true;
        for (int i = 0; i < manufacturerDataLength; i++) {
          if ((uint8_t)manufacturerData[i] != targetManufacturerData[i]) {
            match = false;
            break;
          }
        }

        // マッチした場合、rawデータを表示
        if (match) {
          Serial.println("Target Manufacturer Data found!");

          // Raw advertisement payloadを取得
          uint8_t* payload = advertisedDevice.getPayload();
          int payloadLength = advertisedDevice.getPayloadLength();

          // rawデータを表示
          Serial.print("Raw Data: ");
          for (int i = 0; i < payloadLength; i++) {
            Serial.print(String(payload[i], HEX));
            Serial.print(" ");
          }
          Serial.println();

          // データを表示するための変数
          String I_value = "";
          String K_value = "";

          // 受信したデータを解析
          for (int i = 0; i < payloadLength; i++) {
            // Iを14個目から17個目のデータを取得
            if (i >= 13 && i <= 16) {
              I_value += String(payload[i], HEX); // 空白なしで追加
            }

            // Kを18個目から21個目のデータを取得
            if (i >= 17 && i <= 20) {
              K_value += String(payload[i], HEX); // 空白なしで追加
            }
          }

          // 10進数に変換 (空白なし)
          unsigned long I_decimal = strtoul(I_value.c_str(), nullptr, 16);
          unsigned long K_decimal = strtoul(K_value.c_str(), nullptr, 16);

          // I と K のフォーマットで表示
          Serial.printf("raw lat: %s lon: %s\n", I_value.c_str(), K_value.c_str());
          Serial.printf("lat: %lu\n", I_decimal);
          Serial.printf("lon: %lu\n", K_decimal);

          // TFTディスプレイに表示
          tft.fillScreen(TFT_BLACK); // 背景を黒に設定
          tft.setTextColor(TFT_WHITE); // 文字色を白に設定
          tft.setTextSize(2); // 文字サイズを設定
          tft.setCursor(40, 50); // 表示位置を設定
          tft.printf("lat: %lu\n", I_decimal); // Iの値を表示
          tft.setCursor(40, 100); // 表示位置を変更
          tft.printf("lon: %lu\n", K_decimal); // Kの値を表示
        }
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE client...");

  // TFTの初期化
  tft.init();
  tft.setRotation(1); // ディスプレイの回転を設定（必要に応じて変更）
  tft.fillScreen(TFT_BLACK); // 背景を黒に設定

  BLEDevice::init("XIAOESP32S3_Client");
}

void loop() {
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(scanTime); // スキャンの時間

  // スキャン後、1秒待機
  delay(1000);

  // スキャンが完了したら、BLEデバイスのスキャンを続ける
  pBLEScan->stop();
}
