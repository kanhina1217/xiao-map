#include <BLEDevice.h>  
#include <BLEUtils.h>  
#include <BLEClient.h>  
#include <SD.h> 
 
#include <TFT_eSPI.h> 
TFT_eSPI tft = TFT_eSPI(); 
 
#define LGFX_USE_V1  
#include <LovyanGFX.hpp>  
 
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
    bus_cfg.spi_host = SPI3_HOST;  // VSPI_HOST を SPI3_HOST に置き換え
    bus_cfg.spi_mode = 0;  
    bus_cfg.freq_write = 40000000;  
    bus_cfg.freq_read = 16000000;  
    bus_cfg.spi_3wire = false;  
    bus_cfg.use_lock = true;  
    bus_cfg.dma_channel = 1;  
    bus_cfg.pin_sclk = 18;  
    bus_cfg.pin_mosi = 23;  
    bus_cfg.pin_miso = -1;  
    bus_cfg.pin_dc = 2;  
    _bus.config(bus_cfg);  
    _panel.setBus(&_bus);  

    auto panel_cfg = _panel.config();  
    panel_cfg.pin_cs = 5;  
    panel_cfg.pin_rst = 4;  
    panel_cfg.pin_busy = -1;  
    panel_cfg.memory_width = 240;  
    panel_cfg.memory_height = 240;  
    panel_cfg.panel_width = 240;  
    panel_cfg.panel_height = 240;  
    panel_cfg.offset_x = 0;  
    panel_cfg.offset_y = 0;  
    panel_cfg.offset_rotation = 0;  
    panel_cfg.dummy_read_pixel = 8;  
    panel_cfg.dummy_read_bits = 1;  
    panel_cfg.readable = true;  
    panel_cfg.invert = false;  
    panel_cfg.rgb_order = false;  
    panel_cfg.dlen_16bit = false;  
    panel_cfg.bus_shared = true;  
    _panel.config(panel_cfg);  

    auto light_cfg = _light.config();  
    light_cfg.pin_bl = 21;  
    light_cfg.invert = false;  
    light_cfg.freq = 44100;  
    light_cfg.pwm_channel = 7;  
    _light.config(light_cfg);  
    _panel.setLight(&_light);  

    auto touch_cfg = _touch.config();   
    touch_cfg.pin_int = 7; // for XIAO RP2040  
    touch_cfg.i2c_port = 1; // for XIAO RP2040  
    touch_cfg.pin_sda  = 4; // for XIAO RP2040  
    touch_cfg.pin_scl  = 5; // for XIAO RP2040  
    touch_cfg.freq = 400000;  

    _touch.config(touch_cfg);  
    _panel.setTouch(&_touch);  

    setPanel(&_panel);  
  }  
} display;  

LGFX_Sprite record(&display);  // グローバルに移動

void write(int sx, int sy) {  
  display.startWrite();  
  record.pushSprite(sx, sy);  
  display.endWrite();  
  display.waitDisplay();  
}  

// Manufacturer Dataのマッチングするデータ  
const uint8_t targetManufacturerData[] = {0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04};  
const size_t manufacturerDataLength = sizeof(targetManufacturerData);  

// BLEスキャンの時間  
const int scanTime = 5; // スキャン時間（秒）  

// 画像タイルの位置を記録する変数  
int lastTx = -1;  
int lastTy = -1;  

// SDカードファイルの存在確認を追加し、メモリの最適化を行ったコード

void loadTileImages(String filenames[]) {
  record.fillSprite(TFT_WHITE);
  for (int i = 0; i < 9; i++) {
    Serial.printf("Loading file: %s\n", filenames[i].c_str());
    record.drawPngFile(SD, filenames[i].c_str(), (i % 3) * 256, (i / 3) * 256);
    
  }
}

void updateDisplay(float la, float ln, int rx, int ry) {
  record.fillCircle(120, 120, 8, TFT_LIGHTGREY);
  record.fillCircle(120, 120, 5, TFT_BLUE);
  record.printf("lat: %.6f, lon: %.6f\n", la, ln);
  write(rx, ry);
}

