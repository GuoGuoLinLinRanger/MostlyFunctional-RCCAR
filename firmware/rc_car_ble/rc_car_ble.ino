/*
  OPTIONAL — ESP32 BLE RC Car firmware

  This is an ALTERNATIVE to firmware/rc_car_classic. Flash only ONE firmware.

    rc_car_classic -> Bluetooth Classic (SPP). Works with generic phone
                      "Bluetooth terminal" apps.
    rc_car_ble     -> Bluetooth Low Energy (BLE), Nordic UART Service.
                         Works with the included web controller (app/index.html)
                         in any Web Bluetooth browser: Android Chrome, desktop
                         Chrome/Edge, AND with generic BLE controller/terminal
                         apps that talk Nordic UART. NOTE: iOS Safari does NOT
                         support Web Bluetooth, so iPhones need a BLE app.

  Commands (each is a single character unless noted):
    F        -> drive forward at current speed   (latches: keeps going until S)
    B        -> drive backward at current speed   (latches)
    S        -> stop motors
    L        -> steer a step further left
    R        -> steer a step further right
    C        -> center steering
    +        -> speed up one step  (map your app's "speed ++" button to this)
    -        -> slow down one step  (map your app's "speed --" button to this)
    V<number> -> set motor speed 0..255, needs a newline, example: V160
    A<number> -> set servo angle 0..180, needs a newline, example: A95

  Why "+/-" and not just V<number>?  A generic controller app sends a button's
  text once per tap and often does NOT append a newline, so "V160" alone never
  gets parsed. The single-char +/- steppers work regardless of line-endings and
  are floored at MIN_DRIVE_SPEED so "slow down" can't stall the car.

  Drive model:
    Movement LATCHES -- one tap of F/B keeps the car moving until you tap S, so
    you don't have to spam the button (most controller apps send a command once
    per tap, not continuously). Safety: motors stop immediately if the BLE client
    disconnects. The included web remote streams F/B while held and sends S on
    release, so it works the same way. The old time-based auto-stop is off by
    default (AUTO_STOP_MS = 0); set it to e.g. 1500 to re-enable a watchdog.
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
// Keep these in sync with rc_car_classic so wiring is identical either way.
// Motor driver: L298N. MOTOR_A_PWM -> ENA, MOTOR_B_PWM -> ENB (remove the
// ENA/ENB module jumpers so these GPIOs control speed). No standby pin.
const int MOTOR_A_IN1 = 18; // L298N IN1
const int MOTOR_A_IN2 = 21; // L298N IN2
const int MOTOR_A_PWM = 4;  // L298N ENA

const int MOTOR_B_IN1 = 33; // L298N IN3
const int MOTOR_B_IN2 = 26; // L298N IN4
const int MOTOR_B_PWM = 5;  // L298N ENB

const int SERVO_PIN = 13;

const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8; // 8-bit: 0 to 255
const int PWM_CHANNEL_A = 2;
const int PWM_CHANNEL_B = 3;

// Speed settings (0..255). Tune with +/- or V<number>.
int currentSpeed = 255;
const int MIN_SPEED = 0;
const int MAX_SPEED = 255;
const int SPEED_STEP = 25;       // how much +/- changes speed per tap
const int MIN_DRIVE_SPEED = 110; // +/- won't drop below this (motors stall lower)

// Steering calibration. Modify these after testing your physical steering.
int centerAngle = 90;
int leftAngle = 30;   // full-left servo angle (smaller = more left)
int rightAngle = 150; // full-right servo angle (larger = more right)
int currentAngle = 90;
const int STEER_STEP = 15; // degrees per L/R tap

// Drive direction so +/- can re-apply the new speed while moving.
const int DIR_STOP = 0, DIR_FORWARD = 1, DIR_BACKWARD = -1;
int driveDir = DIR_STOP;

// Safety. AUTO_STOP_MS = 0 disables the time-based watchdog; the BLE disconnect
// callback already stops the car when the controller goes away.
const unsigned long AUTO_STOP_MS = 0;
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
    centerSteering();
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
  setupServo();
  setupPWM();


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
  if (AUTO_STOP_MS > 0) handleAutoStop();
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
         command == 'L' || command == 'R' || command == 'C' ||
         command == '+' || command == '-';
}

void processSingleCharCommand(char command) {
  command = toupper(command);
  lastCommandTime = millis();

  switch (command) {
    case 'F': moveForward(currentSpeed); break;
    case 'B': moveBackward(currentSpeed); break;
    case 'S': stopMotors(); break;
    case 'L': steerBy(-STEER_STEP); break; // lower angle = more left
    case 'R': steerBy(+STEER_STEP); break; // higher angle = more right
    case 'C': centerSteering(); break;
    case '+': changeSpeed(+SPEED_STEP); break;
    case '-': changeSpeed(-SPEED_STEP); break;
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

  driveDir = DIR_FORWARD;
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

  driveDir = DIR_BACKWARD;
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

  driveDir = DIR_STOP;
  motorsRunning = false;
  Serial.println("Motors stopped.");
  notify("S");
}

// Absolute speed (from V<number>). Allows the full 0..255 range.
void setSpeed(int newSpeed) {
  currentSpeed = constrain(newSpeed, MIN_SPEED, MAX_SPEED);
  reapplyDriveSpeed();
  Serial.println("Speed set to " + String(currentSpeed));
  notify("V" + String(currentSpeed));
}

// Relative speed step (from +/-). Floored at MIN_DRIVE_SPEED so a "slow down"
// tap can never leave the car too weak to move; use S or V0 to truly stop.
void changeSpeed(int delta) {
  currentSpeed = constrain(currentSpeed + delta, MIN_DRIVE_SPEED, MAX_SPEED);
  reapplyDriveSpeed();
  Serial.println("Speed -> " + String(currentSpeed));
  notify("V" + String(currentSpeed));
}

// If we're already driving, push the new speed to the motors immediately.
void reapplyDriveSpeed() {
  if (driveDir == DIR_FORWARD)       moveForward(currentSpeed);
  else if (driveDir == DIR_BACKWARD) moveBackward(currentSpeed);
}

// ========================== STEERING CONTROL ==========================
// Step the steering relative to where it is now (finer than slamming to the
// limits). Clamped to the calibrated left/right range.
void steerBy(int deltaDegrees) {
  int lo = min(leftAngle, rightAngle);
  int hi = max(leftAngle, rightAngle);
  int target = constrain(currentAngle + deltaDegrees, lo, hi);
  setSteeringAngle(target);
}

void centerSteering() {
  setSteeringAngle(centerAngle);
  Serial.println("Steering centered.");
}

void setSteeringAngle(int angle) {
  currentAngle = constrain(angle, 0, 180);
  steeringServo.write(currentAngle);
  Serial.println("Servo angle set to " + String(currentAngle));
  notify("A" + String(currentAngle));
}

// ========================== SAFETY ==========================
// Optional time-based watchdog (off unless AUTO_STOP_MS > 0). The BLE disconnect
// callback already stops the car when the controller goes away.
void handleAutoStop() {
  if (motorsRunning && millis() - lastCommandTime > AUTO_STOP_MS) {
    stopMotors();
    Serial.println("Auto-stop triggered: no command received.");
  }
}
