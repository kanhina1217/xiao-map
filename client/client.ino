#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <queue>
#include <SD.h>
 
// display変数 
#define LGFX_USE_V1 
#include <LovyanGFX.hpp> 

#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

bool UseGPS = true;

// RTCライブラリ
#include "I2C_BM8563.h"
I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);

// BLEから取得したデータ
struct BLEData {
  float lat;  // 緯度
  float lon;  // 経度
};

// 計算結果のデータ
struct DisplayData {
  int sx;  // ずらすx座標
  int sy;  // ずらすy座標
  int x;  // 現在のx座標
  int y;  // 現在のy座標
  int tx;  // タイルx座標
  int ty;  // タイルy座標
};

// 画像タイルの位置を記録する変数 
int lastTx = -1; 
int lastTy = -1; 

// touch用変数
float currentLat = 0.0;  // 現在の緯度
float currentLon = 0.0;  // 現在の経度
float bleLat = 0.0;
float bleLon = 0.0;
float bleAlt = 0.0;
float bleSpd = 0.0;

// キューとミューテックスの宣言
std::queue<BLEData> bleDataQueue;
std::queue<DisplayData> displayDataQueue;
SemaphoreHandle_t bleDataMutex;
SemaphoreHandle_t displayDataMutex;

// LovyanGFXのインスタンス
struct Touch_XiaoRound : public lgfx::v1::ITouch { 
  Touch_XiaoRound() { 
    _cfg.x_min = _cfg.y_min = 0; 
    _cfg.x_max = _cfg.y_max = 239; 
    _cfg.i2c_addr = 0x2e; 
  } 
  bool init() override { 
    if (isSPI()) { 
      return false; 
    } 
    if (_cfg.pin_int >= 0) { 
      lgfx::pinMode(_cfg.pin_int, lgfx::v1::pin_mode_t::input_pullup); 
    } 
    return lgfx::i2c::init(_cfg.i2c_port, _cfg.pin_sda, _cfg.pin_scl).has_value(); 
  } 
  void wakeup() override {} 
  void sleep() override {} 
 
  uint_fast8_t getTouchRaw(lgfx::v1::touch_point_t *tp, uint_fast8_t count) override { 
    tp[0].size = 0; 
    tp[0].id = 0; 
    if (_cfg.pin_int < 0) { 
      return 0; 
    } 
    if ((bool)lgfx::gpio_in(_cfg.pin_int)) { 
      ::delay(10); 
      if ((bool)lgfx::gpio_in(_cfg.pin_int)) { 
        return 0; 
      } 
    } 
    uint8_t buf[5]; 
    if (!lgfx::i2c::transactionRead(_cfg.i2c_port, _cfg.i2c_addr, buf, 5, _cfg.freq).has_value()) { 
      return 0; 
    } 
    if (buf[0] != 1) { 
      return 0; 
    } 
    tp[0].x = buf[2]; 
    tp[0].y = buf[4]; 
    tp[0].size = 1; 
    return 1; 
  } 
}; 
 
class XiaoRoundDisplay : public lgfx::LGFX_Device { 
  lgfx::Panel_GC9A01 _panel; 
  lgfx::Bus_SPI _bus; 
  lgfx::Light_PWM _light; 
  Touch_XiaoRound _touch; 
 
