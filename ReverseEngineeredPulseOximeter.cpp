#include <BLEDevice.h>

static BLEUUID serviceUUID("49535343-fe7d-4ae5-8fa9-9fafd205e455");
static BLEUUID charNotifyUUID("49535343-1e4d-4bd9-ba61-23c647249616");
static BLEUUID charWriteUUID("49535343-8841-43f4-a8d4-ecbe34729bb3");

static BLEAddress oximeterAddress("00:A0:50:4A:9B:7C");

BLEClient* pClient;
BLERemoteCharacteristic* pRemoteNotifyChar;
BLERemoteCharacteristic* pRemoteWriteChar;

bool connected = false;

void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  Serial.print("Data received: ");
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", pData[i]);
  }
  Serial.println();

  // Example: if your HR and SpO2 are at specific positions in the packet
  if (length >= 5) {
    uint8_t spo2 = pData[4];
    uint8_t hr = pData[3];
    Serial.printf("Heart Rate: %d bpm, SpO2: %d%%\n", hr, spo2);
  }
}

bool connectToOximeter() {
  Serial.println("Connecting to oximeter...");
  pClient = BLEDevice::createClient();
  if (!pClient->connect(oximeterAddress)) {
    Serial.println("Failed to connect!");
    return false;
  }
  Serial.println("Connected to oximeter.");

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("Service not found!");
    pClient->disconnect();
    return false;
  }
  Serial.println("Service found.");

  pRemoteNotifyChar = pRemoteService->getCharacteristic(charNotifyUUID);
  if (pRemoteNotifyChar == nullptr) {
    Serial.println("Notify characteristic not found!");
    pClient->disconnect();
    return false;
  }

  if (pRemoteNotifyChar->canNotify()) {
    pRemoteNotifyChar->registerForNotify(notifyCallback);
    Serial.println("Notify callback registered.");
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  BLEDevice::init("");
  connectToOximeter();
}

void loop() {
  if (!pClient->isConnected()) {
    Serial.println("Disconnected. Trying to reconnect...");
    delay(1000);
    connectToOximeter();
  }
  delay(1000);
}
