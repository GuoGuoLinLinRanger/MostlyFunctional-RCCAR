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

L298N wiring:

| Motor Driver Pin | Connects To |
|---|---|
| +12V (VS) | Battery motor voltage |
| +5V | Logic supply (see module jumper) |
| GND | Common ground |
| ENA | ESP32 GPIO 4 (remove ENA jumper) |
| IN1 | ESP32 GPIO 18 |
| IN2 | ESP32 GPIO 21 |
| IN3 | ESP32 GPIO 33 |
| IN4 | ESP32 GPIO 26 |
| ENB | ESP32 GPIO 5 (remove ENB jumper) |
| OUT1/OUT2 | Motor A rear motor |
| OUT3/OUT4 | Motor B rear motor |

The L298N has no standby pin. Remove the ENA/ENB jumpers so the ESP32 PWM
signals control motor speed; if left jumpered to +5V the motors only run full-on.

## Servo Wiring

Typical servo wires:

| Servo Wire | Connects To |
|---|---|
| Red | 5V from buck converter |
| Brown/Black | Common ground |
| Orange/Yellow | ESP32 GPIO 13 |

Do not power the servo from the ESP32 3.3V pin.

## Common Issues

- No common ground
- Motors powered from ESP32
- Servo powered from ESP32 3.3V
- Battery voltage connected to wrong ESP32 pin
- Motor directions reversed
- Servo angle reversed
- ESP32 resets when motors start