 public: 
  XiaoRoundDisplay() { 
    auto bus_cfg = _bus.config(); 
    bus_cfg.spi_host = SPI2_HOST; // for XIAO RP2040     
    bus_cfg.spi_mode = 0; 
    bus_cfg.freq_write = 40000000; 
    bus_cfg.freq_read  = 20000000; 
    bus_cfg.pin_sclk = D8; // for XIAO RP2040 
    bus_cfg.pin_mosi = D10; // for XIAO RP2040 
    bus_cfg.pin_miso = D9; // for XIAO RP2040 
    bus_cfg.pin_dc   = D3; // for XIAO RP2040 
    _bus.config(bus_cfg); 
    _panel.setBus(&_bus); 
 
    auto panel_cfg = _panel.config(); 
    panel_cfg.pin_cs = D1; // for XIAO RP2040 
    panel_cfg.pin_rst = -1; 
    panel_cfg.pin_busy = -1; 
    panel_cfg.memory_width = 240; 
    panel_cfg.memory_height = 240; 
    panel_cfg.panel_width = 240; 
    panel_cfg.panel_height = 240; 
    panel_cfg.offset_x = 0; 
    panel_cfg.offset_y = 0; 
    panel_cfg.offset_rotation = 0; 
    panel_cfg.dummy_read_pixel = 8; 
    panel_cfg.dummy_read_bits  = 1; 
    panel_cfg.readable = false; 
    panel_cfg.invert = true; 
    panel_cfg.rgb_order = false; 
    panel_cfg.dlen_16bit = false; 
    panel_cfg.bus_shared =  true; 
    _panel.config(panel_cfg); 
 
    auto light_cfg = _light.config(); 
    light_cfg.pin_bl = D6; // for XIAO RP2040 
    light_cfg.invert = false; 
    light_cfg.freq = 44100; 
    light_cfg.pwm_channel = 7; 
    _light.config(light_cfg); 
    _panel.setLight(&_light); 
 
    auto touch_cfg = _touch.config();  
    touch_cfg.pin_int = D7; // for XIAO RP2040 
    touch_cfg.i2c_port = 1; // for XIAO RP2040 
    touch_cfg.pin_sda  = D4; // for XIAO RP2040 
    touch_cfg.pin_scl  = D5; // for XIAO RP2040 
    touch_cfg.freq = 400000; 
 
    _touch.config(touch_cfg); 
    _panel.setTouch(&_touch); 
 
    setPanel(&_panel); 
  } 
} display; 
 
LGFX_Sprite record(&display); 

//関数定義
void write() { 
  display.startWrite(); 
  record.pushSprite(0,0); 
  display.endWrite(); 
  display.waitDisplay(); 
} 

void batterycheck() {
  int32_t mvolts = 0;
  for(int8_t i=0; i<20; i++){
    mvolts += analogReadMilliVolts(D0);
  }
  mvolts /= 20;
  int32_t level = (mvolts - 1480) * 100 / 570; // 1480 ~ 2050
  level = (level<0) ? 0 : ((level>100) ? 100 : level); 

  int fillper = map(level, 0, 100, 0, 26);

  display.fillRoundRect(170, 40, fillper, 11, 2, TFT_GREEN);
  display.drawRoundRect(170, 40, 26, 11, 2, TFT_BLACK);

  display.setCursor(174,42);
  display.setTextColor(TFT_BLACK);
  display.setTextSize(1);
  display.printf("%d",level);
  
  Serial.printf("Battery: %d \n",level);
}

void timecheck() {
  I2C_BM8563_DateTypeDef dateStruct;
  I2C_BM8563_TimeTypeDef timeStruct;

  // Get RTC
  rtc.getDate(&dateStruct);
  rtc.getTime(&timeStruct);

  // Print RTC
  Serial.printf("%04d/%02d/%02d %02d:%02d:%02d\n",
              dateStruct.year,
              dateStruct.month,
              dateStruct.date,
              timeStruct.hours,
              timeStruct.minutes,
              timeStruct.seconds
              );
  display.fillRoundRect(80, 40, 80, 12, 2, TFT_WHITE);
  display.setCursor(82 ,42);
  display.setTextColor(TFT_BLACK);
  display.setTextSize(1.2);
  display.printf("%02d/%02d %02d:%02d\n",
              dateStruct.month,
              dateStruct.date,
              timeStruct.hours,
              timeStruct.minutes
              );
}

// ボタンの状態を監視してUseGPSをトグル
void handleButton() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(0);

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    // ボタンが押されたとき
    UseGPS = !UseGPS;
    Serial.printf("UseGPS: %s\n", UseGPS ? "ON" : "OFF");
    currentLat = bleLat;
    currentLon = bleLon;
  }

  lastButtonState = currentButtonState;
}

