/*
  OPTIONAL — ESP32 BLE RC Car firmware

  This is an ALTERNATIVE to src/main.ino. Flash only ONE firmware to the board.

    src/main.ino      -> Bluetooth Classic (SPP). Works with generic phone
                         "Bluetooth terminal" apps.
    src/main_ble.ino  -> Bluetooth Low Energy (BLE), Nordic UART Service.
                         Works with the included web controller (app/index.html)
                         in any Web Bluetooth browser: Android Chrome, desktop
                         Chrome/Edge. NOTE: iOS Safari does NOT support Web
                         Bluetooth, so iPhones need a Web-Bluetooth-capable app.

  Command protocol is IDENTICAL to the classic version:
    F        -> move forward at current speed
    B        -> move backward at current speed
    S        -> stop motors
    L        -> steer left
    R        -> steer right
    C        -> center steering
    V<0-255> -> set motor speed, example: V160
    A<0-180> -> set servo angle, example: A95

  Safety:
    - Motors stop if no command arrives for AUTO_STOP_MS.
    - Motors also stop immediately if the BLE client disconnects.
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ESP32Servo.h>

// ========================== BLE (Nordic UART Service) ==========================
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHAR_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // phone -> ESP32 (write)
#define NUS_CHAR_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // ESP32 -> phone (notify)

const char* BLE_NAME = "ESP32_RC_Car";

BLECharacteristic* txCharacteristic = nullptr;
bool deviceConnected = false;

Servo steeringServo;

// ========================== PROJECT CONFIG ==========================
// Keep these in sync with src/main.ino so wiring is identical either way.
// Motor driver: L298N. MOTOR_A_PWM -> ENA, MOTOR_B_PWM -> ENB (remove the
// ENA/ENB module jumpers so these GPIOs control speed). No standby pin.
const int MOTOR_A_IN1 = 25; // L298N IN1
const int MOTOR_A_IN2 = 26; // L298N IN2
const int MOTOR_A_PWM = 27; // L298N ENA

const int MOTOR_B_IN1 = 32; // L298N IN3
const int MOTOR_B_IN2 = 33; // L298N IN4
const int MOTOR_B_PWM = 14; // L298N ENB

const int SERVO_PIN = 18;

const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8; // 8-bit: 0 to 255
const int PWM_CHANNEL_A = 0;
const int PWM_CHANNEL_B = 1;

int currentSpeed = 150; // 0 to 255
const int MIN_SPEED = 0;
const int MAX_SPEED = 255;

int centerAngle = 90;
int leftAngle = 60;
int rightAngle = 120;

const unsigned long AUTO_STOP_MS = 1000;
unsigned long lastCommandTime = 0;
bool motorsRunning = false;

String inputBuffer = "";

// Forward declarations
void stopMotors();
void centerSteering();
void handleIncomingChar(char incomingChar);

// ========================== BLE CALLBACKS ==========================
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
    Serial.println("BLE client connected.");
  }

  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
    Serial.println("BLE client disconnected. Stopping motors and re-advertising.");
    stopMotors(); // safety: never keep driving after the controller is gone
    server->startAdvertising();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    String value = characteristic->getValue().c_str();
    for (size_t i = 0; i < value.length(); i++) {
      handleIncomingChar(value.charAt(i));
    }
  }
};

// ========================== SETUP ==========================
void setup() {
  Serial.begin(115200);

  setupMotorPins();
  setupPWM();
  setupServo();

  stopMotors();
  centerSteering();

  setupBLE();

  lastCommandTime = millis();

  Serial.println("RC car (BLE) ready.");
  Serial.println("Advertising as: " + String(BLE_NAME));
  Serial.println("Open app/index.html in a Web Bluetooth browser to connect.");
}

// ========================== MAIN LOOP ==========================
void loop() {
  handleAutoStop();
  delay(10);
}

// ========================== INITIALIZATION ==========================
void setupMotorPins() {
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);
}

void setupPWM() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(MOTOR_A_PWM, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_B_PWM, PWM_FREQ, PWM_RESOLUTION);
#else
  ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_A_PWM, PWM_CHANNEL_A);
  ledcAttachPin(MOTOR_B_PWM, PWM_CHANNEL_B);
#endif
}

void writeMotorPWM(int pin, int channel, int duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)channel;
  ledcWrite(pin, duty);
#else
  (void)pin;
  ledcWrite(channel, duty);
#endif
}

void setupServo() {
  steeringServo.setPeriodHertz(50);
  steeringServo.attach(SERVO_PIN, 500, 2400);
}

void setupBLE() {
  BLEDevice::init(BLE_NAME);

  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(NUS_SERVICE_UUID);

  // ESP32 -> phone (notify): optional status/ack channel.
  txCharacteristic = service->createCharacteristic(
      NUS_CHAR_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  txCharacteristic->addDescriptor(new BLE2902());

  // phone -> ESP32 (write): command channel.
  BLECharacteristic* rxCharacteristic = service->createCharacteristic(
      NUS_CHAR_RX_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rxCharacteristic->setCallbacks(new RxCallbacks());

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(NUS_SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

// ========================== COMMAND HANDLING ==========================
// Mirrors the classic-firmware parser: single-char commands act immediately,
// V<num>/A<num> accumulate until a newline.
void handleIncomingChar(char incomingChar) {
  if (incomingChar == '\n' || incomingChar == '\r') {
    processBufferedCommand();
    inputBuffer = "";
    return;
  }

  if (isSingleCharCommand(incomingChar)) {
    processSingleCharCommand(incomingChar);
    inputBuffer = "";
  } else {
    inputBuffer += incomingChar;
    if (inputBuffer.length() > 10) {
      inputBuffer = "";
    }
  }
}

bool isSingleCharCommand(char command) {
  command = toupper(command);
  return command == 'F' || command == 'B' || command == 'S' ||
         command == 'L' || command == 'R' || command == 'C';
}

void processSingleCharCommand(char command) {
  command = toupper(command);
  lastCommandTime = millis();

  switch (command) {
    case 'F': moveForward(currentSpeed); break;
    case 'B': moveBackward(currentSpeed); break;
    case 'S': stopMotors(); break;
    case 'L': steerLeft(); break;
    case 'R': steerRight(); break;
    case 'C': centerSteering(); break;
    default: Serial.println("Unknown single-character command."); break;
  }
}

void processBufferedCommand() {
  inputBuffer.trim();
  if (inputBuffer.length() == 0) return;

  char commandType = toupper(inputBuffer.charAt(0));
  String valueText = inputBuffer.substring(1);
  int value = valueText.toInt();

  lastCommandTime = millis();

  switch (commandType) {
    case 'V': setSpeed(value); break;
    case 'A': setSteeringAngle(value); break;
    default: Serial.println("Unknown buffered command: " + inputBuffer); break;
  }
}

void notify(const String& message) {
  if (deviceConnected && txCharacteristic != nullptr) {
    txCharacteristic->setValue(message.c_str());
    txCharacteristic->notify();
  }
}

// ========================== MOTOR CONTROL ==========================
void moveForward(int speedValue) {
  speedValue = constrain(speedValue, MIN_SPEED, MAX_SPEED);

  digitalWrite(MOTOR_A_IN1, HIGH);
  digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, HIGH);
  digitalWrite(MOTOR_B_IN2, LOW);

  writeMotorPWM(MOTOR_A_PWM, PWM_CHANNEL_A, speedValue);
  writeMotorPWM(MOTOR_B_PWM, PWM_CHANNEL_B, speedValue);

  motorsRunning = speedValue > 0;
  Serial.println("Moving forward at speed " + String(speedValue));
  notify("F" + String(speedValue));
}

void moveBackward(int speedValue) {
  speedValue = constrain(speedValue, MIN_SPEED, MAX_SPEED);

  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, HIGH);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, HIGH);

  writeMotorPWM(MOTOR_A_PWM, PWM_CHANNEL_A, speedValue);
  writeMotorPWM(MOTOR_B_PWM, PWM_CHANNEL_B, speedValue);

  motorsRunning = speedValue > 0;
  Serial.println("Moving backward at speed " + String(speedValue));
  notify("B" + String(speedValue));
}

void stopMotors() {
  writeMotorPWM(MOTOR_A_PWM, PWM_CHANNEL_A, 0);
  writeMotorPWM(MOTOR_B_PWM, PWM_CHANNEL_B, 0);

  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, LOW);

  motorsRunning = false;
  Serial.println("Motors stopped.");
  notify("S");
}

void setSpeed(int newSpeed) {
  currentSpeed = constrain(newSpeed, MIN_SPEED, MAX_SPEED);
  Serial.println("Speed set to " + String(currentSpeed));
  notify("V" + String(currentSpeed));
}

// ========================== STEERING CONTROL ==========================
void steerLeft() {
  setSteeringAngle(leftAngle);
  Serial.println("Steering left.");
}

void steerRight() {
  setSteeringAngle(rightAngle);
  Serial.println("Steering right.");
}

void centerSteering() {
  setSteeringAngle(centerAngle);
  Serial.println("Steering centered.");
}

void setSteeringAngle(int angle) {
  angle = constrain(angle, 0, 180);
  steeringServo.write(angle);
  Serial.println("Servo angle set to " + String(angle));
  notify("A" + String(angle));
}

// ========================== SAFETY ==========================
void handleAutoStop() {
  if (motorsRunning && millis() - lastCommandTime > AUTO_STOP_MS) {
    stopMotors();
    Serial.println("Auto-stop triggered: no command received.");
  }
}
