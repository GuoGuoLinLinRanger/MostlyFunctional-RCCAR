# ESP32 Bluetooth RC Car

A small rear-wheel-drive RC car project using an ESP32, a motor driver, rear DC motors, and a front steering servo.

## Project Goals

- Control the car from a phone using Bluetooth
- Drive rear motors forward and backward
- Steer front wheels using a servo
- Support adjustable motor speed
- Support adjustable servo angles
- Stop automatically if the Bluetooth signal stops sending commands

## Assumed Hardware

This code assumes the following setup:

- ESP32 development board
- TB6612FNG-style dual motor driver
- Two rear DC motors
- One steering servo, such as SG90 or MG90S
- 2x18650 battery pack
- 5V buck converter for ESP32 and servo power
- Common ground between all parts

If your team uses L298N instead of TB6612FNG, the code can be adapted later.

## Build Requirements

In the Arduino IDE:

1. Install **ESP32 board support** via Boards Manager (search "esp32" by Espressif). Both core 2.x and 3.x are supported by this sketch.
2. Install the **ESP32Servo** library via Library Manager.
3. `BluetoothSerial` ships with the ESP32 board package — no separate install needed.
4. Select an ESP32 board that supports classic Bluetooth (the original ESP32, not S2/S3/C3) and the correct serial port, then upload `src/main.ino`.

## Pin Assignment

| Function | ESP32 Pin |
|---|---:|
| Motor A IN1 | GPIO 25 |
| Motor A IN2 | GPIO 26 |
| Motor A PWM | GPIO 27 |
| Motor B IN1 | GPIO 32 |
| Motor B IN2 | GPIO 33 |
| Motor B PWM | GPIO 14 |
| Motor driver STBY | GPIO 13 |
| Servo signal | GPIO 18 |

## Bluetooth Commands

Use a Bluetooth terminal app on your phone and connect to:

```text
ESP32_RC_Car
```

Commands:

| Command | Meaning |
|---|---|
| `F` | Move forward at current speed |
| `B` | Move backward at current speed |
| `S` | Stop motors |
| `L` | Steer left |
| `R` | Steer right |
| `C` | Center steering |
| `V160` | Set motor speed to 160, range 0 to 255 |
| `A95` | Set servo angle directly to 95 degrees, range 0 to 180 |

For commands like `V160` and `A95`, send a newline after the command if your Bluetooth app supports it.

## Safety Behavior

The motors automatically stop after 1 second without receiving a command.

This prevents the car from continuing to drive if Bluetooth disconnects or the phone stops sending commands.

## Recommended Testing Order

1. Upload the code and check Serial Monitor output.
2. Pair phone with `ESP32_RC_Car`.
3. Send Bluetooth commands and confirm Serial Monitor prints correct actions.
4. Test servo only.
5. Test one motor only.
6. Test both motors.
7. Test motor and steering together.
8. Tune speed and steering angles.

## Important Circuit Notes

Do not power motors from the ESP32.

Use this power idea:

```text
Battery pack -> motor driver motor power
Battery pack -> 5V buck converter -> ESP32 VIN/5V + servo power
All grounds connected together
```

Add a capacitor near the motor driver if the ESP32 resets when motors start.

Recommended capacitor:

```text
470uF to 1000uF
```

## Folder Structure

```text
rc-car-esp32/
  README.md
  src/
    main.ino
  docs/
    wiring-notes.md
    task-breakdown.md
```

## Future Improvements

- Add custom phone app
- Add smoother acceleration
- Add battery voltage monitoring
- Add ultrasonic sensor
- Add 3D printed enclosure
