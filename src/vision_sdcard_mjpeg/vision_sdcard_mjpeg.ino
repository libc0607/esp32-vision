/*
   Vision V3.3 SD MJPEG WebServer OTA DEMO
   Github: libc0607/esp32-vision

   ref:
   https://github.com/moononournation/RGB565_video 
   examples/SDWebServer
   examples/OTAWebUpdater

   WebServer bring-up:
   curl -X POST -F "file=@ota.htm" http://vision.local/edit
   curl -X POST -F "file=@index.htm" http://vision.local/edit
   curl -X POST -F "file=@edit.htm" http://vision.local/edit

   How-to in Arduino IDE:
    Choose:
      ESP32 Dev Module, 240MHz CPU, 80MHz Flash, DIO, 4MB(32Mb),
      default with spiffs, PSRAM Disable;
    upload;

   Dependencies:
     Install espressif/arduino-esp32 (V1.0.6, due to compatibility issue) first
     moononournation/Arduino_GFX
     bitbank2/JPEGDEC
     DFRobot/DFRobot_LIS (see tapInterrupt example)
     stevemarple/IniFile

   Config ini example:

    [vision]
    video=/loop.mjpeg
    lcd_rotation=0
    target_fps=15

   This is just an EARLY DEMO to verify all parts of the
   hardware can work properly.
   Many bugs. No guarantee for the performance of this code.
   就是抄了一大堆例程，并小心地让它们能一起工作而已。很烂，轻喷

*/

#define CONFIG_FILENAME "/config.txt"
const char* wifi_ssid = "Celestia";
const char* wifi_pwd = "mimitomo";
const char* wifi_host = "vision";
#define CONF_GIFNAME_DEFAULT "/loop.mjpeg"
#define CONF_TARGET_FPS_DEFAULT 15
//#define CONF_GRAVITY_DEFAULT  true
#define CONF_LCD_ROTATION_DEFAULT  0
#define CONF_LOOP_MODE_DEFAULT false
#define DEEP_SLEEP_LONG_S  30
#define DEEP_SLEEP_SHORT_S  2
#define DEEP_SLEEP_SHORT_CNT  6
#define TEMP_PROTECT_HIGH_THRESH_12B  1840 // ~60°C @10k,B3380
#define LCD_BACKLIGHT_MIN_8B  16
#define LCD_PWM_FIR_LEN 16


#define FILE_SYSTEM  SD

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <SD_MMC.h>
#include <ESPmDNS.h>
#include <DFRobot_LIS.h>
#include <DFRobot_LIS2DW12.h>
#include <IniFile.h>
#include <Update.h>
#include <SPIFFS.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <rom/rtc.h>
#include <driver/rtc_io.h>
#include <driver/adc.h>
#include <esp_wifi.h>



#define PIN_MISO        2     // sdcard
#define PIN_SCK         14    // sdcard
#define PIN_MOSI        15    // sdcard
#define PIN_SD_CS       13    // sdcard
#define PIN_TFT_CS      5     // LCD
#define PIN_TFT_BL      22    // LCD
#define PIN_TFT_DC      27    // LCD
#define PIN_TFT_RST     33    // LCD
#define PIN_LCD_PWR_EN  19    // LCD, SDcard & temt6000 3v3 power (SY6280AAC on board)
#define PIN_ADC_BAT     35    // battery voltage, with 1M/1M resistor dividor on hardware
#define PIN_TEMP_ADC    36    // 
#define PIN_BAT_STDBY   37    // cn3165 standby, low == charge finish, ext. pull-up
#define PIN_BAT_CHRG    38    // cn3165 chrg, low == charging, ext. pull-up
#define PIN_ACC_INT1    39    // from accelerometer, push-pull
#define PIN_ACC_INT2    32    // not used
#define PIN_SENSOR_ADC  34    // light sensor (temt6000), to GND if not connected
#define PIN_I2C_SCL     25    // to accelerometer
#define PIN_I2C_SDA     26    // to accelerometer
#define PIN_LED         23    // ext. led
#define PIN_KEY         18    // ext. key

