/*
  ESP32 Bluetooth RC Car
  Architecture:
  - Rear-wheel drive using an L298N dual motor driver
  - Front-wheel steering using a servo
  - Phone control over Bluetooth Serial

  Board: original ESP32 (WROOM-32, CP2102 USB). Classic Bluetooth is supported,
  so this sketch runs as-is. Do NOT use GPIO 34/35/36/39 for outputs on this
  chip -- they are input-only.

  Commands (each is a single character unless noted):
    F        -> drive forward at current speed   (latches: keeps going until S)
    B        -> drive backward at current speed   (latches)
    S        -> stop motors
    L        -> steer a step further left
    R        -> steer a step further right
    C        -> center steering
    +        -> speed up one step  (great for a "speed ++" terminal macro)
    -        -> slow down one step  (great for a "speed --" macro)
    V<number> -> set motor speed 0..255, needs a newline, example: V160
    A<number> -> set servo angle 0..180, needs a newline, example: A95

  Why "+/-" and not just V<number>?  A phone terminal sends a macro's text once
  per tap and usually does NOT append a newline, so "V160" alone never gets
  parsed. The single-char +/- steppers work no matter how line-endings are set,
  and they're floored at MIN_DRIVE_SPEED so "slow down" can't drop the car below
  the point where the motors stall.

  Drive model:
    Movement LATCHES -- one tap of F/B keeps the car moving until you tap S
    (a terminal can't stream a command while you hold a button, so this avoids
    the "tap forward repeatedly" problem). Safety: the motors stop automatically
    if the Bluetooth client disconnects. The old time-based auto-stop is off by
    default (AUTO_STOP_MS = 0); set it to e.g. 1500 to re-enable a watchdog.
*/

#include "BluetoothSerial.h"
#include <ESP32Servo.h>

BluetoothSerial SerialBT;
Servo steeringServo;

// ========================== PROJECT CONFIG ==========================
const char* BLUETOOTH_NAME = "ESP32_RC_Car";

// Motor driver: L298N. Each motor uses two direction pins plus one PWM pin.
// On the L298N the PWM pin is the EN (enable) input -- MOTOR_A_PWM -> ENA,
// MOTOR_B_PWM -> ENB. Remove the ENA/ENB jumpers on the module so these GPIOs
// actually control speed; left jumpered, the motors only run full-on/off.
const int MOTOR_A_IN1 = 18; // L298N IN1
const int MOTOR_A_IN2 = 21; // L298N IN2
const int MOTOR_A_PWM = 4;  // L298N ENA

const int MOTOR_B_IN1 = 33; // L298N IN3
const int MOTOR_B_IN2 = 26; // L298N IN4
const int MOTOR_B_PWM = 5;  // L298N ENB

// The L298N has no standby pin (unlike the TB6612FNG), so none is used.

// Servo pin
const int SERVO_PIN = 13;

// PWM settings for ESP32 motors
const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8; // 8-bit: 0 to 255
const int PWM_CHANNEL_A = 0;
const int PWM_CHANNEL_B = 1;

// Speed settings (0..255). Starts at full speed; tune down with +/- or V<number>.
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

// Safety. AUTO_STOP_MS = 0 disables the time-based watchdog; we instead stop the
// car the moment the Bluetooth client disconnects (see handleConnectionSafety).
const unsigned long AUTO_STOP_MS = 0;
unsigned long lastCommandTime = 0;
bool motorsRunning = false;
bool wasConnected = false;

// Input buffer for commands like V160 or A95
String inputBuffer = "";

// ========================== SETUP ==========================
void setup() {
  Serial.begin(115200);
  SerialBT.begin(BLUETOOTH_NAME);

  setupMotorPins();
  setupPWM();
  setupServo();

  stopMotors();
  centerSteering();

  lastCommandTime = millis();

  Serial.println("RC car ready.");
  Serial.println("Pair with Bluetooth device: " + String(BLUETOOTH_NAME));
  Serial.println("Commands: F, B, S, L, R, C, +, -, V<number>, A<number>");
}