void writelog() {
  I2C_BM8563_DateTypeDef dateStruct;
  I2C_BM8563_TimeTypeDef timeStruct;

  // Get RTC
  rtc.getDate(&dateStruct);
  rtc.getTime(&timeStruct);

  File logFile = SD.open("/log.txt", FILE_APPEND);
  if (logFile) {
    logFile.print(dateStruct.year);
    logFile.print('/');
    logFile.print(dateStruct.month);
    logFile.print('/');
    logFile.print(dateStruct.date);
    logFile.print(", ");
    logFile.print(timeStruct.hours);
    logFile.print(':');
    logFile.print(timeStruct.minutes);
    logFile.print(':');
    logFile.print(timeStruct.seconds);
    logFile.print(", ");
    logFile.print(bleLat, 6);  // 緯度
    logFile.print(", ");
    logFile.print(bleLon, 6);  // 経度
    logFile.print(", ");
    logFile.print(bleAlt, 2);  // 高度
    logFile.print(", ");
    logFile.print(bleSpd, 2);  // 速度
    logFile.println();

    logFile.close();

    Serial.print(dateStruct.year);
    Serial.print('/');
    Serial.print(dateStruct.month);
    Serial.print('/');
    Serial.print(dateStruct.date);
    Serial.print(", ");
    Serial.print(timeStruct.hours);
    Serial.print(':');
    Serial.print(timeStruct.minutes);
    Serial.print(':');
    Serial.print(timeStruct.seconds);
    Serial.print(", ");
    Serial.print(bleLat, 6);  // 緯度
    Serial.print(", ");
    Serial.print(bleLon, 6);  // 経度
    Serial.print(", ");
    Serial.print(bleAlt, 2);  // 高度
    Serial.print(", ");
    Serial.print(bleSpd, 2);  // 速度
    Serial.println();

    Serial.println("Log file written");
  } else {
    Serial.println("Failed to open log file");
  }
}


// BLEスキャンの設定
BLEScan* pBLEScan;
int scanTime = 5;  // スキャン時間

// Manufacturer Dataのマッチングするデータ 
const uint8_t targetManufacturerData[] = {0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04}; 
const size_t manufacturerDataLength = sizeof(targetManufacturerData); 

//BLE取得クラス
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
          String Balt = "";
          String Bspd = "";
 
          for (int i = 0; i < payloadLength; i++) { 
            if (i >= 13 && i <= 16) { 
              Blat += String(payload[i], HEX); 
            } 
            if (i >= 17 && i <= 20) { 
              Blon += String(payload[i], HEX); 
            } 
            if (i >= 21 && i <= 24) { 
              Balt += String(payload[i], HEX); 
            } 
            if (i >= 25 && i <= 28) { 
              Bspd += String(payload[i], HEX); 
            } 
          } 
 
          unsigned long tlat = strtoul(Blat.c_str(), nullptr, 16); 
          unsigned long tlon = strtoul(Blon.c_str(), nullptr, 16); 
          unsigned long talt = strtoul(Balt.c_str(), nullptr, 16); 
          unsigned long tspd = strtoul(Bspd.c_str(), nullptr, 16); 
 
          float lat = tlat / 1000000.0; 
          float lon = tlon / 1000000.0; 
          float alt = talt / 100.0;
          float spd = tspd / 100.0;

          bleLat = lat;
          bleLon = lon;
          bleAlt = alt;
          bleSpd = spd;

          writelog();

          // BLEデータをキューに追加
          if (UseGPS) {
            BLEData data;
            data.lat = lat;
            data.lon = lon;
            Serial.printf("lat: %.6f, lon: %.6f\n", lat, lon); 
            xSemaphoreTake(bleDataMutex, portMAX_DELAY);
            bleDataQueue.push(data);
            xSemaphoreGive(bleDataMutex);
          }
          if (!UseGPS) {
            
            uint16_t x,y;
            if (display.getTouch(&x, &y)) {
              // スライド量に応じて緯度と経度を更新
              float deltaLon = (x - 120) * 0.0001;  // スライド量に応じた経度の変化量
              float deltaLat = (y - 120) * 0.0001;  // スライド量に応じた緯度の変化量
              currentLon += deltaLon;
              currentLat += deltaLat;
            }

            BLEData data;
            data.lat = currentLat;
            data.lon = currentLon;
            Serial.printf("lat: %.6f, lon: %.6f\n", currentLat, currentLon); 
            xSemaphoreTake(bleDataMutex, portMAX_DELAY);
            bleDataQueue.push(data);
            xSemaphoreGive(bleDataMutex);
          }
        }
      }
    }
  }
};