#define WIFI_CONNECT_TIMEOUT_S 20
#define uS_TO_S_FACTOR (1000000ULL)
#define BAT_ADC_THRESH_LOW   (1700)  // analogRead( 1/2*Vbat )
#define BAT_ADC_THRESH_WARN  (1880)  // analogRead( 1/2*Vbat )
#define SENSOR_DISWIFI_THRESH_8B  10
#define ACCE_DISWIFI_Z_THRESH  850 // while g~980
#define ACCE_ORIENTATION_THRESH 850
//#define FPS 24                     // but still lots of frame skipped. haiyaa why r u so weak
#define MJPEG_BUFFER_SIZE (240 * 240 * 2 / 4)

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_TFT_DC/* DC */, PIN_TFT_CS /* CS */, PIN_SCK, PIN_MOSI, PIN_MISO, VSPI, true );
Arduino_GC9A01  *gfx = new Arduino_GC9A01(bus, PIN_TFT_RST, 2, true); 
//Arduino_ST7789  *gfx = new Arduino_ST7789(bus, PIN_TFT_RST, 0, true, 240, 240, 0, 0, 0, 80);

DFRobot_LIS2DW12_I2C acce(&Wire, 0x19);   // sdo/sa0 internal pull-up


#include "MjpegClass.h"
static MjpegClass mjpeg;

int next_frame = 0;
unsigned long start_ms, curr_ms, next_frame_ms, lightsleep_ms;
uint8_t lcd_pwm_filter[LCD_PWM_FIR_LEN] = {LCD_BACKLIGHT_MIN_8B};
uint8_t p_lcd_pwm_filter;

WebServer server(80);
static bool hasSD = false;
File uploadFile;
uint8_t lcd_brightness;
int led_status;

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int lastTappedCount = 0;
RTC_DATA_ATTR uint8_t lcd_brightness_by_key = LCD_BACKLIGHT_MIN_8B;

void set_io_before_deep_sleep() {

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

  pinMode(PIN_KEY, INPUT_PULLUP);
  pinMode(PIN_LED, INPUT);
  
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_ACC_INT1 , HIGH);
  //esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BAT_STDBY , LOW);
  //esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BAT_CHRG , LOW);
  
  digitalWrite(PIN_LCD_PWR_EN, LOW);
  gpio_hold_en(( gpio_num_t ) PIN_LCD_PWR_EN );
  gpio_deep_sleep_hold_en();
  esp_wifi_stop();
  //adc_power_release();
}

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.htm";
  }

  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  }

  File dataFile = FILE_SYSTEM.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = FILE_SYSTEM.open(path.c_str());
  }

  if (!dataFile) {
    return false;
  }

  if (server.hasArg("download")) {
    dataType = "application/octet-stream";
  }

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    Serial.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

