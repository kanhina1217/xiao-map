#include <BLEDevice.h> 
#include <BLEUtils.h> 
#include <BLEClient.h> 
#include <SD.h>
#include <cmath>
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
 
// display変数 
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
 
void write() { 
  display.startWrite(); 
  record.pushSprite(0,0); 
  display.endWrite(); 
  display.waitDisplay(); 
} 
 
const uint8_t targetManufacturerData[] = {0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04}; 
const size_t manufacturerDataLength = sizeof(targetManufacturerData); 
const int scanTime = 5; 
int lastTx = -1; 
int lastTy = -1; 

void getLocationFromBLE(const uint8_t* payload, int payloadLength, float &la, float &ln) {
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

  la = tlat / 1000000.0; 
  ln = tlon / 1000000.0; 
  Serial.printf("lat: %.6f, lon: %.6f\n", la, ln); 
}

void displayTile(float la, float ln) {
  int z = 16; 
  double L = 85.05112878; 
  double px = int(pow(2.0, z + 7.0) * ((ln / 180.0) + 1.0)); 
  double py = int(pow(2.0, z + 7.0) * (-1 * atanh(sin(PI * la / 180.0)) + atanh(sin(PI * L / 180.0))) / PI); 
  int tx = px / 256; 
  int ty = py / 256; 
  Serial.printf("tx: %d, ty: %d\n", tx, ty); 

  int x = int(px) % 256; 
  int y = int(py) % 256; 

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
    File file = SD.open(filename[4].c_str()); 
    if (file) { 
      record.drawPng(&file, 0, 0); 
      file.close(); 
      write(); 
    } else { 
      Serial.println("File not found!"); 
    } 
  } 
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

          float la, ln;
          getLocationFromBLE(payload, payloadLength, la, ln);
          displayTile(la, ln);
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
  record.createSprite(240, 240);

  BLEDevice::init("");
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(scanTime, false);
}

void loop() { 
  delay(10000); 
}