// BLEデータ取得タスク
void bleDataTask(void *pvParameters) {
  for (;;) {
    pBLEScan = BLEDevice::getScan();  // BLEスキャン開始
    pBLEScan->start(scanTime, false);

    if (UseGPS) {
      Serial.println("Use GPS");
      vTaskDelay(1000 / portTICK_PERIOD_MS);  // 1秒ごとにスキャン
    }
    if (!UseGPS) {
      Serial.println("Not Use GPS");
      vTaskDelay(50 / portTICK_PERIOD_MS);  // 0.05秒ごとにスキャン
    }
  }
}

// 計算タスク
void calculationTask(void *pvParameters) {
  for (;;) {
    // BLEデータキューからデータを取得
    xSemaphoreTake(bleDataMutex, portMAX_DELAY);
    if (!bleDataQueue.empty()) {
      BLEData data = bleDataQueue.front();
      bleDataQueue.pop();
      xSemaphoreGive(bleDataMutex);

      // ピクセル座標
      int z = 16;  // ズームレベル
      double L = 85.05112878;
      double px = int(pow(2.0, z + 7.0) * ((data.lon / 180.0) + 1.0));
      double py = int(pow(2.0, z + 7.0) * (-1 * atanh(sin(PI * data.lat / 180.0)) + atanh(sin(PI * L / 180.0))) / PI);
      // タイル座標
      int tx = px / 256; 
      int ty = py / 256;
      // 位置座標
      int x = int(px) % 256; 
      int y = int(py) % 256; 
      // ずらす座標
      int sx = 120 - x;  // x座標
      int sy = 120 - y;  // y座標


      // 計算結果をキューに追加
      DisplayData displayData;
      displayData.tx = tx;
      displayData.ty = ty;
      displayData.x = x;
      displayData.y = y;
      displayData.sx = sx;
      displayData.sy = sy;

      xSemaphoreTake(displayDataMutex, portMAX_DELAY);
      displayDataQueue.push(displayData);
      xSemaphoreGive(displayDataMutex);
    } else {
      xSemaphoreGive(bleDataMutex);
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);  // 適宜ディレイ
  }
}

