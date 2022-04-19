/*
   Vision V3.3 SD MJPEG WebServer OTA BLE DEMO
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
      Minimal SPIFFS (1.9MB with OTA/190kB SPIFFS), PSRAM Disable;
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
    loop_mode=false
    ble_en=false
    ble_mac=00:12:34:56:78:9a

   There are 3 working modes:
    a. Loop video A or B, controlled by iTag;
      you should: 
        set loop_mode = true
        set ble_en = true
        set ble_mac to your iTag MAC address
        upload /a_setup.mjpeg, /a_loop.mjpeg, /b_setup.mjpeg, /b_loop.mjpeg
    b. Loop one video;
      u should:
        upload video (e.g. hydro.mjpeg)
        set loop_mode = true
        set ble_en = false
        set video = hydro.mjpeg
    c. play video once, then deep sleep; wakeup by double tap;
      u should:
        upload video (e.g. hydro.mjpeg)
        set loop_mode = false
        set video = hydro.mjpeg
        
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
#define CONF_LOOP_MODE_DEFAULT true
#define DEEP_SLEEP_LONG_S  30
#define DEEP_SLEEP_SHORT_S  2
#define DEEP_SLEEP_SHORT_CNT  6
#define TEMP_PROTECT_HIGH_THRESH_12B  1840 // ~60°C @10k,B3380
#define LCD_BACKLIGHT_MIN_8B  32
#define LCD_PWM_FIR_LEN 32
#define VIDEO_A_SETUP   "/a_setup.mjpeg"
#define VIDEO_B_SETUP   "/b_setup.mjpeg"
#define VIDEO_A_LOOP    "/a_loop.mjpeg"
#define VIDEO_B_LOOP    "/b_loop.mjpeg"

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
#include "BLEDevice.h"


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
#define MJPEG_BUFFER_SIZE (240 * 240 * 2 / 4)

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_TFT_DC/* DC */, PIN_TFT_CS /* CS */, PIN_SCK, PIN_MOSI, PIN_MISO, VSPI, true );
//Arduino_GC9A01  *gfx = new Arduino_GC9A01(bus, PIN_TFT_RST, 2, true); 
Arduino_ST7789  *gfx = new Arduino_ST7789(bus, PIN_TFT_RST, 0, true, 240, 240, 0, 0, 0, 80);

DFRobot_LIS2DW12_I2C acce(&Wire, 0x19);   // sdo/sa0 internal pull-up


#include "MjpegClass.h"
static MjpegClass mjpeg;
uint8_t *mjpeg_buf;

unsigned long lightsleep_ms;
uint8_t lcd_pwm_filter[LCD_PWM_FIR_LEN] = {LCD_BACKLIGHT_MIN_8B};
uint8_t p_lcd_pwm_filter;

WebServer server(80);
static bool hasSD = false;
File uploadFile;
uint8_t lcd_brightness;
int led_status;

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int lastTappedCount = 0;

/*  BLE Connection to iTag. 
 *   
 *  BUG: cause reset when connect itag at the second time, and i don't know why 
 *       however the first connection is ok, if u keep that connection it's still usable
 *       
 *  This bunch of sh*t logic is designed to smoothly switching 
 *  between two video parts A & B when BLE got notified
 *  
 *  to enable this feature, you need 4 pieces of video:
 *  Length:     |--------------|--------------|--------------|--------------|
 *  Part:       |   A setup    |    A Loop    |   B Setup    |   B Loop     |  
 *  
 *  the BLE part: i just copied his code, no optimization
 *  See http://100-x-arduino.blogspot.com/2018/05/itag-i-arduino-ide-czy-esp32-otworzy.html
 *  
 *  About how to fix the memory leak of the iTag example, see
 *    https://github.com/nkolban/esp32-snippets/issues/1006
 *    https://github.com/nkolban/esp32-snippets/issues/786
 *    
*/
static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID charUUID("0000ffe1-0000-1000-8000-00805f9b34fb"); 
static BLEAddress *pServerAddress;
static boolean ble_doconnect = false;
static boolean ble_connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEClient* pClient;
bool deviceBleConnected = false;
bool ble_key_pressed = false;   // when ble got notify
TaskHandle_t BLE_TaskHandle;

