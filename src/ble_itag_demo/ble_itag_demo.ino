/*
 * A simple BLE iTag example
 * 
 * tested on: 
 *  ESP32 Arduino V1.0.6
 *  Widora AIR (ESP32)
 *  
 * You should change pServerAddress to your iTag's MAC address
 * (nRF Connect may help)
 * 
 * Original from http://100-x-arduino.blogspot.com/
 * modified by @libc0607
 */
 
/*  
 * 一个简单的将蓝牙 iTag 用作按键输入的示例
 * 每次按下 iTag 上的按钮，都会在串口打印 FreeHeap
 * 
 * 在 
 *    软件：ESP32 Arduino V1.0.6
 *    硬件：Widora AIR (ESP32)
 * 上测试通过
 * 
 * 需要根据你的硬件修改 iTag 的蓝牙 MAC 地址 pServerAddress
 * （可以用 nRF Connect 查看
 * 按下按钮后代码会执行到 notifyCallback 中
 * 蓝牙连接状态由 ble_task_loop 任务维护
 * 
 * 大部分参考自 http://100-x-arduino.blogspot.com/ 
 * 做了一点点修补，瞎改，混乱
 */
 
#include "BLEDevice.h"

// config   
static BLEAddress *pServerAddress = new BLEAddress("ff:ff:10:0c:e6:49");

static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID charUUID("0000ffe1-0000-1000-8000-00805f9b34fb"); 
static bool ble_doconnect = false;
static bool ble_connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEClient* pClient;
static bool deviceBleConnected = false;
TaskHandle_t BLE_TaskHandle;

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
  Serial.print("BLE: Notify from iTag, FreeHeap = "); Serial.println(ESP.getFreeHeap());
}

bool connectToServer(BLEAddress pAddress) {
  if (pClient != nullptr) {
    delete(pClient);
  }
  pClient = BLEDevice::createClient();  
  pClient->setClientCallbacks(callbacks); 
  pClient->connect(pAddress); 
  if (!pClient->isConnected()) {
    return false;
  }
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID); 
  if (pRemoteService == nullptr) {
    Serial.print("BLE: Failed to find our service UUID");    
    return false;
  }
  Serial.println("BLE: " + String(pRemoteService->toString().c_str())); 

  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("BLE: Failed to find our characteristic UUID");    
    return false;
  }
  Serial.println("BLE: " + String(pRemoteCharacteristic->toString().c_str()));

  const uint8_t notificationOn[] = {0x1, 0x0};
  pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true); 
  pRemoteCharacteristic->registerForNotify(notifyCallback); 
  Serial.println("BLE: Notification ON");
  return true;
}

void ble_task_loop(void * par) {
  ble_connected = false;
  deviceBleConnected = false;
  String newValue = "T";            // i don't know why, but it works
  while(1) {
    if (ble_connected == false) {
      delay(500);                   // wait for iTag init
      if (connectToServer(*pServerAddress)) {
        ble_connected = true; 
        Serial.println("BLE: Server UP");
      } else {
        Serial.println("BLE: Server DOWN");
        deviceBleConnected = false;
      }
    }
    if (deviceBleConnected) {
      pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());  
    }
    delay(500); 
  }
}

void setup() {
  Serial.begin(115200);
  
  ble_connected = false;
  deviceBleConnected = false;
  BLEDevice::init(""); 
  xTaskCreatePinnedToCore(
        ble_task_loop, "BLE_task",
        8192, NULL, 1,
        &BLE_TaskHandle, 0
  );
}


void loop() {
  delay(500);   // go on other tasks
} 
