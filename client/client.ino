#include <BLEDevice.h>     
#include <BLEUtils.h>     
#include <BLEClient.h>     
#include <Arduino.h>     
#include <TFT_eSPI.h>   
#include <SD.h>   
#include <lvgl.h>   
#include <PNGdec.h>   // PNGdecライブラリのインクルード  
#define USE_TFT_ESPI_LIBRARY   
#include "lv_xiao_round_screen.h"  

//TFT_eSPI tft = TFT_eSPI();  // TFTインスタンスの作成   
PNG png;  // PNGdecのインスタンス 

// Manufacturer Dataのマッチングするデータ     
const uint8_t targetManufacturerData[] = {0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04};     
const size_t manufacturerDataLength = sizeof(targetManufacturerData);     

// BLEスキャンの時間     
const int scanTime = 5; // スキャン時間（秒）     

// 画像を描画するための変数
int lastTx = -1;
int lastTy = -1;

// ファイルを開くためのコールバック関数 
void *PNG_open(const char *filename, int32_t *size) { 
  File *pngFile = new File(SD.open(filename)); 
  if (!pngFile) { 
    return nullptr; 
  } 
  *size = pngFile->size(); 
  return (void *)pngFile; 
} 

// ファイルを閉じるためのコールバック関数 
void PNG_close(void *handle) { 
  File *pngFile = (File *)handle; 
  if (pngFile) { 
    pngFile->close(); 
    delete pngFile; 
  } 
} 

// 読み込むためのコールバック関数 
int32_t PNG_read(PNGFILE *handle, uint8_t *buffer, int32_t length) { 
  File *pngFile = (File *)handle; 
  if (!pngFile) { 
    return 0; 
  } 
  return pngFile->read(buffer, length); 
} 

// シーク位置を設定するコールバック関数 
int32_t PNG_seek(PNGFILE *handle, int32_t position) { 
  File *pngFile = (File *)handle; 
  if (!pngFile) { 
    return 0; 
  } 
  return pngFile->seek(position); 
} 

// PNGを描画する関数 
void drawPngFile(const char *filename, int x, int y) {  
  // PNG画像をデコードし、TFT_eSPIに描画  
  int16_t rc = png.open(filename, PNG_open, PNG_close, PNG_read, PNG_seek, PNGDraw);  
  if (rc != PNG_SUCCESS) {  
    Serial.printf("PNG open failed with rc: %d\n", rc);  
    return;  
  }  

  // PNGデコードプロセス  
  rc = png.decode(NULL, 0); // x, y座標に描画  
  if (rc != PNG_SUCCESS) {  
    Serial.printf("PNG decode failed with rc: %d\n", rc);  
  } 
} 

// デコードしたPNGデータを描画するコールバック関数 
void PNGDraw(PNGDRAW *pDraw) {  
  uint8_t lineBuffer[320]; // 描画用のラインバッファ (画面横幅に合わせて設定)  
  tft.drawBitmap(0, 0, lineBuffer, pDraw->iWidth, 1, TFT_WHITE);  
} 

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {     
  void onResult(BLEAdvertisedDevice advertisedDevice) {     
    if (advertisedDevice.haveManufacturerData()) {     
      std::string manufacturerData = advertisedDevice.getManufacturerData();     

      if (manufacturerData.length() == manufacturerDataLength) {     
        bool match = true;     
        for (int i = 0; i < manufacturerDataLength; i++) {     
          if ((uint8_t)manufacturerData[i] != targetManufacturerData[i]) {     
            match = false;     
            break;     
          }     
        }     

        if (match) {     
          Serial.println("Target Manufacturer Data found!");     
          uint8_t* payload = advertisedDevice.getPayload();     
          int payloadLength = advertisedDevice.getPayloadLength();     

          String Blat = "";     
          String Blon = "";     

          for (int i = 0; i < payloadLength; i++) {     
            if (i >= 13 && i <= 16) {     
              Blat += String(payload[i], HEX);   
            }     
            if (i >= 17 && i <= 20) {     
              Blon += String(payload[i], HEX);   
            }     
          }     

          unsigned long tlat = strtoul(Blat.c_str(), nullptr, 16);     
          unsigned long tlon = strtoul(Blon.c_str(), nullptr, 16);     

          float la = tlat / 1000000.0;   
          float ln = tlon / 1000000.0;   
          Serial.printf("lat: %.6f, lon: %.6f\n", la, ln);    

          int z = 16;   
          double L = 85.05112878;   
          double px = int(pow(2.0, z + 7.0) * ((ln / 180.0) + 1.0));   
          double py = int(pow(2.0, z + 7.0) * (-1 * atanh(sin(PI * la / 180.0)) + atanh(sin(PI * L / 180.0))) / PI);   
          int tx = px / 256;   
          int ty = py / 256;   
          Serial.printf("tx: %d, ty: %d\n", tx, ty);    

          int x = int(px) % 256;   
          int y = int(py) % 256;   

          // 新しいタイルの位置が最後の位置と異なる場合のみ描画
          if (tx != lastTx || ty != lastTy) {
            lastTx = tx; // 更新
            lastTy = ty; // 更新

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

            int mainx = -1 * (x - 160);   
            int mainy = -1 * (y - 120);   
            drawPngFile(filename[4].c_str(), mainx, mainy);  // 中心の画像をPNGで描画 

            if (mainx > 0 && mainy > 0) {   
              drawPngFile(filename[0].c_str(), mainx - 256, mainy - 256);   
            }   
            if (mainy > 0) {   
              drawPngFile(filename[1].c_str(), mainx , mainy - 256);   
            }   
            if (mainx + 256 < 320 && mainy >0) {   
              drawPngFile(filename[2].c_str(), mainx + 256, mainy - 256);   
            }   

            if (mainx > 0) {   
              drawPngFile(filename[3].c_str(), mainx - 256, mainy );   
            }   
            if (mainx + 256 < 320) {   
              drawPngFile(filename[5].c_str(), mainx + 256, mainy );   
            }   
            if (mainy + 256 < 240) {   
              drawPngFile(filename[7].c_str(), mainx , mainy + 256);   
            }   
            if (mainx > 0 && mainy + 256 < 240) {   
              drawPngFile(filename[6].c_str(), mainx - 256, mainy + 256);   
            }   
            if (mainx + 256 < 320 && mainy + 256 < 240) {   
              drawPngFile(filename[8].c_str(), mainx + 256, mainy + 256);   
            }   
          }
        }     
      }     
    }     
  }     
};     

void setup() {  
  Serial.begin(115200);   
  BLEDevice::init("");     
  BLEScan *pBLEScan = BLEDevice::getScan(); // create new scan   
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());     
  pBLEScan->setInterval(100);     
  pBLEScan->setWindow(99);     
  pBLEScan->setActiveScan(true);     
  pBLEScan->start(scanTime, false);   

  // TFTの初期化  
  tft.begin();   
  tft.setRotation(1);  
  tft.fillScreen(TFT_BLACK);   

  // SDカードの初期化  
  if (!SD.begin()) {  
    Serial.println("Card Mount Failed");  
    return;  
  }  
} 

void loop() {  
  // スキャンを再開  
  BLEDevice::getScan()->start(scanTime, false);   
} 