// ========================== MAIN LOOP ==========================
void loop() {
  readBluetoothCommands();
  handleConnectionSafety();
  if (AUTO_STOP_MS > 0) handleAutoStop();
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
  // ESP32 Arduino core 3.x: attach the pin directly with freq + resolution.
  ledcAttach(MOTOR_A_PWM, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_B_PWM, PWM_FREQ, PWM_RESOLUTION);
#else
  // ESP32 Arduino core 2.x: configure a channel, then bind the pin to it.
  ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(MOTOR_A_PWM, PWM_CHANNEL_A);
  ledcAttachPin(MOTOR_B_PWM, PWM_CHANNEL_B);
#endif
}

// Write a PWM duty cycle. Core 3.x addresses the pin; core 2.x the channel.
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
  steeringServo.setPeriodHertz(50); // standard servo frequency
  steeringServo.attach(SERVO_PIN, 500, 2400);
}

// ========================== BLUETOOTH INPUT ==========================
void readBluetoothCommands() {
  while (SerialBT.available()) {
    char incomingChar = SerialBT.read();

    if (incomingChar == '\n' || incomingChar == '\r') {
      processBufferedCommand();
      inputBuffer = "";
      continue;
    }

    if (isSingleCharCommand(incomingChar)) {
      processSingleCharCommand(incomingChar);
      inputBuffer = "";
    } else {
      inputBuffer += incomingChar;

      // Prevent accidental memory growth from malformed input.
      if (inputBuffer.length() > 10) {
        inputBuffer = "";
      }
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
    case 'F':
      moveForward(currentSpeed);
      break;
    case 'B':
      moveBackward(currentSpeed);
      break;
    case 'S':
      stopMotors();
      break;
    case 'L':
      steerBy(-STEER_STEP); // lower angle = more left
      break;
    case 'R':
      steerBy(+STEER_STEP); // higher angle = more right
      break;
    case 'C':
      centerSteering();
      break;
    case '+':
      changeSpeed(+SPEED_STEP);
      break;
    case '-':
      changeSpeed(-SPEED_STEP);
      break;
    default:
      Serial.println("Unknown single-character command.");
      break;
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
    case 'V':
      setSpeed(value);
      break;
    case 'A':
      setSteeringAngle(value);
      break;
    default:
      Serial.println("Unknown buffered command: " + inputBuffer);
      break;
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
}

// Absolute speed (from V<number>). Allows the full 0..255 range.
void setSpeed(int newSpeed) {
  currentSpeed = constrain(newSpeed, MIN_SPEED, MAX_SPEED);
  reapplyDriveSpeed();
  Serial.println("Speed set to " + String(currentSpeed));
}

// Relative speed step (from +/-). Floored at MIN_DRIVE_SPEED so a "slow down"
// tap can never leave the car too weak to move; use S or V0 to truly stop.
void changeSpeed(int delta) {
  currentSpeed = constrain(currentSpeed + delta, MIN_DRIVE_SPEED, MAX_SPEED);
  reapplyDriveSpeed();
  Serial.println("Speed -> " + String(currentSpeed));
}

// If we're already driving, push the new speed to the motors immediately.
void reapplyDriveSpeed() {
  if (driveDir == DIR_FORWARD)       moveForward(currentSpeed);
  else if (driveDir == DIR_BACKWARD) moveBackward(currentSpeed);
}

// ========================== STEERING CONTROL ==========================
// Step the steering relative to where it is now (finer than slamming L/R to the
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
}

// ========================== SAFETY ==========================
// Stop the moment the phone disconnects so the car never drives away on its own.
void handleConnectionSafety() {
  bool connected = SerialBT.hasClient();
  if (wasConnected && !connected) {
    Serial.println("Bluetooth client gone -- stopping for safety.");
    stopMotors();
    centerSteering();
  }
  wasConnected = connected;
}

// Optional time-based watchdog (off unless AUTO_STOP_MS > 0).
void handleAutoStop() {
  if (motorsRunning && millis() - lastCommandTime > AUTO_STOP_MS) {
    stopMotors();
    Serial.println("Auto-stop triggered: no command received.");
  }
}