// led indicator
TaskHandle_t LED_TaskHandle;
xQueueHandle led_queue;

class MyClientCallbacks: public BLEClientCallbacks {
    void onConnect(BLEClient *pClient) {
      deviceBleConnected = true;
      Serial.println("BLE: connected to my server");
    };
    void onDisconnect(BLEClient *pClient) {
      pClient->disconnect();
      deviceBleConnected = false;
      Serial.println("BLE: disconnected from my server");
      ble_connected = false;
    }
};

MyClientCallbacks* callbacks = new MyClientCallbacks();

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData, size_t length, bool isNotify) {
  Serial.println("BLE: Notify from iTAG");  
  ble_key_pressed = true;
}

bool connectToServer(BLEAddress pAddress) {
  if (pClient != nullptr) {
    delete(pClient);
  }
  pClient = BLEDevice::createClient();  
  pClient->setClientCallbacks(callbacks); 
  pClient->connect(pAddress); 

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID); 
  if (pRemoteService == nullptr) {
    Serial.print("BLE: Failed to find our service UUID ");    
    return false;
  }
  Serial.println("BLE: Found service " + String(pRemoteService->toString().c_str())); 

  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("BLE: Failed to find our characteristic UUID ");    
    return false;
  }
  Serial.println("BLE: Found characteristic" + String(pRemoteCharacteristic->toString().c_str()));

  //std::string value = pRemoteCharacteristic->readValue();
  //Serial.print("BLE: Read characteristic value: ");  Serial.println(value.c_str());

  //const uint8_t bothOff[]        = {0x0, 0x0};
  const uint8_t notificationOn[] = {0x1, 0x0};
  //const uint8_t indicationOn[]   = {0x2, 0x0};
  //const uint8_t bothOn[]         = {0x3, 0x0};

  pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true); 
  Serial.println("BLE: Notification ON ");
  pRemoteCharacteristic->registerForNotify(notifyCallback); // Callback function when notification
  Serial.println("BLE: Notification Callback ");
  return true;
}

// pixel drawing callback
static int drawMCU(JPEGDRAW *pDraw) {
  // Serial.printf("Draw pos = %d,%d. size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  gfx->draw16bitBeRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
} /* drawMCU() */

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
  //bool is_gzipped = false;
  
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

  // for ffmpeg.wasm
  // 试过了 这不太现实 并且太慢 算了
  //server.sendHeader("Cross-Origin-Opener-Policy", "same-origin");
  //server.sendHeader("Cross-Origin-Embedder-Policy", "require-corp");
  
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
  //stat += "SD_Type,";
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
  //stat += "Total_MB,";
  stat += String(int(FILE_SYSTEM.totalBytes() / (1024 * 1024)));
  //stat += "MB,Used_MB,";
  stat += "MB,";
  stat += String(int(FILE_SYSTEM.usedBytes() / (1024 * 1024)));
  stat += "MB,";
  // Battery info
  //stat += "Battery_ADC,";
  stat += analogRead(PIN_ADC_BAT);
  //stat += ",Charger,";
  stat += ",";
  if (digitalRead(PIN_BAT_CHRG) == LOW) {
    stat += "charging,";
  } else if (digitalRead(PIN_BAT_STDBY) == LOW) {
    stat += "full,";
  } else {
    stat += "not_charging,";
  }

  // light sensor info
  //stat += "Light_ADC,";
  stat += analogRead(PIN_SENSOR_ADC);

  // acce.
  stat += ",X,";
  stat += acce.readAccX();
  stat += ",Y,";
  stat += acce.readAccY();
  stat += ",Z,";
  stat += acce.readAccZ();
  stat += ",";
  
  // firmware info
  stat += __TIME__ ;
  stat += ",";
  stat += __DATE__ ;
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
  uint32_t lcd_pwm_filter_sum = 0;
  
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

