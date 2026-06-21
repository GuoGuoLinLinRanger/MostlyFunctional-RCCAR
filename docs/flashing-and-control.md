# Connecting the Code to the Car

You've built the circuit. This is how you get the code onto the ESP32 and drive
the car. Plan on about 15 minutes the first time.

## Wiring quick reference

The code expects these exact pins — wire the ESP32 to match (labels are those
printed on the WROOM-32 dev module):

| ESP32 pin | Connect to | Purpose |
|---|---|---|
| IO18 | L298N IN1 | Motor A direction |
| IO21 | L298N IN2 | Motor A direction |
| IO4 | L298N ENA | Motor A speed (PWM) |
| IO33 | L298N IN3 | Motor B direction |
| IO26 | L298N IN4 | Motor B direction |
| IO5 | L298N ENB | Motor B speed (PWM) |
| IO13 | Servo signal | Steering |
| 5V / VIN | 5V buck output | Powers the ESP32 |
| GND | Common ground | Tie ESP32 + L298N + battery + 5V supply together |

Motor power (battery → L298N `+12V`/`GND`, motors → `OUT1/2` and `OUT3/4`) and
the servo's `+`/`–` (to the 5V rail) are **not** run through the ESP32. Remove
the L298N `ENA`/`ENB` jumpers so the PWM pins control speed. Don't wire anything
to the input-only pins (IO34/IO35/VP/VN) or the USB serial pins (TXD0/RXD0).

## Step 1 — Install the Arduino IDE

Download and install it from <https://www.arduino.cc/en/software>. Open it once
so it finishes setting up.

## Step 2 — Add ESP32 board support

1. **File → Preferences.**
2. In **Additional boards manager URLs**, paste:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
   Click OK.
3. **Tools → Board → Boards Manager**, search **esp32**, install **esp32 by
   Espressif Systems**. (This download is large; let it finish.)

## Step 3 — Install the servo library

**Tools → Manage Libraries**, search **ESP32Servo**, install it.

## Step 4 — Pick which firmware to use

There are two versions. **Flash only one.**

| Folder | Control method | Use this if... |
|---|---|---|
| `firmware/rc_car_classic/` | Bluetooth Classic | You'll drive it from a phone "Bluetooth terminal" app (simplest) |
| `firmware/rc_car_ble/` | Bluetooth Low Energy | You want the on-screen web remote (`app/index.html`) |

For most people, start with **rc_car_classic**.

Open it: **File → Open**, navigate to `firmware/rc_car_classic/` and open
`rc_car_classic.ino`. (Each sketch lives in its own folder — open the `.ino`,
not the folder.)

## Step 5 — Plug in and pick the board + port

1. Connect the ESP32 to your computer with a **data** USB cable (some cables are
   charge-only — if the board never shows up, try another cable).
2. **Tools → Board → esp32 →** select **ESP32 Dev Module**.
3. **Tools → Port →** select the COM port that appears when you plug the board
   in (e.g. `COM5`). If no new port appears, install the **CP2102 driver** (this
   board uses a CP2102 USB chip) from Silicon Labs.
4. Leave the other Tools settings at their defaults.

## Step 6 — Upload

1. Click the **Upload** button (the right-arrow ➜).
2. It compiles, then flashes. If it stalls at `Connecting....`, **hold the BOOT
   button** on the ESP32 for a couple seconds while it connects, then release.
3. Wait for **Done uploading.**

## Step 7 — Confirm it's alive

1. **Tools → Serial Monitor**, set the baud rate (bottom right) to **115200**.
2. Press the ESP32's **EN/RST** button. You should see:
   ```
   RC car ready.
   Pair with Bluetooth device: ESP32_RC_Car
   ```
   If you see that, the code is running.

## Step 8 — Connect your phone and drive

1. On your phone, install any **Bluetooth terminal / serial** app (e.g.
   "Serial Bluetooth Terminal" on Android).