// ディスプレイ表示タスク
void displayTask(void *pvParameters) {
  for (;;) {
    // 計算結果キューからデータを取得
    xSemaphoreTake(displayDataMutex, portMAX_DELAY);
    if (!displayDataQueue.empty()) {
      DisplayData data = displayDataQueue.front();
      displayDataQueue.pop();
      xSemaphoreGive(displayDataMutex);

      int tx = data.tx;
      int ty = data.ty;
      int x = data.x;
      int y = data.y;
      int sx = data.sx;
      int sy = data.sy;
      int z = 16;

      // 新しいタイルの位置が最後の位置と異なる場合のみ描画 
          if (tx != lastTx || ty != lastTy) { 
            lastTx = tx; 
            lastTy = ty; 
 
            String filename[9]; 
            filename[0] = String("/images/std/" + String(z) + "/" + String(tx - 1) + "/" + String(ty - 1) + ".png"); 
            filename[1] = String("/images/std/" + String(z) + "/" + String(tx) + "/" + String(ty - 1) + ".png"); 
            filename[2] = String("/images/std/" + String(z) + "/" + String(tx + 1) + "/" + String(ty - 1) + ".png"); 
            filename[3] = String("/images/std/" + String(z) + "/" + String(tx - 1) + "/" + String(ty) + ".png"); 
            filename[4] = String("/images/std/" + String(z) + "/" + String(tx) + "/" + String(ty) + ".png"); 
            filename[5] = String("/images/std/" + String(z) + "/" + String(tx + 1) + "/" + String(ty) + ".png"); 
            filename[6] = String("/images/std/" + String(z) + "/" + String(tx - 1) + "/" + String(ty + 1) + ".png"); 
            filename[7] = String("/images/std/" + String(z) + "/" + String(tx) + "/" + String(ty + 1) + ".png"); 
            filename[8] = String("/images/std/" + String(z) + "/" + String(tx + 1) + "/" + String(ty + 1) + ".png"); 
 
            for (int i = 0; i < 9; i++) { 
              Serial.printf("%s\n", filename[i].c_str()); 
            } 

            record.fillSprite(TFT_WHITE);
            if ((120 <= x && x <= 136) && (120 <= y && y <= 136)) {
              File file = SD.open(filename[4].c_str()); // 画像をSDカードから読み込む 
              if (file) { 
                record.drawPng(&file, sx, sy); // SDカードからPNG画像を描画 
                file.close(); // ファイルを閉じる 
                write(); // 画面に表示 
                Serial.println("Update success");
              } else { 
                Serial.println("File not found!"); 
              }
            } 
            if ((0 <= x && x <= 120) && (120 <= y && y <= 136)) {
              File file1 = SD.open(filename[4].c_str()); // 画像をSDカードから読み込む 
              File file2 = SD.open(filename[3].c_str());
              if (file1) { 
                record.drawPng(&file1, sx, sy); // SDカードからPNG画像を描画 
                record.drawPng(&file2, sx - 256, sy);
                file1.close(); // ファイルを閉じる
                file2.close(); 
                write(); // 画面に表示 
                Serial.println("Update success");
              } else { 
                Serial.println("File not found!"); 
              }
            }
            if ((120 <= x && x <= 136) && (0 <= y && y <= 120)) {
              File file1 = SD.open(filename[4].c_str()); // 画像をSDカードから読み込む 
              File file2 = SD.open(filename[1].c_str());
              if (file1) { 
                record.drawPng(&file1, sx, sy); // SDカードからPNG画像を描画 
                record.drawPng(&file2, sx, sy - 256);
                file1.close(); // ファイルを閉じる
                file2.close(); 
                write(); // 画面に表示 
                Serial.println("Update success");
              } else { 
                Serial.println("File not found!"); 
              }
            }
            if ((136 <= x && x <= 256) && (120 <= y && y <= 136)) {
              File file1 = SD.open(filename[4].c_str()); // 画像をSDカードから読み込む 
              File file2 = SD.open(filename[5].c_str());
              if (file1) { 
                record.drawPng(&file1, sx, sy); // SDカードからPNG画像を描画 
                record.drawPng(&file2, sx + 256, sy);
                file1.close(); // ファイルを閉じる
                file2.close(); 
                write(); // 画面に表示 
                Serial.println("Update success");
              } else { 
                Serial.println("File not found!"); 
              }
            }
            if ((120 <= x && x <= 136) && (136 <= y && y <= 256)) {
              File file1 = SD.open(filename[4].c_str()); // 画像をSDカードから読み込む 
              File file2 = SD.open(filename[7].c_str());
              if (file1) { 
                record.drawPng(&file1, sx, sy); // SDカードからPNG画像を描画 
                record.drawPng(&file2, sx, sy + 256);
                file1.close(); // ファイルを閉じる
                file2.close(); 
                write(); // 画面に表示 
                Serial.println("Update success");
              } else { 
                Serial.println("File not found!"); 
              }
            }
            if ((0 <= x && x <= 120) && (0 <= y && y <= 120)) {
              File file1 = SD.open(filename[4].c_str()); // 画像をSDカードから読み込む 
              File file2 = SD.open(filename[0].c_str());
              File file3 = SD.open(filename[1].c_str());
              File file4 = SD.open(filename[3].c_str());
              if (file1) { 
                record.drawPng(&file1, sx, sy); // SDカードからPNG画像を描画 
                record.drawPng(&file2, sx - 256, sy - 256);
                record.drawPng(&file3, sx, sy - 256);
                record.drawPng(&file4, sx - 256, sy);
                file1.close(); // ファイルを閉じる
                file2.close(); 
                file3.close();
                file4.close();
                write(); // 画面に表示 
                Serial.println("Update success");
              } else { 
                Serial.println("File not found!"); 
              }
            }
            if ((136 <= x && x <= 256) && (0 <= y && y <= 120)) {
              File file1 = SD.open(filename[4].c_str()); // 画像をSDカードから読み込む 
              File file2 = SD.open(filename[1].c_str());
              File file3 = SD.open(filename[2].c_str());
              File file4 = SD.open(filename[5].c_str());
              if (file1) { 
                record.drawPng(&file1, sx, sy); // SDカードからPNG画像を描画 
                record.drawPng(&file2, sx, sy - 256);
                record.drawPng(&file3, sx + 256, sy - 256);
                record.drawPng(&file4, sx + 256, sy);
                file1.close(); // ファイルを閉じる
                file2.close(); 
                file3.close();
                file4.close(); 
                write(); // 画面に表示 
                Serial.println("Update success");
              } else { 
                Serial.println("File not found!"); 
              }
            }
            if ((0 <= x && x <= 120) && (136 <= y && y <= 256)) {
              File file1 = SD.open(filename[4].c_str()); // 画像をSDカードから読み込む 
              File file2 = SD.open(filename[3].c_str());
              File file3 = SD.open(filename[6].c_str());
              File file4 = SD.open(filename[7].c_str());
              if (file1) { 
                record.drawPng(&file1, sx, sy); // SDカードからPNG画像を描画 
                record.drawPng(&file2, sx - 256, sy);
                record.drawPng(&file3, sx - 256, sy + 256);
                record.drawPng(&file4, sx, sy + 256);
                file1.close(); // ファイルを閉じる
                file2.close(); 
                file3.close();
                file4.close();
                write(); // 画面に表示 
                Serial.println("Update success");
              } else { 
                Serial.println("File not found!"); 
              }
            }
            if ((136 <= x && x <= 256) && (136 <= y && y <= 256)) {
              File file1 = SD.open(filename[4].c_str()); // 画像をSDカードから読み込む 
              File file2 = SD.open(filename[5].c_str());
              File file3 = SD.open(filename[7].c_str());
              File file4 = SD.open(filename[8].c_str());
              if (file1) { 
                record.drawPng(&file1, sx, sy); // SDカードからPNG画像を描画 
                record.drawPng(&file2, sx + 256, sy);
                record.drawPng(&file3, sx, sy + 256);
                record.drawPng(&file4, sx + 256, sy + 256);
                file1.close(); // ファイルを閉じる
                file2.close(); 
                file3.close();
                file4.close(); 
                write(); // 画面に表示 
                Serial.println("Update success");
              } else { 
                Serial.println("File not found!"); 
              }
            }
            //中心に印をつける
            if (UseGPS) {
              display.fillCircle(120, 120, 8, TFT_LIGHTGREY);
              display.fillCircle(120, 120, 5, TFT_BLUE);
            }
            if (!UseGPS) {
              display.drawLine(115, 120, 125, 120, TFT_RED);
              display.drawLine(120, 115, 120, 125, TFT_RED);
            }
            batterycheck();
            timecheck();
          } else {
            record.pushSprite(sx, sy);
            write();
            Serial.println("Tile shift");
            Serial.printf("sx: %d, sy: %d\n", sx, sy);
            if (UseGPS) {
              display.fillCircle(120, 120, 8, TFT_LIGHTGREY);
              display.fillCircle(120, 120, 5, TFT_BLUE);
            }
            if (!UseGPS) {
              display.drawLine(115, 120, 125, 120, TFT_RED);
              display.drawLine(120, 115, 120, 125, TFT_RED);
            }
            batterycheck();
            timecheck();
          }                  
    } else {
      xSemaphoreGive(displayDataMutex);
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);  // 適宜ディレイ
  }
}
 