void led_blink_task(void * par) {
  int led_blink_cnt = 1;
  int i = 0;
  BaseType_t x_stat;
  TickType_t x_timeout = pdMS_TO_TICKS(100);

  while(1) {
    x_stat = xQueueReceive(led_queue, &led_blink_cnt, x_timeout);
    //if (x_stat != pdPASS) {
    //  led_blink_cnt = 1;
    //}
    for (i=0; i<led_blink_cnt; i++) {
      digitalWrite(PIN_LED, LOW);
      vTaskDelay(pdMS_TO_TICKS(100));
      digitalWrite(PIN_LED, HIGH);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(1000-(200*led_blink_cnt)));
  }
}

// a thread to: (1) keep ble connected (2) clear the status
// key status is not here, it will be updated in notifyCallback()
void ble_task_loop(void * par) {

  //bool last_ble_key_pressed = false;
  ble_connected = false;
  deviceBleConnected = false;
  String newValue = "T";  
  //Serial.print("BLE: task created on Core ");
  //Serial.println(xPortGetCoreID());

  while(1) {
    if (ble_connected == false) {
      if (connectToServer(*pServerAddress)) {
        ble_connected = true; 
        Serial.println("BLE: Server UP");
      } else {
        Serial.println("BLE: Server DOWN");
        deviceBleConnected = false;
        //break;  // hot glue for BLE connection bug
      }
    }


    //last_ble_key_pressed = ble_key_pressed;
    if (deviceBleConnected) {
      //if ((!ble_key_pressed) && last_ble_key_pressed) {   // if notify is cleared by main task 
        //Serial.print("+");        
        // then send clear message to itag
        pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());  
      //} 
    }
    
    delay(500); 
  }

  // 由于我也不知道是itag的状态机bug 还是抄来的代码bug 还是我写的bug，
  // itag断连后重连就会崩
  // 所以就暂且让它能够在每次开机的第一次连接成功。。屎山上打补丁
  while(1) {
    Serial.println("BLE: unexpected error, needs reset");
    delay(10000);     // hot glue for BLE connection bug
  }
}

void lock_screen_handler(bool reset_ble) {
  while(digitalRead(PIN_KEY) == LOW);
  Serial.println("Light sleep start");
  ledcWrite(1, 0);
  digitalWrite(PIN_LED, LOW); delay(50);
  digitalWrite(PIN_LED, HIGH); 
  gfx->displayOff();
  lightsleep_ms = millis();
  gpio_wakeup_enable((gpio_num_t)PIN_KEY, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
  lightsleep_ms = millis() - lightsleep_ms;
  Serial.print("Wakeup from light sleep, ");
  Serial.print(lightsleep_ms); Serial.println("ms");
  gfx->begin();
  if (reset_ble) {
    vTaskDelete(BLE_TaskHandle);
    BLEDevice::init(""); 
    xTaskCreatePinnedToCore(
      ble_task_loop, "BLE_task",
      8192, NULL, 1,
      &BLE_TaskHandle, 0);
  }
  while(digitalRead(PIN_KEY) == LOW); // wait for key release
  delay(50);  // ~20fps, 1 frame
}

void check_battery_then_deep_sleep() {
  if (analogRead(PIN_ADC_BAT) < BAT_ADC_THRESH_LOW) {
    // just go sleep if battery low
    Serial.println("battery low, deep sleep start");
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_LONG_S * uS_TO_S_FACTOR);
    set_io_before_deep_sleep();
    esp_deep_sleep_start();
  } 
}