2. In your phone's Bluetooth settings, **pair** with **ESP32_RC_Car**.
3. Open the terminal app, connect to `ESP32_RC_Car`, and send these:

   | Send | Does |
   |---|---|
   | `F` | drive forward (latches — keeps going) |
   | `B` | drive backward (latches) |
   | `S` | **stop** |
   | `L` | steer a step further left |
   | `R` | steer a step further right |
   | `C` | center steering |
   | `+` | speed up one step |
   | `-` | slow down one step |
   | `V160` | set exact speed 0–255 (needs a newline) |
   | `A95` | set exact steering angle 0–180 (needs a newline) |

   **Driving latches:** tap `F` once and the car keeps moving until you tap
   `S` — you don't have to spam the button. It also stops automatically if the
   Bluetooth connection drops. (The old 1-second auto-stop is off by default; set
   `AUTO_STOP_MS` in the firmware to e.g. `1500` if you want that watchdog back.)

   **Speed `+` / `-`:** these single characters work no matter how your app sets
   line endings, so they're ideal for "speed ++ / speed --" buttons. They're
   floored so "slow down" can't drop the car below the point where it stalls; use
   `S` to stop. `V160`-style commands set an exact speed but **must end with a
   newline**, or your app just buffers the text and nothing happens.

   **Steering:** `L`/`R` now nudge the wheels a step at a time (tap again to turn
   more); `C` re-centers. For smooth proportional steering, use the web remote's
   steering slider (below) or map an app slider to send `A<angle>` (e.g. `A60`).

   ### Using a BLE controller app (Nordic UART)
   If you flashed `rc_car_ble` and drive it from a downloaded BLE controller app,
   set each on-screen button to send one of the characters above (`F`, `B`, `S`,
   `L`, `R`, `C`, `+`, `-`). Make sure there's a **Stop** button sending `S`.

### (Optional) Web remote instead of a terminal app
If you flashed `rc_car_ble` instead, open `app/index.html` in **Chrome on
Android or desktop** (not iOS Safari — it can't do Web Bluetooth), tap
**Connect**, pick `ESP32_RC_Car`, and hold the on-screen buttons to drive. It has
a **steering slider** for smooth proportional steering and a speed slider, and it
re-sends while you hold a button so driving stays smooth.

## Step 9 — First drive checklist (do this with wheels off the ground)

Test in this order so a wiring mistake doesn't send the car off a table:

1. Servo only — send `A90`, `A60`, `A120`. Steering should move and center.
2. One motor — send `F` briefly, then `S`. Confirm it spins.
3. Both motors — `F`/`B`/`S`.
4. Steering + drive together.

## When something's wrong

| Symptom | Fix |
|---|---|
| No COM port shows up | Bad/charge-only cable, or missing CP2102 driver |
| Upload stuck at `Connecting...` | Hold **BOOT** during upload |
| `ESP32_RC_Car` not in Bluetooth list | You flashed the BLE version — it won't show in normal pairing; use the web remote. Or flash `rc_car_classic`. |
| A motor spins the wrong way | Swap that motor's two wires on the L298N |
| Steering reversed | Swap `leftAngle` / `rightAngle` in the code |
| Have to tap forward over and over to keep moving | Your app sends the command once per tap; the firmware now **latches** (tap `F` to go, `S` to stop). Re-flash if you're on an old build. |
| Speed `+`/`-` buttons do nothing | Map them to send the single characters `+` and `-`. `V160`-style commands need a trailing newline — set your app's line ending to Newline, or just use `+`/`-`. |
| Car barely crawls | Tap `+` a few times, or raise `currentSpeed` in the firmware. A weak/low battery also drops speed under the L298N's ~2 V loss. |
| Motors don't change speed with `V` | ENA/ENB jumpers still on the L298N — remove them. Also make sure the `V###` command ends with a newline. |
| ESP32 resets when motors start | Add the 470–1000µF capacitor; don't power motors from the ESP32 |
| Motors don't move at all | Check the L298N ENA/ENB are wired (GPIO 4 / 5) and grounds are shared |

See [hardware-setup.md](hardware-setup.md) for the wiring those pins expect.
