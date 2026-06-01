# MIT App Inventor Build Guide

A step-by-step guide to recreate the controller app at
[ai2.appinventor.mit.edu](https://ai2.appinventor.mit.edu). App Inventor is visual, so
the "blocks" below are written as readable pseudo-blocks you assemble by dragging.

---

## 1. Command Protocol (must match the Arduino code)

The app sends **single ASCII characters**:

| Action            | Char |
|-------------------|------|
| Forward           | `F`  |
| Backward          | `B`  |
| Left              | `L`  |
| Right             | `R`  |
| Stop              | `S`  |
| Manual mode       | `M`  |
| Autonomous mode   | `A`  |
| **Calibrate reverse** | `C`  |
| Speed 0–max       | `0`–`9` |

Send each character with `BluetoothClient.SendText`. The Arduino reads one char at a
time, so do **not** append newlines.

---

## 2. Designer — Component Layout

```
Screen1
├── HorizontalArrangement_Top
│   ├── ListPicker  "PickBT"        (text: "Connect")
│   └── Label       "lblStatus"     (text: "Disconnected")
│
├── HorizontalArrangement_Mode
│   ├── Button "btnManual"          (text: "MANUAL")
│   └── Button "btnAuto"            (text: "AUTONOMOUS")
│
├── TableArrangement_Dpad  (3 cols × 3 rows)
│   │   [ . ][ ▲ Fwd ][ . ]
│   │   [◄ L][ ■ Stop][ R ►]
│   │   [ . ][ ▼ Back][ . ]
│   ├── Button "btnFwd"   "▲"
│   ├── Button "btnBack"  "▼"
│   ├── Button "btnLeft"  "◄"
│   ├── Button "btnRight" "►"
│   └── Button "btnStop"  "■"
│
├── Slider "sldSpeed"   (MinValue 0, MaxValue 9, ThumbPosition 6)
├── Label  "lblSpeed"   (text: "Speed: 6")
│
└── NON-VISIBLE
    └── BluetoothClient  "BTClient"
```

UI tips: give the D-pad buttons a fixed size (e.g., 80×80), color the Stop button red,
and disable the movement buttons until connected.

---

## 3. Bluetooth Settings

- Component: **BluetoothClient** (Connectivity drawer), non-visible.
- HC-05 pairing PIN: usually `1234` or `0000` (pair once in Android Bluetooth settings).
- App Inventor uses the classic **SPP** profile automatically — you do **not** enter a
  UUID manually; `BluetoothClient.Connect` handles the default SPP UUID
  `00001101-0000-1000-8000-00805F9B34FB`.
- Default baud must match firmware: **9600**.

---

## 4. Blocks Logic (pseudo-blocks)

### A. Populate & connect
```
when PickBT.BeforePicking:
    set PickBT.Elements = BTClient.AddressesAndNames

when PickBT.AfterPicking:
    set connected = call BTClient.Connect(address = PickBT.Selection)
    if connected:
        set lblStatus.Text = "Connected"
        set lblStatus.TextColor = green
    else:
        set lblStatus.Text = "Failed"
        set lblStatus.TextColor = red
```

### B. Mode buttons
```
when btnManual.Click:
    call BTClient.SendText("M")

when btnAuto.Click:
    call BTClient.SendText("A")
```

### C. Movement (press to move, release to stop — feels natural)
```
when btnFwd.TouchDown:   if BTClient.IsConnected then SendText("F")
when btnFwd.TouchUp:     if BTClient.IsConnected then SendText("S")

when btnBack.TouchDown:  if BTClient.IsConnected then SendText("B")
when btnBack.TouchUp:    if BTClient.IsConnected then SendText("S")

when btnLeft.TouchDown:  if BTClient.IsConnected then SendText("L")
when btnLeft.TouchUp:    if BTClient.IsConnected then SendText("S")

when btnRight.TouchDown: if BTClient.IsConnected then SendText("R")
when btnRight.TouchUp:   if BTClient.IsConnected then SendText("S")

when btnStop.Click:      if BTClient.IsConnected then SendText("S")
```
> Use `TouchDown`/`TouchUp` (under "Other" events) instead of `Click` so the rover
> moves only while a button is held. After the safety override stops the rover, simply
> releasing and pressing again sends a fresh "new safe command".

### D. Speed slider
```
when sldSpeed.PositionChanged (thumbPosition):
    set lblSpeed.Text = join("Speed: ", round(thumbPosition))
    if BTClient.IsConnected:
        call BTClient.SendText( round(thumbPosition) )   // sends "0".."9"
```

### E. (Optional) Calibration button
Add a `Button "btnCalibrate"` (text: "Calibrate Reverse"). Run it once on a clear floor,
then measure how far the rover backed up and update `ROVER_REVERSE_SPEED_CMPS` in the
firmware.
```
when btnCalibrate.Click:
    if BTClient.IsConnected then SendText("C")
```

---

## 5. Testing checklist
1. Power the rover; the HC-05 LED blinks (unpaired) then turns solid (connected).
2. In the app, tap **Connect** → pick `HC-05`.
3. Tap **MANUAL**, then hold **▲**. Place your hand <50cm in front → rover should
   reverse ~50cm and stop, ignoring the held button.
4. Move your hand away, release and press **▲** again → rover resumes.
5. Tap **AUTONOMOUS** → rover roams and avoids obstacles by itself.