void onTargetManufacturerDataFound(float la, float ln, int tx, int ty, int rx, int ry) {
  if (tx != lastTx || ty != lastTy) {
    lastTx = tx;
    lastTy = ty;
    String filename[9];
    filename[0] = String("/images/std/16/" + String(tx - 1) + "/" + String(ty - 1) + ".png");
    filename[1] = String("/images/std/16/" + String(tx) + "/" + String(ty - 1) + ".png");
    filename[2] = String("/images/std/16/" + String(tx + 1) + "/" + String(ty - 1) + ".png");
    filename[3] = String("/images/std/16/" + String(tx - 1) + "/" + String(ty) + ".png");
    filename[4] = String("/images/std/16/" + String(tx) + "/" + String(ty) + ".png");
    filename[5] = String("/images/std/16/" + String(tx + 1) + "/" + String(ty) + ".png");
    filename[6] = String("/images/std/16/" + String(tx - 1) + "/" + String(ty + 1) + ".png");
    filename[7] = String("/images/std/16/" + String(tx) + "/" + String(ty + 1) + ".png");
    filename[8] = String("/images/std/16/" + String(tx + 1) + "/" + String(ty + 1) + ".png");

    loadTileImages(filename);  // SDカードから画像を読み込み
    updateDisplay(la, ln, rx, ry);  // ディスプレイに表示を更新
    Serial.println("Display updated with new data.");
  } else {
    Serial.println("No updates, same tile.");
    updateDisplay(la, ln, rx, ry);  // ディスプレイを更新（位置の表示）
  }
}


// BLEコールバッククラス  
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Manufacturer Dataがあるか確認
    if (advertisedDevice.haveManufacturerData()) {
      std::string manufacturerData = advertisedDevice.getManufacturerData();

      // ターゲットデータと一致するかチェック
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

          // 緯度と経度を抽出
          uint8_t* payload = advertisedDevice.getPayload();
          int payloadLength = advertisedDevice.getPayloadLength();
          String Blat = "", Blon = "";
          for (int i = 0; i < payloadLength; i++) {
            if (i >= 13 && i <= 16) {
              Blat += String(payload[i], HEX);
            }
            if (i >= 17 && i <= 20) {
              Blon += String(payload[i], HEX);
            }
          }

          // 16進数から浮動小数点に変換
          unsigned long tlat = strtoul(Blat.c_str(), nullptr, 16);
          unsigned long tlon = strtoul(Blon.c_str(), nullptr, 16);
          float lat = tlat / 1000000.0;
          float lon = tlon / 1000000.0;
          Serial.printf("lat: %.6f, lon: %.6f\n", lat, lon);

          // タイル座標の計算
          int z = 16;
          double L = 85.05112878;
          double px = int(pow(2.0, z + 7.0) * ((lon / 180.0) + 1.0));
          double py = int(pow(2.0, z + 7.0) * (-1 * atanh(sin(PI * lat / 180.0)) + atanh(sin(PI * L / 180.0))) / PI);
          int tx = px / 256;
          int ty = py / 256;
          Serial.printf("tx: %d, ty: %d\n", tx, ty);

          int x = int(px) % 256;
          int y = int(py) % 256;
          int rx = 120 - x + 256;
          int ry = 120 - y + 256;

          onTargetManufacturerDataFound(lat, lon, tx, ty, rx, ry);
        }
      }
    }
  }
};


void setup() {  
  Serial.begin(115200);  
  tft.init();  

  bool cardMounted = false;  

  pinMode(3, OUTPUT);  

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

  display.init();  
  display.setRotation(0);  
  record.createSprite(384, 384);  

  BLEDevice::init("");  
  BLEScan *pBLEScan = BLEDevice::getScan();  
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());  
  pBLEScan->setInterval(100);  
  pBLEScan->setWindow(99);  
  pBLEScan->setActiveScan(true);  
}

void loop() {  
  Serial.println("BLE scan starting...");  
  BLEScan *pBLEScan = BLEDevice::getScan();  
  pBLEScan->start(scanTime, false);  
  Serial.println("Done.");  
  delay(5000);  
}