void handleFileUpload() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  String filename = upload.filename;
  if (!filename.startsWith("/"))
    filename = "/" + filename;
  if (upload.status == UPLOAD_FILE_START) {
    gfx->fillScreen(ORANGE);SD.begin(PIN_SD_CS);
    if (FILE_SYSTEM.exists(filename)) {
      FILE_SYSTEM.remove(filename);
    }
    uploadFile = FILE_SYSTEM.open(filename, FILE_WRITE);
    Serial.print("Upload: START, filename: "); Serial.println(filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    led_status = (led_status == HIGH)? LOW: HIGH;
    digitalWrite(PIN_LED, led_status);
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    Serial.print("Upload: WRITE, Bytes: "); Serial.println(upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
    gfx->fillScreen(BLUE);SD.begin(PIN_SD_CS);
    Serial.print("Upload: END, Size: "); Serial.println(upload.totalSize);
  }
}

void deleteRecursive(String path) {
  File file = FILE_SYSTEM.open((char *)path.c_str());
  if (!file.isDirectory()) {
    file.close();
    FILE_SYSTEM.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while (true) {
    File entry = file.openNextFile();
    if (!entry) {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) {
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      FILE_SYSTEM.remove((char *)entryPath.c_str());
    }
    yield();
  }

  FILE_SYSTEM.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !FILE_SYSTEM.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || FILE_SYSTEM.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0) {
    File file = FILE_SYSTEM.open((char *)path.c_str(), FILE_WRITE);
    if (file) {
      file.write(0);
      file.close();
    }
  } else {
    FILE_SYSTEM.mkdir((char *)path.c_str());
  }
  returnOK();
}

void getStatus() {

  // A human-readable system status API

  String stat = "";

  // card type
  stat += "SD_Type,";
  uint8_t cardType = FILE_SYSTEM.cardType();
  if (cardType == CARD_NONE) {
    stat += "No_Card";
  } else if (cardType == CARD_MMC) {
    stat += "SD_MMC";
  } else if (cardType == CARD_SD) {
    stat += "SD_SC";
  } else if (cardType == CARD_SDHC) {
    stat += "SD_HC";
  } else {
    stat += "Card_UNKNOWN";
  }
  stat += ",";

  // card space
  stat += "Total_MB,";
  stat += String(int(FILE_SYSTEM.totalBytes() / (1024 * 1024)));
  stat += "MB,Used_MB,";
  stat += String(int(FILE_SYSTEM.usedBytes() / (1024 * 1024)));
  stat += "MB,";
  // Battery info
  stat += "Battery_ADC,";
  stat += analogRead(PIN_ADC_BAT);
  stat += ",Charger,";
  if (digitalRead(PIN_BAT_CHRG) == LOW) {
    stat += "charging,";
  } else if (digitalRead(PIN_BAT_STDBY) == LOW) {
    stat += "full,";
  } else {
    stat += "not_charging,";
  }

  // light sensor info
  stat += "Light_ADC,";
  stat += analogRead(PIN_SENSOR_ADC);

  // acce.
  stat += ",Acc_X,";
  stat += acce.readAccX();
  stat += ",Acc_Y,";
  stat += acce.readAccY();
  stat += ",Acc_Z,";
  stat += acce.readAccZ();

  stat += ",\r\n";
  server.send(200, "text/plain", stat);
}

void printDirectory() {
  if (!server.hasArg("dir")) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  //if (path != "/" && !SD.exists((char *)path.c_str())) {
  if (path != "/" && !FILE_SYSTEM.exists((char *)path.c_str())) {
    return returnFail("BAD PATH");
  }
  //File dir = SD.open((char *)path.c_str());
  File dir = FILE_SYSTEM.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory()) {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    String output;
    if (cnt > 0) {
      output = ',';
    }

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\",\"size\":\"";
    output += entry.size();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
  }
  server.sendContent("]");
  dir.close();
}

void handleNotFound() {
  if (hasSD && loadFromSdCard(server.uri())) {
    return;
  }
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.print(message);
}

void handleOTAresult() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  Serial.println("OTA update success, rebooting...");
  gfx->begin();
  gfx->fillScreen(GREEN);
  delay(200);
  ESP.restart();
}
  
void handleOTA() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    gfx->begin();
    gfx->fillScreen(RED);
    //Serial.println("Start OTA Update");
    Serial.setDebugOutput(true);
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin()) { //start with max available size
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    led_status = (led_status == HIGH)? LOW: HIGH;
    digitalWrite(PIN_LED, led_status);
    Serial.print(".");
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { //true to set the size to the current progress
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
    Serial.setDebugOutput(false);
  } else {
    Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
  }
}

