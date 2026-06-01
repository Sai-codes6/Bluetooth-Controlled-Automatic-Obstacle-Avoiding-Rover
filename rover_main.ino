/**************************************************************************************
 *  Bluetooth Controlled Automatic Obstacle Avoiding Rover
 *  ------------------------------------------------------------------------------------
 *  Board   : Arduino Nano (ATmega328P)
 *  Sensors : HC-SR04 Ultrasonic
 *  Comms   : HC-05 Bluetooth (SoftwareSerial)
 *  Drive   : L298N Motor Driver + 2x/4x DC Motors
 *
 *  DUAL-MODE STATE MACHINE
 *    MODE_MANUAL      -> Drive from the MIT App Inventor app (F/B/L/R/S).
 *    MODE_AUTONOMOUS  -> Rover avoids obstacles on its own.
 *
 *  SAFETY OVERRIDE (Manual mode only)
 *    If an obstacle is detected within OBSTACLE_THRESHOLD_CM ahead, the rover:
 *      1. Immediately ignores all app movement commands.
 *      2. Reverses a fixed distance (BACKUP_DISTANCE_CM).
 *      3. Stops completely and stays stopped until a NEW, SAFE command
 *         (path clear) arrives from the app.
 *
 *  Author : <your-name>
 *  License: MIT
 **************************************************************************************/

#include <SoftwareSerial.h>

/* ====================================================================================
 *  PIN DEFINITIONS  (match /hardware/schematics.md exactly)
 * ==================================================================================== */
// --- HC-05 Bluetooth (SoftwareSerial) ---
const uint8_t BT_RX_PIN = 11;   // Nano D11 <- HC-05 TXD
const uint8_t BT_TX_PIN = 12;   // Nano D12 -> HC-05 RXD (use voltage divider!)

// --- HC-SR04 Ultrasonic ---
const uint8_t TRIG_PIN = 2;     // Nano D2  -> HC-SR04 Trig
const uint8_t ECHO_PIN = 10;    // Nano D10 <- HC-SR04 Echo

// --- L298N Motor Driver ---
const uint8_t ENA = 5;          // PWM, left motor speed
const uint8_t IN1 = 6;          // left motor direction
const uint8_t IN2 = 7;          // left motor direction
const uint8_t IN3 = 8;          // right motor direction
const uint8_t IN4 = 9;          // right motor direction
const uint8_t ENB = 3;          // PWM, right motor speed

/* ====================================================================================
 *  TUNABLE CONFIGURATION
 * ==================================================================================== */
const uint16_t OBSTACLE_THRESHOLD_CM = 50;   // "danger zone" ahead
const uint16_t BACKUP_DISTANCE_CM    = 50;   // how far to reverse on override

// The safety reverse ALWAYS runs at this fixed speed (NOT the live driveSpeed),
// so the timed backup distance is repeatable no matter what speed you're driving.
const uint8_t  BACKUP_SPEED = 200;           // 0-255

//   1. Upload the code, connect, and set your speed.
//   2. Send the 'C' command -> rover reverses for exactly CALIB_DURATION_MS.
//   3. Measure the distance it traveled in cm.
//   4. ROVER_REVERSE_SPEED_CMPS = measured_cm / (CALIB_DURATION_MS / 1000.0)
//      e.g. if it went 64 cm in 2.0 s -> 64 / 2.0 = 32.0
// The placeholder below is a guess; replace it with YOUR measured value.
const float    ROVER_REVERSE_SPEED_CMPS = 30.0;
const unsigned long CALIB_DURATION_MS   = 2000; // reverse time during 'C' calibration

uint8_t  driveSpeed = 200;                   // 0-255, set live from app (0-9)
const unsigned long SENSOR_INTERVAL_MS = 40; // how often we ping the sensor
const unsigned long ECHO_TIMEOUT_US    = 25000UL; // ~4 m max range

/* ====================================================================================
 *  STATE
 * ==================================================================================== */
SoftwareSerial btSerial(BT_RX_PIN, BT_TX_PIN);

enum DriveMode { MODE_MANUAL, MODE_AUTONOMOUS };
DriveMode mode = MODE_MANUAL;