void setup() {
  Serial.begin(115200);
  tft.init();

  bool cardMounted = false;

  pinMode(3, OUTPUT);
  pinMode(0, INPUT_PULLUP);

  while (!cardMounted) {
    if (SD.begin(3)) {
      uint8_t cardType = SD.cardType();
      if (cardType != CARD_NONE) {
        cardMounted = true;
        Serial.println("SD Card Mounted Successfully");
      } else {
        Serial.println("No SD card attached");
      }
    } else {
      Serial.println("Card Mount Failed");
      Serial.println("Retrying to mount SD card...");
      delay(1000);
    }
  }

  analogReadResolution(12);

  //初期化
  display.init();
  record.createSprite(240, 240);
  Wire.begin();
  rtc.begin();

  // BLE初期化
  BLEDevice::init("");
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setActiveScan(true);

  // ミューテックスの作成
  bleDataMutex = xSemaphoreCreateMutex();
  displayDataMutex = xSemaphoreCreateMutex();

  // タスクの作成
  xTaskCreate(bleDataTask, "BLEDataTask", 8192, NULL, 3, NULL);
  xTaskCreate(calculationTask, "CalculationTask", 8192, NULL, 1, NULL);
  xTaskCreate(displayTask, "DisplayTask", 8192, NULL, 1, NULL);
}


 
void loop() {
  handleButton();
}
