# Hardware Wiring & Pin Map

This document mirrors the physical layout in `hardware/layout-photo.jpeg` and matches
the pin definitions in `src/rover_main.ino` exactly.

> ✅ Confirmed against my build: Nano **5V** powers the **HC-05 VCC** and **HC-SR04 VCC**,
> and the HC-SR04 **Echo** is wired to Nano **D10**.

---

## 1. Master Pin Map (Arduino Nano)

| Nano Pin  | Connects To            | Direction | Purpose                   |
|-----------|------------------------|-----------|---------------------------|
| D2        | HC-SR04 `Trig`         | OUT       | Ultrasonic trigger        |
| D10 (PWM) | HC-SR04 `Echo`         | IN        | Ultrasonic echo           |
| D5 (PWM)  | L298N `ENA`            | OUT       | Left motor speed          |
| D6        | L298N `IN1`            | OUT       | Left motor direction      |
| D7        | L298N `IN2`            | OUT       | Left motor direction      |
| D8        | L298N `IN3`            | OUT       | Right motor direction     |
| D9        | L298N `IN4`            | OUT       | Right motor direction     |
| D3 (PWM)  | L298N `ENB`            | OUT       | Right motor speed         |
| D11       | HC-05 `TXD`            | IN        | Bluetooth RX (SoftSerial) |
| D12       | HC-05 `RXD`            | OUT       | Bluetooth TX (SoftSerial) |
| 5V        | HC-05 VCC, HC-SR04 VCC | PWR       | Logic / sensor power      |
| GND       | Common ground (all)    | PWR       | Shared ground             |

> Note: `Echo` on D10 only uses `pulseIn()` (digital read), so it does not need D10's PWM
> capability. `ENB` was moved to D3, which **is** PWM-capable, so speed control still works.

---

## 2. HC-SR04 Ultrasonic Sensor

| HC-SR04 | → | Nano |
|---------|---|------|
| VCC     | → | 5V   |
| Trig    | → | D2   |
| Echo    | → | D10  |
| GND     | → | GND  |

The HC-SR04 `Echo` outputs 5V, which is safe for the Nano.

---

## 3. HC-05 Bluetooth Module

| HC-05 | → | Nano | Note |
|-------|---|------|------|
| VCC   | → | 5V   | shared with HC-SR04 VCC |
| GND   | → | GND  | |
| TXD   | → | D11  | HC-05 sends → Nano reads (5V safe) |
| RXD   | → | D12  | **Add a voltage divider** (see below) |
| STATE | → | (NC) | optional |
| EN    | → | (NC) | leave floating for normal data mode |

### ⚠️ Voltage divider on HC-05 RXD
The HC-05 RXD pin is **3.3V tolerant**, but the Nano outputs 5V on D12. Drop it with a
divider to avoid slowly damaging the module:

```
Nano D12 ──[ 1kΩ ]──┬── HC-05 RXD
                    │
                  [ 2kΩ ]
                    │
                   GND
```

---

## 4. L298N Motor Driver

### Logic / control side → Nano
| L298N | → | Nano |
|-------|---|------|
| ENA   | → | D5   |
| IN1   | → | D6   |
| IN2   | → | D7   |
| IN3   | → | D8   |
| IN4   | → | D9   |
| ENB   | → | D3   |

Remove the ENA/ENB jumpers so PWM speed control works.

### Power / output side
| L298N Terminal | Connects To                              |
|----------------|------------------------------------------|
| +12V (VS)      | Battery pack **+**                       |
| GND            | Battery pack **−** AND Nano GND (common) |
| +5V (out)      | Nano `5V` pin (keep onboard 5V jumper ON)|
| OUT1 / OUT2    | Left Motor (+ / −)                       |
| OUT3 / OUT4    | Right Motor (+ / −)                      |

> If the battery pack exceeds ~12V, **do not** feed the L298N 5V output to the Nano.
> Power the Nano separately (e.g., USB or its own regulator) and only share GND.

---

## 5. Power & Batteries

- Two cells in **series** (e.g., 2× 18650 ≈ 7.4V) → L298N `+12V` and `GND`.
- The L298N onboard regulator produces 5V → powers the Nano.
- The Nano's **5V pin** then feeds the **HC-05 VCC** and **HC-SR04 VCC**.
- **All grounds must be common** (Nano GND, L298N GND, battery −).

```
[Battery +]───────────────► L298N +12V
[Battery −]───────┬────────► L298N GND
                  └────────► Nano GND  (common ground)

L298N +5V ────────────────► Nano 5V ──► HC-05 VCC, HC-SR04 VCC
```

---

## 6. Motor direction sanity check
After uploading, send `F` (forward). If a wheel spins backward, swap that motor's two
OUT wires on the L298N (e.g., swap OUT1 ↔ OUT2). No code change needed.