// Safety-override sub-state machine (used in manual mode)
enum OverridePhase { OV_NONE, OV_BACKING, OV_WAITING_SAFE };
OverridePhase overridePhase = OV_NONE;
unsigned long backupStartMs = 0;
unsigned long backupDurationMs = 0;

char manualCommand = 'S';   // latest applied manual command
bool commandIsNew  = false; // a fresh movement byte arrived this cycle

long lastDistanceCm = 999;
unsigned long lastSensorMs = 0;

/* ====================================================================================
 *  SETUP
 * ==================================================================================== */
void setup() {
  Serial.begin(9600);        // USB debug
  btSerial.begin(9600);      // HC-05 default baud

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  // Convert the fixed reverse distance into a timed duration.
  backupDurationMs = (unsigned long)((BACKUP_DISTANCE_CM / ROVER_REVERSE_SPEED_CMPS) * 1000.0);

  stopMotors();
  Serial.println(F("Rover ready. Mode = MANUAL"));
}

/* ====================================================================================
 *  MAIN LOOP
 * ==================================================================================== */
void loop() {
  processBluetooth();          // 1. read incoming commands
  updateDistance();            // 2. refresh ultrasonic reading (throttled)

  switch (mode) {
    case MODE_MANUAL:      handleManualMode();      break;
    case MODE_AUTONOMOUS:  handleAutonomousMode();  break;
  }
}

/* ====================================================================================
 *  BLUETOOTH COMMAND PARSER
 *  Accepts single-character commands from the app:
 *    Movement : F B L R S
 *    Mode     : M (manual)  A (autonomous)
 *    Speed    : 0-9  (maps 0..255)
 * ==================================================================================== */
void processBluetooth() {
  while (btSerial.available()) {
    char c = btSerial.read();

    switch (c) {
      case 'M':  // switch to manual
        mode = MODE_MANUAL;
        resetOverride();
        manualCommand = 'S';
        stopMotors();
        Serial.println(F("Mode -> MANUAL"));
        break;

      case 'A':  // switch to autonomous
        mode = MODE_AUTONOMOUS;
        resetOverride();
        stopMotors();
        Serial.println(F("Mode -> AUTONOMOUS"));
        break;

      case 'C':  // run reverse-speed calibration (see top of file)
        runCalibration();
        break;

      case '0' ... '9':  // GCC range-case: live speed control
        driveSpeed = map(c - '0', 0, 9, 0, 255);
        Serial.print(F("Speed -> ")); Serial.println(driveSpeed);
        break;

      case 'F': case 'B': case 'L': case 'R': case 'S':
        // Movement commands only matter in manual mode.
        if (mode == MODE_MANUAL) {
          manualCommand = c;
          commandIsNew  = true;   // consumed by the override logic
        }
        break;

      default:
        break; // ignore unknown bytes
    }
  }
}

/* ====================================================================================
 *  MANUAL MODE  (with safety override)
 * ==================================================================================== */
void handleManualMode() {
  bool obstacleAhead = (lastDistanceCm > 0 && lastDistanceCm < OBSTACLE_THRESHOLD_CM);

  // --- Trigger override the instant an obstacle appears ---
  if (overridePhase == OV_NONE && obstacleAhead) {
    overridePhase = OV_BACKING;
    backupStartMs = millis();
    moveBackward(BACKUP_SPEED);   // fixed speed -> repeatable distance
    Serial.println(F("!! OBSTACLE -> override: reversing"));
    return;
  }

  // --- Override in progress ---
  if (overridePhase == OV_BACKING) {
    if (millis() - backupStartMs >= backupDurationMs) {
      stopMotors();
      overridePhase = OV_WAITING_SAFE;
      Serial.println(F("Override: reversed. Waiting for safe command."));
    }
    commandIsNew = false;        // discard any app input during reverse
    return;
  }

  if (overridePhase == OV_WAITING_SAFE) {
    // Stay stopped. Only a NEW command + clear path releases the override.
    if (commandIsNew) {
      commandIsNew = false;
      bool pathClear = (lastDistanceCm == 0 || lastDistanceCm >= OBSTACLE_THRESHOLD_CM);
      if (pathClear) {
        resetOverride();
        applyCommand(manualCommand);
        Serial.println(F("Override cleared. Resuming manual control."));
      } else {
        Serial.println(F("Command ignored: path still blocked."));
      }
    }
    return;
  }

  // --- Normal manual driving ---
  commandIsNew = false;
  applyCommand(manualCommand);
}

