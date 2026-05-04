# Wiring Notes

## System Architecture

```text
Phone
  ↓ Bluetooth
ESP32
  ├── GPIO pins -> motor driver -> rear motors
  └── PWM signal -> servo -> front steering
```

## Power Architecture

Recommended setup:

```text
2x18650 battery pack
        ↓
     switch
        ↓
  +-------------+
  |             |
motor driver   5V buck converter
  |             |
rear motors    ESP32 + servo
```

All grounds must be shared:

```text
Battery GND = motor driver GND = buck converter GND = ESP32 GND = servo GND
```

## Motor Driver Pins

Assumed TB6612FNG-style wiring:

| Motor Driver Pin | Connects To |
|---|---|
| VM | Battery motor voltage |
| VCC | ESP32 3.3V logic |
| GND | Common ground |
| STBY | ESP32 GPIO 13 |
| AIN1 | ESP32 GPIO 25 |
| AIN2 | ESP32 GPIO 26 |
| PWMA | ESP32 GPIO 27 |
| BIN1 | ESP32 GPIO 32 |
| BIN2 | ESP32 GPIO 33 |
| PWMB | ESP32 GPIO 14 |
| AO1/AO2 | Left or Motor A rear motor |
| BO1/BO2 | Right or Motor B rear motor |

## Servo Wiring

Typical servo wires:

| Servo Wire | Connects To |
|---|---|
| Red | 5V from buck converter |
| Brown/Black | Common ground |
| Orange/Yellow | ESP32 GPIO 18 |

Do not power the servo from the ESP32 3.3V pin.

## Common Issues

- No common ground
- Motors powered from ESP32
- Servo powered from ESP32 3.3V
- Battery voltage connected to wrong ESP32 pin
- Motor directions reversed
- Servo angle reversed
- ESP32 resets when motors start
