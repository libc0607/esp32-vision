/*
   Vision V3.2 SPIFFS GIF DEMO
   Github: libc0607/esp32-lcd-vision

   ref: https://github.com/moononournation/RGB565_video

   About this demo:
     - play loop.gif once, and then go into deep sleep;
     - wake by tapping or timer;

   How-to in Arduino IDE:
    Choose:
      ESP32 Dev Module, 240MHz CPU, 80MHz Flash, DIO, 4MB(32Mb),
      No OTA (1M APP/3M SPIFFS), PSRAM Disable;
    check <project_dir>/data/loop.gif; (should less than ~3MB
    check DEEP_SLEEP_TIME_S below;
    upload;
    "ESP32 Sketch Data upload";

   Dependencies:
     Install espressif/arduino-esp32 (V1.0.6) first
      (using V2.0+ together with DFRobot_LIS will cause reset on every i2c.write now)
     moononournation/Arduino_GFX
     bitbank2/JPEGDEC
     DFRobot/DFRobot_LIS (see tapInterrupt example)
     me-no-dev/arduino-esp32fs-plugin (update test file to spiffs)

   This is just an early DEMO to verify all parts of the
   hardware can work properly.
   Many bugs. No guarantee for the performance of this code.

*/

#define GIF_FILENAME "/loop.gif"    // <project_dir>/data/loop.gif 
#define DEEP_SLEEP_TIME_S   30      // wakeup by timer every 30 seconds

#include <DFRobot_LIS.h>
#include <DFRobot_LIS2DW12.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <Arduino_GFX_Library.h>
#include "gifdec.h"
#include <Wire.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <rom/rtc.h>
#include <driver/rtc_io.h>
#include <driver/adc.h>
#include <esp_wifi.h>

#define PIN_MISO        2     // sdcard, not used (in this demo)
#define PIN_SCK         14    // sdcard, not used (in this demo)
#define PIN_MOSI        15    // sdcard, not used (in this demo)
#define PIN_SD_CS       13    // sdcard, not used (in this demo)
#define PIN_TFT_CS      5     // LCD
#define PIN_TFT_BL      22    // LCD
#define PIN_TFT_DC      27    // LCD
#define PIN_TFT_RST     33    // LCD
#define PIN_LCD_PWR_EN  19    // LCD, SDcard & temt6000 3v3 power (SY6280AAC on board)
#define PIN_ADC_BAT     35    // battery voltage, with 1M/1M resistor dividor on hardware
#define PIN_TEMP_ADC    36    // not used (in this demo)
#define PIN_BAT_STDBY   37    // cn3165 standby, low == charge finish, ext. pull-up
#define PIN_BAT_CHRG    38    // cn3165 chrg, low == charging, ext. pull-up
#define PIN_ACC_INT1    39    // from accelerometer, push-pull
#define PIN_ACC_INT2    32    // not used
#define PIN_SENSOR_ADC  34    // light sensor (temt6000), to GND if not connected
#define PIN_I2C_SCL     25    // to accelerometer
#define PIN_I2C_SDA     26    // to accelerometer

#define uS_TO_S_FACTOR 1000000ULL
#define BAT_ADC_THRESH_LOW  1700  // analogRead( 1/2*Vbat )

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_TFT_DC/* DC */, PIN_TFT_CS /* CS */, PIN_SCK, PIN_MOSI, PIN_MISO, VSPI );
//Arduino_GC9A01  *gfx = new Arduino_GC9A01(bus, PIN_TFT_RST, 2, true);
Arduino_ST7789  *gfx = new Arduino_ST7789(bus, PIN_TFT_RST, 0, true, 240, 240, 0, 0);
RTC_DATA_ATTR int bootCount = 0;

DFRobot_LIS2DW12_I2C acce(&Wire, 0x19);   // sdo/sa0 internal pull-up

void set_io_before_deep_sleep()
{
  pinMode(PIN_TFT_BL, INPUT);
  pinMode(PIN_TFT_CS, INPUT);
  pinMode(PIN_TFT_RST, INPUT);
  pinMode(PIN_MISO, INPUT);
  pinMode(PIN_SCK, INPUT);
  pinMode(PIN_MOSI, INPUT);
  pinMode(PIN_SD_CS, INPUT);
  pinMode(PIN_TFT_DC, INPUT);
  pinMode(1, INPUT);
  pinMode(3, INPUT);
  pinMode(PIN_I2C_SCL, INPUT);
  pinMode(PIN_I2C_SDA, INPUT);
  pinMode(PIN_ACC_INT2, INPUT);
  pinMode(PIN_ADC_BAT, INPUT);
  pinMode(PIN_TEMP_ADC, INPUT);
  pinMode(PIN_SENSOR_ADC, INPUT);

  pinMode(PIN_ACC_INT1, INPUT);
  pinMode(PIN_BAT_STDBY, INPUT);
  pinMode(PIN_BAT_CHRG, INPUT);

  digitalWrite(PIN_LCD_PWR_EN, LOW);
  gpio_hold_en(( gpio_num_t ) PIN_LCD_PWR_EN );
  gpio_deep_sleep_hold_en();
  esp_wifi_stop();

}

