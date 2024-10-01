//計算式 https://www.trail-note.net/tech/coordinate/
#include <BLEDevice.h>  
#include <BLEUtils.h>  
#include <BLEClient.h>  
#include <Arduino.h>  
//#include <TFT_eSPI.h>
#include <SD.h>
#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>

// TFTインスタンスの作成  
//TFT_eSPI tft = TFT_eSPI();

static LGFX lcd;

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
          String Blat = "";  
          String Blon = "";  

          // 受信したデータを解析  
          for (int i = 0; i < payloadLength; i++) {  
            // Iを14個目から17個目のデータを取得  
            if (i >= 13 && i <= 16) {  
              Blat += String(payload[i], HEX);
            }  

            // Kを18個目から21個目のデータを取得  
            if (i >= 17 && i <= 20) {  
              Blon += String(payload[i], HEX);
            }  
          }  

          // 16進数から10進数に変換  
          unsigned long tlat = strtoul(Blat.c_str(), nullptr, 16);  
          unsigned long tlon = strtoul(Blon.c_str(), nullptr, 16);  

          // 緯度・経度を浮動小数点数にする)
          float la = tlat / 1000000.0;
          float ln = tlon / 1000000.0;
          Serial.printf("lat: %.6f, lon: %.6f\n", la, ln); 

          // タイル座標の計算 
          int z = 16;  // ズームレベル 
          double L = 85.05112878;
          //緯度経度→ピクセル座標の変換計算
          double px = int(pow(2.0, z + 7.0) * ((ln / 180.0) + 1.0));
          double py = int(pow(2.0, z + 7.0) * (-1 * atanh(sin(PI * la / 180.0)) + atanh(sin(PI * L / 180.0))) / PI);
          //ピクセル座標→タイル座標の変換計算
          int tx = px / 256;
          int ty = py / 256;
          Serial.printf("tx: %d, ty: %d\n", tx, ty); 
          //タイル画像の中の座標を計算
          int x = int(px) % 256;
          int y = int(py) % 256;

          // SDカード内のパスに基づいて画像を読み込む
          char imagePath[64];
          sprintf(imagePath, "/images/std/16/%d/%d.png", tx, ty);
          
          //画像9枚のファイルアドレスを用意
          String filename[9];
          filename[0] = String("/images/std/" + String(z) + "/" + String(tx - 1) + "/" + String(ty-1) + ".png");
          filename[1] = String("/images/std/" + String(z) + "/" + String(tx) + "/" + String(ty-1) + ".png");
          filename[2] = String("/images/std/" + String(z) + "/" + String(tx + 1) + "/" + String(ty-1) + ".png");
          filename[3] = String("/images/std/" + String(z) + "/" + String(tx - 1) + "/" + String(ty) + ".png");
          filename[4] = String("/images/std/" + String(z) + "/" + String(tx) + "/" + String(ty) + ".png");
          filename[5] = String("/images/std/" + String(z) + "/" + String(tx + 1) + "/" + String(ty) + ".png");
          filename[6] = String("/images/std/" + String(z) + "/" + String(tx - 1) + "/" + String(ty+1) + ".png");
          filename[7] = String("/images/std/" + String(z) + "/" + String(tx) + "/" + String(ty+1) + ".png");
          filename[8] = String("/images/std/" + String(z) + "/" + String(tx + 1) + "/" + String(ty+1) + ".png");

          /*
          画像9枚の並びはこのようになっている
          [0][1][2]
          [3][4][5]
          [6][7][8]
          */

          //Stringからchar配列に変換
          for (int i = 0; i < 9; i++)
          {
            int str_len = filename[i].length() + 1;
            char file[9][str_len];
            filename[i].toCharArray(file[i], str_len);
          }

          //filename[4]を中心として画像を描画
          int mainx = -1 * (x-160);
          int mainy = -1 * (y-120);
          lcd.drawJpgFile(SD, filename[4], mainx, mainy);
          //他8枚の画像を描画

          if (mainx > 0 && mainy > 0)
          {
            lcd.drawJpgFile(SD, filename[0], mainx - 256, mainy-256);
          }
          if (mainy > 0)
          {
            lcd.drawJpgFile(SD, filename[1], mainx , mainy-256);
          }
          if (mainx + 256 < 320 && mainy >0)
          {
            lcd.drawJpgFile(SD, filename[2], mainx + 256, mainy - 256);
          }


          if (mainx > 0)
          {
            lcd.drawJpgFile(SD, filename[3], mainx - 256, mainy);
          }
          if (mainx + 256 < 320)
          {
            lcd.drawJpgFile(SD, filename[5], mainx + 256, mainy);
          }


          if (mainx > 0 && mainy < -26)
          {
            lcd.drawJpgFile(SD, filename[6], mainx - 256, mainy + 256);
          }
          if (mainy < -26)
          {
            lcd.drawJpgFile(SD, filename[7], mainx, mainy + 256);
          }
          if (mainx + 256 < 320 && mainy < -26)
          {
            lcd.drawJpgFile(SD, filename[8], mainx + 256, mainy + 256);
          }

          //中心に印をつける
          lcd.fillCircle(160, 120, 8, TFT_CYAN);
          lcd.fillCircle(160, 120, 5, TFT_BLUE);

          delay(1000);
        }  
      }  
    }  
  }  
};  

void setup() {  
  Serial.begin(115200);  
  Serial.println("Starting BLE client...");  

  // // TFTの初期化  
  // tft.init();  
  // tft.setRotation(1); // ディスプレイの回転を設定（必要に応じて変更）  
  // tft.fillScreen(TFT_BLACK); // 背景を黒に設定  

  // SDカードの初期化
  if (!SD.begin()) {
    Serial.println("SD Card initialization failed!");
    return;
  }

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


