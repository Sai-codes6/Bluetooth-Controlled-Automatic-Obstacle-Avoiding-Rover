# 🤖 Bluetooth Controlled Automatic Obstacle Avoiding Rover

A dual-mode Arduino Nano rover that you drive from an Android app over Bluetooth —
with a hard safety override that won't let you crash it. The moment an obstacle appears
within 50 cm ahead, the rover ignores your commands, reverses to safety, and waits for a
clear path before listening to you again.

---

## ✨ Features

- **Dual operating modes** — switch live between **Manual** (Bluetooth) and
  **Autonomous** (self-driving obstacle avoidance).
- **Hard safety override** — in manual mode, an obstacle within 50 cm forces an
  immediate reverse-50 cm-and-stop, locking out app commands until the path is clear.
- **Live speed control** — adjust motor speed 0–9 from an in-app slider.
- **Press-and-hold driving** — intuitive D-pad control via `TouchDown`/`TouchUp`.
- **Clean, modular firmware** — non-blocking sensor reads, a readable state machine, and
  isolated motor-driver functions.
- **Free hardware UART** — HC-05 runs on SoftwareSerial so USB upload/debug always works.

---

## 🛠 Tech Stack

| Layer      | Technology |
|------------|------------|
| MCU        | Arduino Nano (ATmega328P) |
| Firmware   | Arduino C++ (`SoftwareSerial`) |
| Sensing    | HC-SR04 Ultrasonic |
| Comms      | HC-05 Bluetooth (SPP) |
| Drive      | L298N Motor Driver + DC Motors |
| Controller | Android app built in MIT App Inventor |

---

## 📁 Repository Structure

```
.
├── README.md
├── LICENSE
├── .gitignore
├── src/
│   └── rover_main.ino              # Production firmware
├── hardware/
│   ├── schematics.md               # Pin map + wiring guide
│   └── layout-photo.jpeg           # Your physical build reference
└── app/
    └── mit_app_inventor_guide.md   # Android app build guide
```

---

## 🔌 Pin Map (quick reference)

| Function          | Nano Pin |
|-------------------|----------|
| HC-SR04 Trig      | D2       |
| HC-SR04 Echo      | D10      |
| L298N ENA (PWM)   | D5       |
| L298N IN1 / IN2   | D6 / D7  |
| L298N IN3 / IN4   | D8 / D9  |
| L298N ENB (PWM)   | D3       |
| HC-05 TXD → RX    | D11      |
| HC-05 RXD ← TX    | D12      |
| 5V                | HC-05 VCC + HC-SR04 VCC |

Full wiring detail in [`hardware/schematics.md`](hardware/schematics.md).

---

## 🚀 Installation

### Firmware
1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. Open `src/rover_main.ino`.
3. Select **Tools → Board → Arduino Nano** (use **ATmega328P (Old Bootloader)** if a
   clone fails to upload).
4. Select the correct COM port and click **Upload**.
5. **Wiring matters:** the HC-05 RX/TX (D11/D12) draw current — if upload stalls,
   briefly disconnect the HC-05 TXD line, upload, then reconnect.

### Hardware
Wire everything per [`hardware/schematics.md`](hardware/schematics.md). Don't skip the
voltage divider on the HC-05 RXD line, and make sure **all grounds are common**.

### Android App
Follow [`app/mit_app_inventor_guide.md`](app/mit_app_inventor_guide.md) to build and
install the controller in MIT App Inventor.

---

## 📖 Usage

1. Power the rover and pair your phone with `HC-05` (PIN `1234`/`0000`).
2. Open the app → **Connect** → select `HC-05`.
3. **Manual:** tap `MANUAL`, hold the D-pad to drive, use the slider for speed.
4. **Autonomous:** tap `AUTONOMOUS` and let it roam.

### Calibrating the reverse distance (do this once)
The override reverses by **time**, not encoders, so it needs to know your rover's actual
reverse speed. The firmware makes this a one-step measurement:

1. Place the rover on a clear floor with a tape measure beside it.
2. Send the `C` command (add a "Calibrate" button in the app, or type `C` in the Arduino
   Serial Monitor). The rover reverses for exactly **2.0 s** at the fixed `BACKUP_SPEED`.
3. Measure how far it traveled, in cm.
4. Set `ROVER_REVERSE_SPEED_CMPS = measured_cm / 2.0` near the top of `rover_main.ino`
   and re-upload. Example: traveled 64 cm → `64 / 2.0 = 32.0`.

Because the safety reverse always uses the fixed `BACKUP_SPEED` (not your live 0–9 speed
setting), this calibration stays accurate no matter how fast you drive.

---

## 📝 License
MIT — see [LICENSE](LICENSE).