void setup()
{
  bootCount++;

  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);
  Serial.print("Vision v3.2, SPIFFS GIF demo; bootcounter: ");
  Serial.println(bootCount);

  gpio_deep_sleep_hold_dis();
  gpio_hold_dis(( gpio_num_t ) PIN_LCD_PWR_EN );
  pinMode(PIN_LCD_PWR_EN, OUTPUT);

  Wire.setPins(PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.print("finding acce. chip");
  while (!acce.begin()) {
    delay(50);
    Serial.print(".");
  }
  Serial.println("!");
  Serial.print("acce. chip id : ");
  Serial.println(acce.getID(), HEX);

  pinMode(PIN_ACC_INT1, INPUT);
  pinMode(PIN_BAT_STDBY, INPUT);
  pinMode(PIN_BAT_CHRG, INPUT);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_ACC_INT1 , HIGH);
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME_S * uS_TO_S_FACTOR);

  // find out why we are working
  RESET_REASON rst_reason = rtc_get_reset_reason(0);
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (rst_reason == DEEPSLEEP_RESET) {
    // wakeup by acce.
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
      Serial.println("wakeup by ext0");
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
      Serial.println("wakeup by timer");
    } else {
      Serial.println("wakeup by yjsnpi");
    }

  } else {
    Serial.println("bootup, init acce");

    acce.setRange(DFRobot_LIS2DW12::e2_g);
    acce.setPowerMode(DFRobot_LIS2DW12::eContLowPwrLowNoise1_12bit);
    acce.setDataRate(DFRobot_LIS2DW12::eRate_100hz);
    acce.enableTapDetectionOnZ(true);
    acce.enableTapDetectionOnY(true);
    acce.setTapThresholdOnY(0.8);
    acce.setTapThresholdOnZ(0.8);
    acce.setTapDur(3);
    acce.setTapMode(DFRobot_LIS2DW12::eBothSingleDouble);
    acce.setInt1Event(DFRobot_LIS2DW12::eDoubleTap);

  }

  uint32_t battery_voltage_adc = analogRead(PIN_ADC_BAT);


  Serial.print("battery_adc: "); Serial.println(battery_voltage_adc);
  if (battery_voltage_adc < BAT_ADC_THRESH_LOW) {
    Serial.println("battery low, deep sleep start");
    set_io_before_deep_sleep();
    esp_deep_sleep_start();
  } else {
    digitalWrite(PIN_LCD_PWR_EN, HIGH);
    delay(50);
  }

  // init lcd
  gfx->begin();
  gfx->fillScreen(BLACK);
  delay(50);  

  // light sensor -> lcd brightness
  float light_sens = analogRead(PIN_SENSOR_ADC);
  float light_sens_norm = light_sens/4096.0;
  light_sens_norm = pow(light_sens_norm, 2.0);
  ledcAttachPin(PIN_TFT_BL, 1); // assign TFT_BL pin to channel 1
  ledcSetup(1, 12000, 8);   // 12 kHz PWM, 8-bit resolution
  uint8_t lcd_brightness = int(light_sens_norm * 224.0) + 31; //  minimum
  ledcWrite(1, lcd_brightness);  // brightness
  Serial.print("lcd_brightness: ");
  Serial.println(lcd_brightness);

  // charger state
  if (digitalRead(PIN_BAT_CHRG) == LOW) {
    Serial.println("battery: charging");
  } else if (digitalRead(PIN_BAT_STDBY) == LOW) {
    Serial.println("battery: full");
  } else {
    Serial.println("battery: not charging");
  }

  // Init SPIFFS & play GIF
  if (!SPIFFS.begin(true)) {
    Serial.println(F("ERROR: SPIFFS mount failed!"));
    gfx->println(F("ERROR: SPIFFS mount failed!"));
  } else {
    File vFile = SPIFFS.open(GIF_FILENAME);
    if (!vFile || vFile.isDirectory()) {
      Serial.println(F("ERROR: Failed to open "GIF_FILENAME" file for reading"));
      gfx->println(F("ERROR: Failed to open "GIF_FILENAME" file for reading"));
    } else {
      gd_GIF *gif = gd_open_gif(&vFile);
      if (!gif) {
        Serial.println(F("gd_open_gif() failed!"));
      } else {
        int32_t s = gif->width * gif->height;
        uint8_t *buf = (uint8_t *)malloc(s);
        if (!buf) {
          Serial.println(F("buf malloc failed!"));
        } else {
          Serial.println(F("GIF video start"));
          gfx->setAddrWindow((gfx->width() - gif->width) / 2, (gfx->height() - gif->height) / 2, gif->width, gif->height);
          int t_fstart, t_delay = 0, t_real_delay, res, delay_until;
          int duration = 0, remain = 0;

          while (1) {
            t_fstart = millis();
            t_delay = gif->gce.delay * 10;
            res = gd_get_frame(gif, buf);
            if (res < 0) {
              Serial.println(F("ERROR: gd_get_frame() failed!"));
              break;
            } else if (res == 0) {
              //Serial.printf("rewind, duration: %d, remain: %d (%0.1f %%)\n", duration, remain, 100.0 * remain / duration);
              //duration = 0;
              //remain = 0;
              //gd_rewind(gif);
              //continue;
              Serial.println("gif: break;");
              break;
            }

            gfx->startWrite();
            gfx->writeIndexedPixels(buf, gif->palette->colors, s);
            gfx->endWrite();

            t_real_delay = t_delay - (millis() - t_fstart);
            duration += t_delay;
            remain += t_real_delay;
            delay_until = millis() + t_real_delay;
            do {
              delay(1);
            } while (millis() < delay_until);
          }

          Serial.println(F("GIF video end"));
          Serial.printf("duration: %d, remain: %d (%0.1f %%)\n", duration, remain, 100.0 * remain / duration);
          gd_close_gif(gif);
        }
      }
    }
  }


  Serial.println(F("deep sleep"));
  ledcWrite(1, 0);
  delay(20);
  ledcDetachPin(PIN_TFT_BL);
  gfx->displayOff();

  set_io_before_deep_sleep();
  esp_deep_sleep_start();
}

void loop()
{
}