void testWriteFile(fs::FS &fs, const char *path, uint8_t *buf, int len) {
  unsigned long start_time = millis();
  Serial.printf("Test write %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  int loop = 65536 / len;
  while (loop--)
  {
    if (!file.write(buf, len)) {
      Serial.println("Write failed");
      return;
    }
  }
  file.flush();
  file.close();
  unsigned long time_used = millis() - start_time;
  Serial.printf("Write file used: %d ms, %f KB/s\n", time_used, (float)65536 / time_used);
}

void testReadFile(fs::FS &fs, const char *path, uint8_t *buf, int len) {
  unsigned long start_time = millis();
  Serial.printf("Test read %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  int loop = 65536 / len;
  while (loop--)
  {
    if (!file.read(buf, len)) {
      Serial.println("Read failed");
      return;
    }
  }
  file.close();
  unsigned long time_used = millis() - start_time;
  Serial.printf("Read file used: %d ms, %f KB/s\n", time_used, (float)65536 / time_used);
}

void testIO(fs::FS &fs) {
  /* malloc will not reset all bytes to zero, so it is a random data */
  uint8_t *buf = (uint8_t*)malloc(64 * 1024);
  testWriteFile(fs, "/test_1k.bin", buf, 1024);
  testReadFile(fs, "/test_1k.bin", buf, 1024);
  free(buf);
}

void printErrorMessage(uint8_t e, bool eol = true) {
  switch (e) {
    case IniFile::errorNoError:
      Serial.print("no error");
      break;
    case IniFile::errorFileNotFound:
      Serial.print("file not found");
      break;
    case IniFile::errorFileNotOpen:
      Serial.print("file not open");
      break;
    case IniFile::errorBufferTooSmall:
      Serial.print("buffer too small");
      break;
    case IniFile::errorSeekError:
      Serial.print("seek error");
      break;
    case IniFile::errorSectionNotFound:
      Serial.print("section not found");
      break;
    case IniFile::errorKeyNotFound:
      Serial.print("key not found");
      break;
    case IniFile::errorEndOfFile:
      Serial.print("end of file");
      break;
    case IniFile::errorUnknownError:
      Serial.print("unknown error");
      break;
    default:
      Serial.print("unknown error value");
      break;
  }
  if (eol)
    Serial.println();
}

uint8_t get_lcd_brightness_by_adc (int adc_pin) {
  return uint8_t(( pow((analogRead(adc_pin)/4096.0), 2.0) * float(255-LCD_BACKLIGHT_MIN_8B) ) + LCD_BACKLIGHT_MIN_8B);
}

uint8_t lcd_pwm_fir_filter (uint8_t pwm_val) {
  uint32_t lcd_pwm_filter_sum;
  
  lcd_pwm_filter[p_lcd_pwm_filter] = pwm_val;
  p_lcd_pwm_filter++;
  if (p_lcd_pwm_filter == sizeof(lcd_pwm_filter)) {
    p_lcd_pwm_filter = 0;
  }
  for (int i=0; i<sizeof(lcd_pwm_filter); i++) {
    lcd_pwm_filter_sum += lcd_pwm_filter[i];
  }
  
  return (lcd_pwm_filter_sum/sizeof(lcd_pwm_filter));
}

void setup()
{
  int i;
  String conf_gifname = CONF_GIFNAME_DEFAULT;
  int conf_target_fps = CONF_TARGET_FPS_DEFAULT;
//  bool conf_gravity = CONF_GRAVITY_DEFAULT;
  int conf_lcd_rotation = CONF_LCD_ROTATION_DEFAULT;
  bool conf_loop_mode = CONF_LOOP_MODE_DEFAULT;
  int last_key_state = HIGH;
  int key_state = HIGH;
  uint8_t *mjpeg_buf;
  File vFile;

  WiFi.mode(WIFI_OFF);
  bootCount++;
  Serial.begin(115200);
  Serial.print("Vision v3.3, SD MJPEG WebServer OTA demo; bootcounter: "); Serial.print(bootCount);
  Serial.print(", lastTapped: "); Serial.println(lastTappedCount);

  // disable all PAD HOLD
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis(( gpio_num_t ) PIN_LCD_PWR_EN );
  pinMode(PIN_LCD_PWR_EN, OUTPUT);
  pinMode(PIN_KEY, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW); // startup blink
  
  // find out why we are working
  RESET_REASON rst_reason = rtc_get_reset_reason(0);
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  // init accel.
  Wire.setPins(PIN_I2C_SDA, PIN_I2C_SCL);
  while (!acce.begin()) {
    delay(50);
  }
  
  Serial.print("accel. chip id : "); Serial.println(acce.getID(), HEX);
  Serial.println("Init accel.");
  acce.setRange(DFRobot_LIS2DW12::e2_g);
  acce.setPowerMode(DFRobot_LIS2DW12::eContLowPwrLowNoise1_12bit);
  acce.setDataRate(DFRobot_LIS2DW12::eRate_100hz);
  acce.enableTapDetectionOnZ(true);
  acce.enableTapDetectionOnY(true);
  //acce.enableTapDetectionOnX(true);
  acce.setTapThresholdOnY(0.8);
  acce.setTapThresholdOnZ(0.8);
  //acce.setTapThresholdOnX(0.6);
  acce.setTapDur(3);
  acce.setTapMode(DFRobot_LIS2DW12::eBothSingleDouble);
  acce.setInt1Event(DFRobot_LIS2DW12::eDoubleTap);
  DFRobot_LIS2DW12:: eTap_t tapEvent = acce.tapDetect();
  
  // set wakeup by accel.
  pinMode(PIN_ACC_INT1, INPUT);
  pinMode(PIN_BAT_STDBY, INPUT);
  pinMode(PIN_BAT_CHRG, INPUT);
  
  // check battery voltage & temperature
  uint32_t battery_voltage_adc = analogRead(PIN_ADC_BAT);
  uint32_t ntc_voltage_adc = analogRead(PIN_TEMP_ADC);
  Serial.print("battery_adc: "); Serial.println(battery_voltage_adc);
  Serial.print("ntc_adc: "); Serial.println(ntc_voltage_adc);
  if (battery_voltage_adc < BAT_ADC_THRESH_LOW) {
    // just go sleep if battery low
    Serial.println("battery low, deep sleep start");
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_LONG_S * uS_TO_S_FACTOR);
    set_io_before_deep_sleep();
    esp_deep_sleep_start();
  } 
  /*
  if (ntc_voltage_adc < TEMP_PROTECT_HIGH_THRESH_12B) {
    // just go sleep if very hot
    Serial.println("high temperature, deep sleep start");
    set_io_before_deep_sleep();
    esp_deep_sleep_start();
  }
*/
  // enable 3v3b & init spi for sdcard
  digitalWrite(PIN_LCD_PWR_EN, HIGH);
  delay(50);

  // check charger state
  if (digitalRead(PIN_BAT_CHRG) == LOW) {
    Serial.println("battery: charging");
  } else if (digitalRead(PIN_BAT_STDBY) == LOW) {
    Serial.println("battery: full");
  } else {
    Serial.println("battery: not charging");
  }

  gfx->begin();
  if (battery_voltage_adc < BAT_ADC_THRESH_WARN) {
    gfx->fillScreen(RED);
    delay(200);
  } else {
    gfx->fillScreen(BLACK);
  }
  delay(200);

  
  ledcAttachPin(PIN_TFT_BL, 1);
  ledcSetup(1, 12000, 8);
  lcd_brightness = get_lcd_brightness_by_adc(PIN_SENSOR_ADC);
  for (i=0; i<sizeof(lcd_pwm_filter); i++) {
    lcd_pwm_filter[i] = lcd_brightness;
  }
  p_lcd_pwm_filter = 0;
  if (bootCount < 3) {
    lcd_brightness_by_key = lcd_brightness; // 这。。我有预感 又要变成屎山了
  }
  gfx->fillScreen(BLACK);
  digitalWrite(PIN_TFT_CS, HIGH);
  
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SD_CS);
  //SPIClass spi = SPIClass(HSPI);
  //spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SD_CS);
  //pinMode(PIN_MOSI, INPUT_PULLUP);
  //pinMode(PIN_SCK, INPUT_PULLUP);
  //pinMode(PIN_MISO, INPUT_PULLUP);
  //pinMode(PIN_SD_CS, INPUT_PULLUP);
  //if (!SD_MMC.begin("/sdcard", true)) {
  //if (!SD.begin(PIN_SD_CS, spi, 40000000)) {
  if (!SD.begin(PIN_SD_CS) && !SD.begin(PIN_SD_CS)) {
    Serial.println(F("ERROR: SD card mount failed!"));
  } else {
    hasSD = true;
    Serial.println(F("SD card mounted"));
    uint8_t cardType = FILE_SYSTEM.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("No SD_MMC card attached");
    }
    Serial.print("SD_MMC Card Type: ");
    if (cardType == CARD_MMC) {
      Serial.println("MMC");
    } else if (cardType == CARD_SD) {
      Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
      Serial.println("SDHC");
    } else {
      Serial.println("UNKNOWN");
    }
    Serial.printf("Total space: %lluMB\n", FILE_SYSTEM.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", FILE_SYSTEM.usedBytes() / (1024 * 1024));
    //testIO(FILE_SYSTEM);
  }

  
  // check config file & generate default if not exist
  const size_t buf_len = 128;
  char conf_buf[buf_len];
  if (!FILE_SYSTEM.exists(CONFIG_FILENAME)) {
    Serial.println("Config file not found, use default");
    File conf_file = FILE_SYSTEM.open(CONFIG_FILENAME, FILE_WRITE);
    if (conf_file) {
      conf_file.println("[vision]");
      conf_file.print("video="); conf_file.println(CONF_GIFNAME_DEFAULT);
//      conf_file.print("gravity="); conf_file.println(CONF_GRAVITY_DEFAULT? "true": "false"); 
      conf_file.print("target_fps="); conf_file.println(CONF_TARGET_FPS_DEFAULT);
      conf_file.print("lcd_rotation="); conf_file.println(CONF_LCD_ROTATION_DEFAULT);
//      conf_file.print("loop_mode="); conf_file.println(CONF_LOOP_MODE_DEFAULT? "true": "false");
      conf_file.close();
    }
  }
  // open ini
  
  IniFile ini(CONFIG_FILENAME);
  if (!ini.open()) {
    Serial.println("config file open error");
  }
  if (!ini.validate(conf_buf, buf_len)) {
    Serial.println("config file not valid");
    printErrorMessage(ini.getError());
  }
  Serial.println("Reading config file: ");
  if (ini.getValue("vision", "video", conf_buf, buf_len)) {
    conf_gifname = String(conf_buf);
    Serial.print("vision.video=");
    Serial.println(conf_gifname);
  } else {
    printErrorMessage(ini.getError());
    Serial.print("<vision.video> not found; use vision.video.default=");
    Serial.println(conf_gifname);
  }
/*
  if (ini.getValue("vision", "gravity", conf_buf, buf_len, conf_gravity)) {
    Serial.print("vision.gravity=");
    Serial.println(conf_gravity ? "true" : "false");
  } else {
    printErrorMessage(ini.getError());
    Serial.print("<vision.gravity> not found; use vision.gravity.default=");
    Serial.println(conf_gravity ? "true" : "false");
  }
  if (ini.getValue("vision", "loop_mode", conf_buf, buf_len, conf_loop_mode)) {
    Serial.print("vision.loop_mode=");
    Serial.println(conf_loop_mode ? "true" : "false");
  } else {
    printErrorMessage(ini.getError());
    Serial.print("<vision.loop_mode> not found; use vision.loop_mode.default=");
    Serial.println(conf_loop_mode ? "true" : "false");
  }*/
  if (ini.getValue("vision", "lcd_rotation", conf_buf, buf_len)) {
    conf_lcd_rotation = String(conf_buf).toInt();
    Serial.print("vision.lcd_rotation=");
    Serial.println(conf_lcd_rotation);
  } else {
    printErrorMessage(ini.getError());
    Serial.print("<vision.lcd_rotation> not found; use vision.target_fps.default=");
    Serial.println(conf_lcd_rotation);
  }
  if (ini.getValue("vision", "target_fps", conf_buf, buf_len)) {
    conf_target_fps = String(conf_buf).toInt();
    Serial.print("vision.target_fps=");
    Serial.println(conf_target_fps);
  } else {
    printErrorMessage(ini.getError());
    Serial.print("<vision.target_fps> not found; use vision.target_fps.default=");
    Serial.println(conf_target_fps);
  }

  digitalWrite(PIN_LED, HIGH); // startup blink 
  
  if (rst_reason == DEEPSLEEP_RESET) {
    
    gfx->setRotation(conf_lcd_rotation);
    
    // wakeup by accel. or timer
    // play GIF
    ledcWrite(1, lcd_pwm_fir_filter(get_lcd_brightness_by_adc(PIN_SENSOR_ADC)));

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
      Serial.println("wakeup by ext0");
      lastTappedCount = bootCount;
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
      Serial.println("wakeup by timer");
    } else {
      Serial.println("wakeup by yjsnpi");
    }
    mjpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_SIZE);
    if (!mjpeg_buf) {
      Serial.println(F("mjpeg_buf malloc failed!"));
    }
    vFile = FILE_SYSTEM.open(conf_gifname);
    if (!vFile || vFile.isDirectory()) {
      Serial.println(F("ERROR: Failed to open GIF file for reading"));
    } else {
      do {
/*      if (!vFile.seek(0)) {
          Serial.println(F("seek failed"));
          delay(500);
          continue;
        }*/
        Serial.println(F("MJPEG video start"));
        start_ms = millis();
        curr_ms = millis();
        next_frame_ms = start_ms + (++next_frame * 1000 / conf_target_fps / 2);
        mjpeg.setup(vFile, mjpeg_buf, (Arduino_TFT *)gfx, false);
        while (mjpeg.readMjpegBuf()) {
          curr_ms = millis();
          if (millis() < next_frame_ms) {
            // Play video
            mjpeg.drawJpg();
            int remain_ms = next_frame_ms - millis();
            if (remain_ms > 0) {
              delay(remain_ms);
            }
          } else {
            //Serial.println(F("Skip frame")); 
          }
  
          curr_ms = millis();
          next_frame_ms = start_ms + (++next_frame * 1000 / conf_target_fps);
  
          // check key
          digitalWrite(PIN_LED, digitalRead(PIN_KEY)); // sync LED to key
          last_key_state = key_state;
          key_state = digitalRead(PIN_KEY);
          if (last_key_state == LOW && key_state == LOW) {  
            lastTappedCount = bootCount;
            //if (conf_loop_mode) {
              lightsleep_ms = millis();
              while(digitalRead(PIN_KEY) == LOW);
              Serial.println("Light sleep start");
              ledcWrite(1, 0);
              digitalWrite(PIN_LED, LOW); delay(50);
              digitalWrite(PIN_LED, HIGH); 
              gfx->displayOff();
              gpio_wakeup_enable((gpio_num_t)PIN_KEY, GPIO_INTR_LOW_LEVEL);
              esp_sleep_enable_gpio_wakeup();
              esp_light_sleep_start();
              Serial.print("Wakeup from light sleep, ");
              Serial.print(lightsleep_ms); Serial.println("ms");
              gfx->begin();
              while(digitalRead(PIN_KEY) == LOW); // wait for key release
              lightsleep_ms = millis() - lightsleep_ms;
              start_ms += lightsleep_ms;
              curr_ms += lightsleep_ms;
            //}
          } 
  
          SD.begin(PIN_SD_CS);  // FUCK! THAT! MAGIC!!!就这一行调了一晚上
          // sync lcd brightness
          ledcWrite(1, lcd_pwm_fir_filter(get_lcd_brightness_by_adc(PIN_SENSOR_ADC)));
        }
        //Serial.println(F("Rewind"));
      } while (conf_loop_mode);
      Serial.println(F("MJPEG video end"));
      vFile.close();
    }
    
    digitalWrite(PIN_LED, HIGH);
    Serial.println(F("deep sleep"));
    ledcWrite(1, 0);
    ledcDetachPin(PIN_TFT_BL);
    delay(20);
    gfx->displayOff();
    if (bootCount - lastTappedCount < DEEP_SLEEP_SHORT_CNT) {
      esp_sleep_enable_timer_wakeup(DEEP_SLEEP_SHORT_S * uS_TO_S_FACTOR);
    } else {
      esp_sleep_enable_timer_wakeup(DEEP_SLEEP_LONG_S * uS_TO_S_FACTOR);
    }

    set_io_before_deep_sleep();
    esp_deep_sleep_start();

  } else {
    gfx->begin();
    gfx->fillScreen(LIGHTGREY);SD.begin(PIN_SD_CS);
    // poweron reset, start wi-fi ap
    Serial.println("Power on reset (maybe); ");
    Serial.print("Start ");
    Serial.println(wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_MINUS_1dBm);
    WiFi.begin(wifi_ssid, wifi_pwd);
    ledcWrite(1, LCD_BACKLIGHT_MIN_8B);  // brightness
    
    int wifi_cnt = 0;
    while (WiFi.status() != WL_CONNECTED ) {
      digitalWrite(PIN_LED, led_status);
      led_status = (led_status == HIGH)? LOW: HIGH;
      delay(250);
      Serial.print(".");
      wifi_cnt++;
      if (wifi_cnt > 4 * WIFI_CONNECT_TIMEOUT_S) {
        Serial.println("WiFi timeout");
        break;
      }
      if (digitalRead(PIN_KEY) == LOW) {
        delay(20);
        if (digitalRead(PIN_KEY) == LOW) {
          Serial.println("Wi-Fi skipped by key");
          break;
        }
      }
    }
    digitalWrite(PIN_LED, HIGH);
    server.on("/status", HTTP_GET, getStatus);
    server.on("/list", HTTP_GET, printDirectory);
    server.on("/edit", HTTP_DELETE, handleDelete);
    server.on("/edit", HTTP_PUT, handleCreate);
    server.on("/edit", HTTP_POST, []() {
      returnOK();
    }, handleFileUpload);
    server.onNotFound(handleNotFound);
    server.on("/update", HTTP_POST, handleOTAresult, handleOTA);

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      server.begin();
      if (!MDNS.begin(wifi_host)) {
        Serial.println("Error setting up MDNS responder!");
      }
      MDNS.addService("http", "tcp", 80);
      
      gfx->fillScreen(BLUE); SD.begin(PIN_SD_CS);
    } else {
      // wifi not connected
    }
  }
}

