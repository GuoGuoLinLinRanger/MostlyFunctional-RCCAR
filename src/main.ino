/*
  ESP32 Bluetooth RC Car
  Architecture:
  - Rear-wheel drive using a TB6612FNG-style dual motor driver
  - Front-wheel steering using a servo
  - Phone control over Bluetooth Serial

  Commands:
    F       -> move forward at current speed
    B       -> move backward at current speed
    S       -> stop motors
    L       -> steer left using current left angle
    R       -> steer right using current right angle
    C       -> center steering
    V<number> -> set motor speed from 0 to 255, example: V160
    A<number> -> set servo angle directly from 0 to 180, example: A95

  Safety:
    If no command is received for AUTO_STOP_MS, motors stop automatically.
*/

#include "BluetoothSerial.h"
#include <ESP32Servo.h>

BluetoothSerial SerialBT;
Servo steeringServo;

// ========================== PROJECT CONFIG ==========================
const char* BLUETOOTH_NAME = "ESP32_RC_Car";

// Motor driver assumptions: TB6612FNG-style driver
// You can modify these pins later based on wiring.
const int MOTOR_A_IN1 = 25;
const int MOTOR_A_IN2 = 26;
const int MOTOR_A_PWM = 27;

const int MOTOR_B_IN1 = 32;
const int MOTOR_B_IN2 = 33;
const int MOTOR_B_PWM = 14;

// TB6612FNG has a standby pin. If your driver does not have STBY, ignore this.
const int MOTOR_STBY = 13;

// Servo pin
const int SERVO_PIN = 18;

// PWM settings for ESP32 motors
const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8; // 8-bit: 0 to 255
const int PWM_CHANNEL_A = 0;
const int PWM_CHANNEL_B = 1;

// Default motion settings
int currentSpeed = 150; // 0 to 255. Start conservative.
const int MIN_SPEED = 0;
const int MAX_SPEED = 255;

// Steering calibration. Modify these after testing your physical steering.
int centerAngle = 90;
int leftAngle = 60;
int rightAngle = 120;

// Safety timeout
const unsigned long AUTO_STOP_MS = 1000;
unsigned long lastCommandTime = 0;
bool motorsRunning = false;

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
  Serial.println("Commands: F, B, S, L, R, C, V<number>, A<number>");
}

// ========================== MAIN LOOP ==========================
void loop() {
  readBluetoothCommands();
  handleAutoStop();
}

// ========================== INITIALIZATION ==========================
void setupMotorPins() {
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);
  pinMode(MOTOR_STBY, OUTPUT);

  digitalWrite(MOTOR_STBY, HIGH); // enable TB6612FNG
}

void setupPWM() {
  ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(MOTOR_A_PWM, PWM_CHANNEL_A);
  ledcAttachPin(MOTOR_B_PWM, PWM_CHANNEL_B);
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
      return;
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
         command == 'L' || command == 'R' || command == 'C';
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
      steerLeft();
      break;
    case 'R':
      steerRight();
      break;
    case 'C':
      centerSteering();
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

  ledcWrite(PWM_CHANNEL_A, speedValue);
  ledcWrite(PWM_CHANNEL_B, speedValue);

  motorsRunning = speedValue > 0;
  Serial.println("Moving forward at speed " + String(speedValue));
}

void moveBackward(int speedValue) {
  speedValue = constrain(speedValue, MIN_SPEED, MAX_SPEED);

  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, HIGH);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, HIGH);

  ledcWrite(PWM_CHANNEL_A, speedValue);
  ledcWrite(PWM_CHANNEL_B, speedValue);

  motorsRunning = speedValue > 0;
  Serial.println("Moving backward at speed " + String(speedValue));
}

void stopMotors() {
  ledcWrite(PWM_CHANNEL_A, 0);
  ledcWrite(PWM_CHANNEL_B, 0);

  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, LOW);

  motorsRunning = false;
  Serial.println("Motors stopped.");
}

void setSpeed(int newSpeed) {
  currentSpeed = constrain(newSpeed, MIN_SPEED, MAX_SPEED);
  Serial.println("Speed set to " + String(currentSpeed));
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
}

// ========================== SAFETY ==========================
void handleAutoStop() {
  if (motorsRunning && millis() - lastCommandTime > AUTO_STOP_MS) {
    stopMotors();
    Serial.println("Auto-stop triggered: no command received.");
  }
}