void setup()
{
  int i, k;
  String conf_gifname = CONF_GIFNAME_DEFAULT;
//  bool conf_gravity = CONF_GRAVITY_DEFAULT;
  int conf_lcd_rotation = CONF_LCD_ROTATION_DEFAULT;
  bool conf_loop_mode = CONF_LOOP_MODE_DEFAULT;
  bool conf_ble_en = false;
  bool video_part_a = true;   // true=partA, false=partB
  int last_key_state = HIGH;
  int key_state = HIGH;
  File * pvFile;
  File vFiles[4];
  int led_blink_cnt = 0;

  WiFi.mode(WIFI_OFF);
  bootCount++;
  Serial.begin(115200);
  Serial.print("Vision v3.3, SD MJPEG WebServer OTA demo; bootcounter: "); Serial.print(bootCount);
  Serial.print(", lastTapped: "); Serial.println(lastTappedCount);
  Serial.println("fw: "__DATE__" "__TIME__);

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
  
  //Serial.print("accel. chip id : "); Serial.println(acce.getID(), HEX);
  //Serial.println("Init accel.");
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
  
  check_battery_then_deep_sleep();
  
  // enable 3v3b & init spi for sdcard
  digitalWrite(PIN_LCD_PWR_EN, HIGH);
  delay(20);

  gfx->begin();
  gfx->fillScreen(BLACK);
  p_lcd_pwm_filter = 0;
  ledcAttachPin(PIN_TFT_BL, 1);
  ledcSetup(1, 12000, 8);
  ledcWrite(1, 0);

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS) && !SD.begin(PIN_SD_CS)) {
    Serial.println(F("ERROR: SD card mount failed!"));
  } else {
    hasSD = true;
    //Serial.println(F("SD card mounted"));
    uint8_t cardType = FILE_SYSTEM.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("No SD_MMC card attached");
    }
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
      conf_file.print("lcd_rotation="); conf_file.println(CONF_LCD_ROTATION_DEFAULT);
      conf_file.print("loop_mode="); conf_file.println(CONF_LOOP_MODE_DEFAULT? "true": "false");
      conf_file.println("ble_en=false"); 
      conf_file.println("ble_mac=00:12:34:56:78:9a"); 
      conf_file.println("# when enable ble ctrl, rename video to "VIDEO_A_SETUP", "VIDEO_B_SETUP", "VIDEO_A_LOOP", "VIDEO_A_LOOP";"); 
      conf_file.close();
    }
  }
  
  IniFile ini(CONFIG_FILENAME);
  if (!ini.open()) {
    Serial.println("config file open error");
  }
  if (!ini.validate(conf_buf, buf_len)) {
    Serial.println("config file not valid");
    printErrorMessage(ini.getError());
  }
  //Serial.println("Reading config file: ");
  if (ini.getValue("vision", "video", conf_buf, buf_len)) {
    conf_gifname = String(conf_buf);
    //Serial.print("vision.video=");
    //Serial.println(conf_gifname);
  } else {
    printErrorMessage(ini.getError());
    Serial.print("<vision.video> not found; use vision.video.default=");
    Serial.println(conf_gifname);
  }
  if (ini.getValue("vision", "ble_en", conf_buf, buf_len, conf_ble_en)) {
    //Serial.print("vision.ble_en=");
    //Serial.println(conf_ble_en ? "true" : "false");
  } else {
    printErrorMessage(ini.getError());
    Serial.println("<vision.ble_en> not found; use vision.ble_en.default=false");
  }
  if (conf_ble_en) {
    if (ini.getValue("vision", "ble_mac", conf_buf, buf_len)) {
      pServerAddress = new BLEAddress(String(conf_buf).c_str());
      //Serial.print("vision.ble_mac=");
      //Serial.println(String(conf_buf));
    } else {
      printErrorMessage(ini.getError());
      Serial.println("<vision.ble_mac> not found; disable ble.");
      conf_ble_en = false;
    }
  }