// 这个setup好tm长啊

void loop()
{
  bool exit_loop = false;
  
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    delay(2);//allow the cpu to switch to other tasks
    
    ledcWrite(1, lcd_pwm_fir_filter(get_lcd_brightness_by_adc(PIN_SENSOR_ADC)));
    
    // 退出webserver进入深睡条件：
    // (双击 & 盖住光传感器（或无光）& 屏幕水平放置) || (按下按键)
    // 不满足的话都不会退出
    if (digitalRead(PIN_KEY) == LOW) {
      delay(20);
      if (digitalRead(PIN_KEY) == LOW) {
        exit_loop = true;
      }
    }
    if (digitalRead(PIN_ACC_INT1) == HIGH) {
      DFRobot_LIS2DW12:: eTap_t tapEvent = acce.tapDetect();
      if (tapEvent == DFRobot_LIS2DW12::eDTap) {
        Serial.println("Double Tap Detected");
        int light_level = int(pow((analogRead(PIN_SENSOR_ADC) / 4096.0), 2.0) * 255.0);
        if (SENSOR_DISWIFI_THRESH_8B > light_level) {
          delay(100);
          light_level = int(pow((analogRead(PIN_SENSOR_ADC) / 4096.0), 2.0) * 255.0);
          if (SENSOR_DISWIFI_THRESH_8B > light_level) {
            if (abs(acce.readAccZ()) > ACCE_DISWIFI_Z_THRESH ) {
              exit_loop = true;
            }
          }
        }
      }
    } // 大 箭 头
  } else {
    // 如果没有连接就进到loop，说明 a.前面尝试连接wifi炒屎了 或 b.wifi断开了
    // 就直接睡眠了准备正常工作
    exit_loop = true;
  }

  if (exit_loop) {
    Serial.println(F("init done, deep sleep now"));
    gfx->begin();
    ledcWrite(1, LCD_BACKLIGHT_MIN_8B);
    gfx->fillScreen(GREEN); // 绿一下 提醒一下
    delay(500);
    gfx->fillScreen(BLACK);
    ledcWrite(1, 0);
    ledcDetachPin(PIN_TFT_BL);
    delay(5);
    gfx->displayOff();
    esp_sleep_enable_timer_wakeup(uS_TO_S_FACTOR);
    set_io_before_deep_sleep();
    esp_deep_sleep_start();        
  }
  
}
