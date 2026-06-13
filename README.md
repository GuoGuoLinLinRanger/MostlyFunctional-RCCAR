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

- Original ESP32 development board (WROOM-32, CP2102 USB) — has Classic Bluetooth
- L298N dual motor driver
- Two rear DC motors
- One steering servo, such as SG90 or MG90S
- Battery pack for motor power
- 3.3V/5V supply for the ESP32 and servo
- Common ground between all parts

Note: GPIO 34/35/36/39 on the original ESP32 are input-only and cannot drive
the L298N inputs — keep motor pins off those.

## Build Requirements

> New to the hardware? Start with [docs/hardware-setup.md](docs/hardware-setup.md)
> for a plain-English wiring checklist, then
> [docs/flashing-and-control.md](docs/flashing-and-control.md) to upload the code
> and drive the car.

In the Arduino IDE:

1. Install **ESP32 board support** via Boards Manager (search "esp32" by Espressif). Both core 2.x and 3.x are supported by this sketch.
2. Install the **ESP32Servo** library via Library Manager.
3. `BluetoothSerial` ships with the ESP32 board package — no separate install needed.
4. Select an ESP32 board that supports classic Bluetooth (the original ESP32, not S2/S3/C3) and the correct serial port, then open and upload `firmware/rc_car_classic/rc_car_classic.ino`.

## Pin Assignment

| Function | ESP32 Pin |
|---|---:|
| Motor A IN1 | GPIO 18 |
| Motor A IN2 | GPIO 21 |
| Motor A PWM | GPIO 4 |
| Motor B IN1 | GPIO 33 |
| Motor B IN2 | GPIO 26 |
| Motor B PWM | GPIO 5 |
| Servo signal | GPIO 13 |

These match the project schematic (with IN1/IN2 moved off the input-only
GPIO 34/35 to GPIO 18/21, and ENA/ENB added on GPIO 4/5).

| L298N pin | ESP32 pin |
|---|---:|
| IN1 | GPIO 18 |
| IN2 | GPIO 21 |
| ENA | GPIO 4 |
| IN3 | GPIO 33 |
| IN4 | GPIO 26 |
| ENB | GPIO 5 |

Remove the ENA/ENB jumpers on the L298N module so the ESP32 can control speed
via PWM. The L298N has no standby pin.

### Power and ground

These pins use the labels printed on the WROOM-32 dev module:

| Module pin | Connect to |
|---|---|
| `5V` / `VIN` (a.k.a. `EXT_5V`) | Regulated **5V** from the buck converter — powers the ESP32 |
| `GND` (any of `GND1`/`GND2`/`GND3`) | **Common ground** — tie ESP32, L298N, battery, and 5V supply together |
| `3V3` | Leave unconnected — it's a **3.3V output**, not a power input |

The servo's signal goes to GPIO 13, but its **+ and – go to the 5V rail**, never 3V3.

### Pins to keep free

- `IO34`, `IO35`, `SENSOR_VP` (GPIO 36), `SENSOR_VN` (GPIO 39) — **input-only**, cannot drive the L298N.
- `TXD0` / `RXD0` (GPIO 1 / 3) — the USB serial; wiring to them breaks upload and Serial Monitor.
- `IO0`, `IO2`, `IO12`, `IO15` — boot/strapping pins; leave them free.

## Wiring, Step by Step

Do this with **everything powered off** — no USB, no battery connected — until
the last step. Pin labels are those printed on the WROOM-32 dev module.

1. **Ground first (common ground).** Connect a ground wire between the ESP32
   `GND`, the L298N `GND`, the battery `–`, and the 5V buck converter `GND` so
   they all share one ground. Nothing works without this.
2. **ESP32 → L298N signal wires** (these can run on the breadboard):
   - ESP32 `IO18` → L298N `IN1`
   - ESP32 `IO21` → L298N `IN2`
   - ESP32 `IO4`  → L298N `ENA`
   - ESP32 `IO33` → L298N `IN3`
   - ESP32 `IO26` → L298N `IN4`
   - ESP32 `IO5`  → L298N `ENB`