/* ====================================================================================
 *  AUTONOMOUS MODE  (simple obstacle avoidance)
 * ==================================================================================== */
void handleAutonomousMode() {
  bool obstacleAhead = (lastDistanceCm > 0 && lastDistanceCm < OBSTACLE_THRESHOLD_CM);

  if (obstacleAhead) {
    stopMotors();           delay(120);
    moveBackward(driveSpeed); delay(300);
    stopMotors();           delay(120);
    turnRight(driveSpeed);  delay(400);   // pivot to seek a clear path
    stopMotors();
  } else {
    moveForward(driveSpeed);
  }
}

/* ====================================================================================
 *  ULTRASONIC DISTANCE  (throttled, non-spammy)
 * ==================================================================================== */
void updateDistance() {
  if (millis() - lastSensorMs < SENSOR_INTERVAL_MS) return;
  lastSensorMs = millis();

  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long us = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  // 0 = timeout / out of range. Treat as "clear" (large distance).
  lastDistanceCm = (us == 0) ? 999 : (long)(us / 58.0);
}

/* ====================================================================================
 *  COMMAND -> MOTION MAPPING
 * ==================================================================================== */
void applyCommand(char cmd) {
  switch (cmd) {
    case 'F': moveForward(driveSpeed);  break;
    case 'B': moveBackward(driveSpeed); break;
    case 'L': turnLeft(driveSpeed);     break;
    case 'R': turnRight(driveSpeed);    break;
    case 'S':
    default:  stopMotors();             break;
  }
}

/* ====================================================================================
 *  LOW-LEVEL MOTOR DRIVERS (L298N)
 *  Left motor  = IN1/IN2 + ENA
 *  Right motor = IN3/IN4 + ENB
 * ==================================================================================== */
void moveForward(uint8_t spd) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENA, spd);   analogWrite(ENB, spd);
}

void moveBackward(uint8_t spd) {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  analogWrite(ENA, spd);   analogWrite(ENB, spd);
}

void turnLeft(uint8_t spd) {        // pivot left: left back, right fwd
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENA, spd);   analogWrite(ENB, spd);
}

void turnRight(uint8_t spd) {       // pivot right: left fwd, right back
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  analogWrite(ENA, spd);   analogWrite(ENB, spd);
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);    analogWrite(ENB, 0);
}

/* ==================================================================================== */
void resetOverride() {
  overridePhase = OV_NONE;
  commandIsNew  = false;
}

/* ====================================================================================
 *  CALIBRATION ROUTINE  (triggered by the 'C' command)
 *  Reverses at BACKUP_SPEED for exactly CALIB_DURATION_MS, then stops.
 *  Measure the distance traveled (cm) and update ROVER_REVERSE_SPEED_CMPS:
 *      ROVER_REVERSE_SPEED_CMPS = measured_cm / (CALIB_DURATION_MS / 1000.0)
 * ==================================================================================== */
void runCalibration() {
  resetOverride();
  Serial.println(F("=== CALIBRATION START ==="));
  Serial.print(F("Reversing at BACKUP_SPEED for "));
  Serial.print(CALIB_DURATION_MS / 1000.0); Serial.println(F(" s..."));

  moveBackward(BACKUP_SPEED);
  delay(CALIB_DURATION_MS);   // blocking on purpose: calibration is a deliberate, one-off action
  stopMotors();

  Serial.println(F("Done. Now measure the distance traveled in cm."));
  Serial.print(F("Then set ROVER_REVERSE_SPEED_CMPS = measured_cm / "));
  Serial.println(CALIB_DURATION_MS / 1000.0);
  Serial.println(F("=== CALIBRATION END ==="));
}
