# Hardware Setup — What You Need to Do

This is the plain-English checklist for wiring the car and getting the code
running. The code is already written for an **original ESP32 (WROOM-32)** board
and an **L298N motor driver**. You just need to wire it up to match.

## What you should have

- Original ESP32 dev board (the 30-pin kind with a CP2102 USB chip)
- L298N motor driver module (the one with the big black heatsink)
- 2 DC motors (the rear drive wheels)
- 1 servo (SG90 or similar, for steering)
- A battery pack for the motors (see the power note below)
- A phone with any "Bluetooth terminal" app

## Step 1 — Wire the motor driver (L298N) to the ESP32

Connect these signal wires from the ESP32 pins to the L298N:

| ESP32 pin | goes to L298N | what it does |
|---|---|---|
| GPIO 18 | IN1 | Motor A direction |
| GPIO 21 | IN2 | Motor A direction |
| GPIO 4  | ENA | Motor A speed (PWM) |
| GPIO 33 | IN3 | Motor B direction |
| GPIO 26 | IN4 | Motor B direction |
| GPIO 5  | ENB | Motor B speed (PWM) |

**Important:** the L298N usually ships with little plastic jumpers on the ENA
and ENB pins. **Pull those two jumpers off.** If you leave them on, the motors
can only run full speed or off — the speed control won't work.

Then plug the motors into the output terminals: one motor into OUT1/OUT2, the
other into OUT3/OUT4.

> Do NOT use GPIO 34, 35, 36, or 39 for the motor wires. On this ESP32 those
> pins can only read inputs, they can't send signals out, so the motors won't
> respond.

## Step 2 — Wire the servo (steering)

The servo has three wires:

| Servo wire | Connect to |
|---|---|
| Orange / yellow (signal) | ESP32 GPIO 13 |
| Red (power) | 5V supply (NOT the ESP32 3.3V pin) |
| Brown / black (ground) | Common ground |

## Step 3 — Power and grounds

- Battery pack → L298N motor power input (the +12V / VS terminal).
- The ESP32 can be powered over its USB port while testing, or from a 5V
  source on the VIN pin once it's in the car.
- Power the servo from a 5V source, **not** the ESP32's 3.3V pin — it draws too
  much and will reset the board.
- **Tie all the grounds together:** battery ground, L298N ground, ESP32 ground,
  and servo ground must all connect. Nothing works reliably without this.

Battery note: two AA cells only make 3V, which is too low. Use 4x AA (6V) or a
higher-voltage pack. The L298N drops about 2V, so pick a pack a couple volts
above what the motors want.

If the ESP32 randomly restarts when the motors kick in, add a large capacitor
(470µF–1000µF) across the motor power input on the L298N.

## Step 4 — Upload the code

1. Install the Arduino IDE.
2. In Boards Manager, install **esp32 by Espressif**.
3. In Library Manager, install **ESP32Servo**.
4. Open `src/main.ino`.
5. Pick the board "ESP32 Dev Module" and the right COM port, then click Upload.

## Step 5 — Drive it

1. Open the Serial Monitor at 115200 baud — you should see "RC car ready."
2. On your phone, open a Bluetooth terminal app and pair with **ESP32_RC_Car**.
3. Send these letters to control it:
   - `F` forward, `B` backward, `S` stop
   - `L` left, `R` right, `C` center steering
   - `V160` set speed (0–255), `A95` set steering angle (0–180)

The car auto-stops after 1 second with no command, so the controller needs to
keep sending while driving — that's a safety feature, not a bug.

## If something's wrong

- **A motor spins the wrong way:** swap that motor's two output wires on the
  L298N, or swap its IN pins in the code.
- **Steering is reversed:** swap `leftAngle` and `rightAngle` in the code.
- **Motors don't change speed:** you left the ENA/ENB jumpers on (Step 1).
- **Board resets when motors start:** add the capacitor (Step 3) and make sure
  the motors are NOT powered from the ESP32.