3. **Remove the L298N `ENA` and `ENB` jumpers** so the ESP32's PWM controls
   speed. (Leaving them on locks the motors at full speed.)
4. **Motors → L298N outputs** (screw terminals, not the breadboard):
   - Motor A → `OUT1` and `OUT2`
   - Motor B → `OUT3` and `OUT4`
   - (If a motor runs backwards later, just swap its two wires here.)
5. **Battery → L298N motor power** (screw terminals):
   - Battery `+` → L298N `+12V` (`VS`)
   - Battery `–` → L298N `GND` (already on the common ground from step 1)
   - If the battery is **over 12V**, also remove the L298N 5V jumper and feed it
     5V separately; up to 12V the onboard regulator is fine.
6. **5V buck converter → ESP32 power:**
   - Buck `5V out` → ESP32 `5V` / `VIN`
   - Buck `GND` → common ground (step 1)
   - Leave the ESP32 `3V3` pin unconnected — it is an output, not an input.
7. **Servo (3 wires):**
   - Servo **signal** → ESP32 `IO13`
   - Servo `+` → **5V rail** (the buck output), *not* `3V3`
   - Servo `–` → common ground
8. **Double-check before power:** grounds all tied together, no wire on
   `IO34/IO35/VP/VN` or `TXD0/RXD0`, motors on the screw terminals, `ENA/ENB`
   jumpers removed.
9. **Now power up:** connect the 5V supply and the battery, then plug the ESP32
   into USB to flash it (see [docs/flashing-and-control.md](docs/flashing-and-control.md)).

> First test with the **wheels off the ground** and in order: servo, then one
> motor, then both, then drive + steering. See the Recommended Testing Order
> below.

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

## Optional: Phone Remote Control (Web Bluetooth)

> This is **optional**. The default `firmware/rc_car_classic` (Bluetooth Classic)
> already works with any generic phone "Bluetooth terminal" app. Use this section
> only if you want a touch-friendly on-screen remote with no app install.

Phone *browsers* can only talk to **BLE**, not Bluetooth Classic, so the web remote
needs the alternate BLE firmware. The two firmwares speak the exact same command
protocol — **flash only one**:

| Firmware | Radio | Drive it with |
|---|---|---|
| `firmware/rc_car_classic` | Bluetooth Classic (SPP) | Generic BT terminal apps |
| `firmware/rc_car_ble` | Bluetooth Low Energy (BLE) | The included web remote, `app/index.html` |

To use the web remote:

1. Flash `firmware/rc_car_ble/rc_car_ble.ino` instead of the classic sketch.
2. Open `app/index.html` in a **Web Bluetooth** browser — Android Chrome, or desktop
   Chrome/Edge. (iOS Safari does **not** support Web Bluetooth.)
3. Tap **Connect** and pick `ESP32_RC_Car`.
4. **Hold** a direction button to drive (it auto-resends to beat the 1-second safety
   timeout) and release to stop; use the slider for speed and **STOP** for an
   immediate halt.

You can open the file directly (`file://`) or serve it locally, e.g. `python -m http.server`.

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
  LICENSE
  firmware/
    rc_car_classic/
      rc_car_classic.ino   # default firmware: Bluetooth Classic (SPP)
    rc_car_ble/
      rc_car_ble.ino       # optional firmware: BLE, for the web remote
  app/
    index.html     # optional Web Bluetooth phone remote
  hardware/
    rc-car.kicad_sch   # KiCad schematic
  docs/
    hardware-setup.md       # plain-English wiring + setup checklist
    flashing-and-control.md # upload the code and drive the car
    wiring-notes.md
    task-breakdown.md
```

## Future Improvements

- Add smoother acceleration
- Add battery voltage monitoring
- Add ultrasonic sensor
- Add 3D printed enclosure

## License

Released under the MIT License. See [LICENSE](LICENSE) for details.