/*
  if (ini.getValue("vision", "gravity", conf_buf, buf_len, conf_gravity)) {
    Serial.print("vision.gravity=");
    Serial.println(conf_gravity ? "true" : "false");
  } else {
    printErrorMessage(ini.getError());
    Serial.print("<vision.gravity> not found; use vision.gravity.default=");
    Serial.println(conf_gravity ? "true" : "false");
  }*/
  if (ini.getValue("vision", "loop_mode", conf_buf, buf_len, conf_loop_mode)) {
    //Serial.print("vision.loop_mode=");
    //Serial.println(conf_loop_mode ? "true" : "false");
  } else {
    printErrorMessage(ini.getError());
    Serial.print("<vision.loop_mode> not found; use vision.loop_mode.default=");
    Serial.println(conf_loop_mode ? "true" : "false");
  }
  conf_ble_en = conf_loop_mode? conf_ble_en: false;
  if (ini.getValue("vision", "lcd_rotation", conf_buf, buf_len)) {
    conf_lcd_rotation = String(conf_buf).toInt();
    //Serial.print("vision.lcd_rotation=");
    //Serial.println(conf_lcd_rotation);
  } else {
    printErrorMessage(ini.getError());
    Serial.print("<vision.lcd_rotation> not found; use vision.lcd_rotation.default=");
    Serial.println(conf_lcd_rotation);
  }

  digitalWrite(PIN_LED, HIGH); // startup blink 
  
  if (rst_reason == DEEPSLEEP_RESET) {
    
    gfx->setRotation(conf_lcd_rotation);
    
    // init ble
    if (conf_ble_en) {
      BLEDevice::init(""); 
      xTaskCreatePinnedToCore(
        ble_task_loop, "BLE_task",
        8192, NULL, 1,
        &BLE_TaskHandle, 0);
    }

    // set led task
    led_queue = xQueueCreate(8, sizeof(int));
    xTaskCreatePinnedToCore(
        led_blink_task, "LED_task",
        8192, NULL, 1,
        &LED_TaskHandle, 0);

    // wakeup by accel. or timer
    // play GIF
    ledcWrite(1, lcd_pwm_fir_filter(get_lcd_brightness_by_adc(PIN_SENSOR_ADC)));

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
      lastTappedCount = bootCount;
    } 
    
    mjpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_SIZE);
    if (!mjpeg_buf) {
      Serial.println(F("mjpeg_buf malloc failed!"));
    }
    if (conf_ble_en) {
      vFiles[0] = FILE_SYSTEM.open(VIDEO_A_SETUP);
      vFiles[1] = FILE_SYSTEM.open(VIDEO_A_LOOP);
      vFiles[2] = FILE_SYSTEM.open(VIDEO_B_SETUP);
      vFiles[3] = FILE_SYSTEM.open(VIDEO_B_LOOP);
      pvFile = &vFiles[1];
      for (i=0; i<4; i++) {
        if (!vFiles[i]) {
          conf_ble_en = false;
        }
      }
    } 
    if (!conf_ble_en) {
      vFiles[0] = FILE_SYSTEM.open(conf_gifname);
      pvFile = &vFiles[0];
    }
    if (!(*pvFile) || pvFile->isDirectory()) {
      Serial.println(F("ERROR: Failed to open GIF file for reading"));
    } else {
      mjpeg.setup(pvFile, mjpeg_buf, drawMCU, false, true);
      Serial.println("MJPEG video start");
      
      if (conf_loop_mode) {
        while (1) {
          mjpeg.readMjpegBuf();                 // 1. read, decode & render jpeg frame;
          mjpeg.drawJpg();
          //Serial.print(".");
          
          if (ble_key_pressed) {                // 2. check BLE itag status
            ble_key_pressed = false;
            if (pvFile == &vFiles[1] || pvFile == &vFiles[3]) {
              // only accept switch a/b when looping
              video_part_a = !video_part_a;    
              //Serial.print("ble video switch ");
              Serial.println(video_part_a? "part a": "part b");
              led_blink_cnt = (video_part_a)? 1: 2;
              xQueueSendToFront(led_queue, &led_blink_cnt, pdMS_TO_TICKS(1));
            }
          }

          check_battery_then_deep_sleep();      // 3. check battery voltage

          last_key_state = key_state;           // 4. check key 'lock screen' 
          key_state = digitalRead(PIN_KEY);
          if (last_key_state == LOW && key_state == LOW) {  
            lastTappedCount = bootCount;
            //lock_screen_handler(conf_ble_en);
            lock_screen_handler(false);
          } 

          SD.begin(PIN_SD_CS);                  // 5. hardware magics
          ledcWrite(1, lcd_pwm_fir_filter(get_lcd_brightness_by_adc(PIN_SENSOR_ADC)));
  
          // 6. set file position
          if (conf_ble_en) {
            if (video_part_a) {
              // 6.1 needs to switch to video A
              // 6.1.1 it's looping A now, then just loop A
              if (pvFile == &vFiles[1] && (!pvFile->available()) ) {
                mjpeg.updateFilePointer(pvFile); 
                pvFile->seek(0); 
                //Serial.println("from A loop end to A loop");
              }
              // 6.1.2 it's looping B, when it ends(not available), jump to A setup
              if (pvFile == &vFiles[3] && (!pvFile->available()) ) { 
                pvFile = &vFiles[0];
                mjpeg.updateFilePointer(pvFile);
                pvFile->seek(0);
                //Serial.println("from B loop end to A setup");
              }
            } else {
              // 6.2 needs to switch to video B
              // 6.2.1 it's looping B now, keep it
              if (pvFile == &vFiles[3] && (!pvFile->available()) ) {
                mjpeg.updateFilePointer(pvFile);
                pvFile->seek(0);
                //Serial.println("from B loop to B loop");
              }
              // 6.2.2 or "when A loop ends, jump to B setup" 
              if (pvFile == &vFiles[1] && (!pvFile->available()) ) {
                pvFile = &vFiles[2];
                mjpeg.updateFilePointer(pvFile);
                pvFile->seek(0);
                //Serial.println("from A loop to B setup");
              }
            }
            // and, play loop after setup
            if (pvFile == &vFiles[0] && (!pvFile->available()) ) {
              pvFile = &vFiles[1];
              pvFile->seek(0);
              mjpeg.updateFilePointer(pvFile);
              //Serial.println("from A setup to A loop");
            }
            if (pvFile == &vFiles[2] && (!pvFile->available()) ) {
              pvFile = &vFiles[3];
              pvFile->seek(0);
              mjpeg.updateFilePointer(pvFile);
              //Serial.println("from B setup to B loop");
            }
            
          } else {
            if (!pvFile->available()) { 
              pvFile->seek(0);     // ble disable, simple loop
              mjpeg.updateFilePointer(pvFile);
            }
          }
        }  
      } else {
        // loop_mode=false
        while (pvFile->available()) {
          mjpeg.readMjpegBuf();
          mjpeg.drawJpg();
          last_key_state = key_state;
          key_state = digitalRead(PIN_KEY);
          if (last_key_state == LOW && key_state == LOW) {  
            lastTappedCount = bootCount;
          } 
          SD.begin(PIN_SD_CS);  
          ledcWrite(1, lcd_pwm_fir_filter(get_lcd_brightness_by_adc(PIN_SENSOR_ADC)));
        }
      }// loop_mode=false
      Serial.println("MJPEG video end");
      //vFile.close();
      pvFile = NULL;
      for (k=0; k<4; k++) {
        if (!vFiles[k]) {
          vFiles[k].close();
        }
      }
    }

    ledcWrite(1, 0);
    ledcDetachPin(PIN_TFT_BL);
    digitalWrite(PIN_LED, HIGH);
    Serial.println(F("deep sleep"));

    if (bootCount - lastTappedCount < DEEP_SLEEP_SHORT_CNT) {
      esp_sleep_enable_timer_wakeup(DEEP_SLEEP_SHORT_S * uS_TO_S_FACTOR);
    } else {
      esp_sleep_enable_timer_wakeup(DEEP_SLEEP_LONG_S * uS_TO_S_FACTOR);
    }
    gfx->displayOff();
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
    
    // 退出webserver进入深睡条件：按下按键
    if (digitalRead(PIN_KEY) == LOW) {
      delay(20);
      if (digitalRead(PIN_KEY) == LOW) {
        exit_loop = true;
      }
    }
  } else {
    // 如果没有连接就进到loop，说明 a.前面尝试连接wifi炒屎了 或 b.wifi断开了
    // 就直接睡眠了准备正常工作
    exit_loop = true;
  }

  if (exit_loop) {
    Serial.println(F("init done, deep sleep now"));
    gfx->begin();
    ledcWrite(1, LCD_BACKLIGHT_MIN_8B);
    gfx->fillScreen(GREEN); 
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
