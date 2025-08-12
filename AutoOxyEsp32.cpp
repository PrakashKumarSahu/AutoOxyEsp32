// Schematic & Pin Details
// Component	ESP32 Pin	Notes
// Stepper Driver STEP	GPIO 18	Step pulse input
// Stepper Driver DIR	GPIO 19	Direction input
// Stepper Driver EN	GPIO 21	Enable pin (check active LOW/HIGH)
// Relay Module IN	GPIO 22	Controls buzzer ON/OFF via relay
// Relay Module VCC	5V or 3.3V (check relay specs)	Power for relay coil
// Relay Module GND	GND	Common ground
// Buzzer	Connected to relay's NO (Normally Open) and power line	Relay switches buzzer ON/OFF
// BLE Oximeter	Wireless	Connects via BLE

// Notes for schematic:
// Connect ESP32 GPIO 22 to the relay module IN pin.

// Relay module VCC and GND to ESP32 power and ground (make sure voltage matches relay specs).

// Connect your buzzer’s positive line to relay’s Normally Open (NO) terminal, and the other side to the power supply positive (e.g., 5V).

// Buzzer negative goes to common ground.



#include <BLEDevice.h>
#include <Arduino.h>

// ================= BLE UUIDs =================
static BLEUUID serviceUUID("49535343-fe7d-4ae5-8fa9-9fafd205e455");
static BLEUUID charNotifyUUID("49535343-1e4d-4bd9-ba61-23c647249616");
static BLEUUID charWriteUUID("49535343-8841-43f4-a8d4-ecbe34729bb3");

// Replace with your oximeter's MAC address
static BLEAddress oximeterAddress("00:A0:50:4A:9B:7C");

// BLE client variables
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteNotifyChar = nullptr;

bool connected = false;
uint8_t currentSpO2 = 0;
uint8_t currentHR = 0;

// ================= Stepper Motor (A4988) =================
// Connect these pins to your ESP32
#define STEP_PIN    18
#define DIR_PIN     19
#define ENABLE_PIN  21

// ================= Buzzer Relay =================
#define BUZZER_RELAY_PIN 22

// Stepper config
const int stepsPerRev = 200;  // Adjust to your motor specs
const int stepDelay = 800;    // microseconds delay between steps

// Valve position tracking
int valvePosition = 0; // Number of steps opened; prevent over-rotating
const int valveMaxSteps = 500; // Max steps valve can open

// Timing
unsigned long lastBleDataTime = 0;
const unsigned long bleTimeout = 30000; // 30 seconds timeout without data triggers reconnect and alarm

// ================== Function declarations ==================
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
void adjustOxygenValve();
void rotateStepper(bool open, int steps);
bool connectToOximeter();
void soundAlarm(bool on);
void disconnectCleanup();

// ================== BLE Notification Callback ==================
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

  if (length >= 5) {
    currentSpO2 = pData[4];
    currentHR = pData[3];
    Serial.printf("Heart Rate: %d bpm, SpO2: %d%%\n", currentHR, currentSpO2);

    lastBleDataTime = millis(); // Reset timeout timer
    soundAlarm(false);          // Turn off alarm if previously on

    adjustOxygenValve();
  } else {
    Serial.println("Received data too short, ignoring.");
  }
}

// ================= Function to Control Oxygen Valve =================
void adjustOxygenValve() {
  if (currentSpO2 < 98) {
    Serial.println("Low SpO2 detected! Increasing oxygen flow...");
    int stepsToMove = min(50, valveMaxSteps - valvePosition);
    if (stepsToMove > 0) {
      rotateStepper(true, stepsToMove); // Open valve
      valvePosition += stepsToMove;
    } else {
      Serial.println("Valve already fully opened.");
    }
  } 
  else if (currentSpO2 > 98) {
    Serial.println("High SpO2 detected! Decreasing oxygen flow...");
    int stepsToMove = min(50, valvePosition);
    if (stepsToMove > 0) {
      rotateStepper(false, stepsToMove); // Close valve
      valvePosition -= stepsToMove;
    } else {
      Serial.println("Valve already fully closed.");
    }
  } 
  else {
    Serial.println("SpO2 in normal range. No adjustment.");
  }
}

// ================= Function to Rotate Stepper =================
// Non-blocking rotation (simple version)
void rotateStepper(bool open, int steps) {
  digitalWrite(DIR_PIN, open ? HIGH : LOW);
  for (int i = 0; i < steps; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(stepDelay);
  }
}

// ================= BLE Connect Function =================
bool connectToOximeter() {
  Serial.println("Connecting to oximeter...");

  if (pClient) {
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
  }
  
  pClient = BLEDevice::createClient();
  if (!pClient->connect(oximeterAddress)) {
    Serial.println("Failed to connect!");
    connected = false;
    return false;
  }
  Serial.println("Connected to oximeter.");
  connected = true;

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("Service not found!");
    disconnectCleanup();
    return false;
  }
  Serial.println("Service found.");

  pRemoteNotifyChar = pRemoteService->getCharacteristic(charNotifyUUID);
  if (pRemoteNotifyChar == nullptr) {
    Serial.println("Notify characteristic not found!");
    disconnectCleanup();
    return false;
  }

  if (pRemoteNotifyChar->canNotify()) {
    pRemoteNotifyChar->registerForNotify(notifyCallback);
    Serial.println("Notify callback registered.");
  }

  lastBleDataTime = millis();
  return true;
}

// ================= Cleanup on disconnect =================
void disconnectCleanup() {
  if (pClient) {
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
  }
  connected = false;
  pRemoteNotifyChar = nullptr;
}

// ================= Sound alarm via relay-controlled buzzer =================
void soundAlarm(bool on) {
  digitalWrite(BUZZER_RELAY_PIN, on ? HIGH : LOW);
  if (on) Serial.println("Alarm ON!");
  else Serial.println("Alarm OFF!");
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);

  // Stepper motor pins
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW); // enable driver (check your driver for active LOW or HIGH)

  // Buzzer relay pin
  pinMode(BUZZER_RELAY_PIN, OUTPUT);
  soundAlarm(false); // start with alarm off

  // Init BLE
  BLEDevice::init("");

  if (!connectToOximeter()) {
    Serial.println("Initial connection failed.");
  }
}

// ================= Main loop =================
void loop() {
  // Reconnect logic with delay and timeout
  if (!connected || (pClient && !pClient->isConnected())) {
    static unsigned long lastAttemptTime = 0;
    unsigned long now = millis();

    if (now - lastAttemptTime > 5000) { // try reconnect every 5 seconds
      Serial.println("Disconnected or not connected. Trying to reconnect...");
      if (connectToOximeter()) {
        Serial.println("Reconnected.");
        soundAlarm(false);
      } else {
        Serial.println("Reconnect failed.");
      }
      lastAttemptTime = now;
    }
  }

  // Check for BLE data timeout to trigger alarm
  if (connected && (millis() - lastBleDataTime > bleTimeout)) {
    Serial.println("No BLE data received for 30 seconds! Triggering alarm.");
    soundAlarm(true);
  }

  delay(100); // Small delay for loop stability
}
