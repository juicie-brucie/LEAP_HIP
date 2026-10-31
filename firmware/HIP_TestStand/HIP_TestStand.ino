// =====================================================================
//  HIP Test Stand Firmware
//  Board: Teensy 4.1
//
//  ARM  -> standby : init SD, tare the load cell (zero reference)
//  FIRE -> run     : solenoid on, then igniter, log everything to SD
//  ARM  -> end     : shut the outputs off and close the log
//
//  This firmware is written for exactly one test.
// =====================================================================

#include <SD.h>

#include "config.h"

// =====================================================================
//  BUTTONS
// =====================================================================
struct Button {
  uint8_t  pin;
  bool     rawLast;   // raw pin state, true = pressed (LOW)
  bool     stable;    // debounced state
  uint32_t tChange;
  bool     pressed;   // one-shot edge flag, cleared by the caller
};

Button armButton  = { PIN_ARM_BTN,  false, false, 0, false };
Button fireButton = { PIN_FIRE_BTN, false, false, 0, false };

// ------------------------------------------------------------- STATES
enum class State : uint8_t { Idle, Standby, Run, Done };

static const char *stateName(State s) {
  switch (s) {
    case State::Idle:    return "IDLE";
    case State::Standby: return "STANDBY";
    case State::Run:     return "RUN";
    case State::Done:    return "DONE";
  }
  return "?";
}

static void pollButton(Button &b) {
  bool raw = (digitalRead(b.pin) == LOW);   // INPUT_PULLUP, active LOW
  if (raw != b.rawLast) {
    b.rawLast = raw;
    b.tChange = millis();
  }
  if ((millis() - b.tChange) >= kDebounceMs && b.stable != raw) {
    b.stable = raw;
    if (b.stable) b.pressed = true;
  }
}

// =====================================================================
//  STATE
// =====================================================================
State    state       = State::Idle;
File     logFile;
int32_t  tareOffset  = 0;
int32_t  lastRaw     = 0;
float    lastNewton  = 0.0f;


// TIMERS
uint32_t runStartMs  = 0;
uint32_t lastLogMs   = 0;
uint32_t lastFlushMs = 0;
uint32_t lastPrintMs  = 0;

bool     solOn       = false;
bool     ignOn       = false;
bool     ignFired    = false;
uint32_t ignOnAtMs   = 0;

// =====================================================================
//  OUTPUTS
// =====================================================================
static void applyOutputs() {
  digitalWrite(PIN_SOL_CMD, (kFireEnabled && solOn) ? HIGH : LOW);
  digitalWrite(PIN_IGN_CMD, (kFireEnabled && ignOn) ? HIGH : LOW);
}

static void outputsOff() {
  solOn = false;
  ignOn = false;
  applyOutputs();
}

// =====================================================================
//  LOAD CELL - HX711, channel A, gain 128
//  24-bit two's complement, 25 clock pulses per reading.
// =====================================================================
static void lcBegin() {
  pinMode(PIN_HX_SCK, OUTPUT);
  digitalWrite(PIN_HX_SCK, LOW);
  pinMode(PIN_HX_DT, INPUT);
}

static inline bool lcReady() {
  return digitalRead(PIN_HX_DT) == LOW;   // LOW = conversion ready
}

// Returns the raw 24-bit value, or INT32_MIN if the sensor never answers.
static int32_t lcReadRaw() {
  uint32_t t0 = micros();
  while (!lcReady()) {
    if ((micros() - t0) > 150000UL) return INT32_MIN;
    yield();
  }

  uint32_t v = 0;
  noInterrupts();
  for (uint8_t i = 0; i < 24; i++) {
    digitalWriteFast(PIN_HX_SCK, HIGH);
    delayMicroseconds(1);
    v = (v << 1) | (digitalReadFast(PIN_HX_DT) ? 1UL : 0UL);
    digitalWriteFast(PIN_HX_SCK, LOW);
    delayMicroseconds(1);
  }
  digitalWriteFast(PIN_HX_SCK, HIGH);   // 25th pulse: channel A, gain 128
  delayMicroseconds(1);
  digitalWriteFast(PIN_HX_SCK, LOW);
  delayMicroseconds(1);
  interrupts();

  if (v & 0x800000UL) v |= 0xFF000000UL;   // sign extend bit 23
  return (int32_t)v;
}

// Reads a sample only when one is ready; otherwise returns immediately.
static void pollLoadCell() {
  if (!lcReady()) return;
  int32_t raw = lcReadRaw();
  if (raw == INT32_MIN) return;
  lastRaw    = raw;
  lastNewton = (float)(raw - tareOffset) / kCountsPerNewton;
}

// Averages kTareSamples readings and stores them as the zero reference.
static bool tareLoadCell() {
  int32_t sum = 0;
  uint8_t got = 0;
  uint32_t t0 = millis();

  while (got < kTareSamples && (millis() - t0) < kTareTimeoutMs) {
    if (lcReady()) {
      int32_t r = lcReadRaw();
      if (r != INT32_MIN) {
        sum += r;
        got++;
      }
    }
    yield();
  }

  if (got < kTareSamples) return false;
  tareOffset = sum / (int32_t)got;
  return true;
}

// =====================================================================
//  SD LOGGING - one file for the whole test
// =====================================================================
static bool openTestLog() {
  if (!SD.begin(PIN_SD_CS)) return false;
  SD.remove("TEST.CSV");
  logFile = SD.open("TEST.CSV", FILE_WRITE);
  if (!logFile) return false;

  logFile.println("# HIP test stand - single test");
  logFile.printf("# countsPerNewton=%.4f  fireEnabled=%d\n",
                 (double)kCountsPerNewton, (int)kFireEnabled);
  logFile.println(F("boot_ms,run_ms,state,raw,newton,sol,ign"));
  logFile.flush();
  lastFlushMs = millis();
  return true;
}

static void logEvent(const char *text) {
  if (!logFile) return;
  uint32_t runMs = (state == State::Run) ? (millis() - runStartMs) : 0;
  logFile.printf("# %lu %lu %s\n", (unsigned long)millis(),
                 (unsigned long)runMs, text);
}

static void logSample() {
  if (!logFile) return;
  uint32_t runMs = (state == State::Run) ? (millis() - runStartMs) : 0;

  logFile.print(millis());
  logFile.print(',');
  logFile.print(runMs);
  logFile.print(',');
  logFile.print(stateName(state));
  logFile.print(',');
  logFile.print(lastRaw);
  logFile.print(',');
  logFile.print(lastNewton, 3);
  logFile.print(',');
  logFile.print(solOn ? 1 : 0);
  logFile.print(',');
  logFile.println(ignOn ? 1 : 0);
}

static void flushIfDue() {
  if (!logFile) return;
  if ((millis() - lastFlushMs) >= kFlushMs) {
    logFile.flush();
    lastFlushMs = millis();
  }
}

// =====================================================================
//  TEST SEQUENCE
// =====================================================================

// ARM pressed while idle: bring the stand up and take the zero reference.
static void enterStandby() {
  Serial.println(F("ARM: initialising SD card..."));

  if (!openTestLog()) {
    Serial.println(F("  SD card failed - staying in IDLE"));
    return;
  }

  Serial.println(F("ARM: taring load cell..."));
  if (!tareLoadCell()) {
    Serial.println(F("  load cell did not respond - staying in IDLE"));
    logFile.close();
    return;
  }

  state     = State::Standby;
  lastLogMs = millis();
  lastPrintMs = millis();
  logEvent("STANDBY: load cell zeroed");
  Serial.printf("  zero reference = %ld counts\n", (long)tareOffset);
  Serial.println(F("  STANDBY - press FIRE to run"));

  if (kCalibrationOutput) {
    Serial.println(F("  CALIBRATION: put a known mass on the stand and read"));
    Serial.println(F("  'calibration number' below, once it settles."));
  }
}

// FIRE pressed while in standby: start the run.
static void startRun() {
  state       = State::Run;
  runStartMs  = millis();
  solOn       = false;
  ignOn       = false;
  ignFired    = false;
  lastLogMs   = millis();

  logEvent("RUN START");
  Serial.println(F("FIRE: sequence running"));
}

// ARM pressed during a run, or the hard time limit reached.
static void endTest(const char *reason) {
  outputsOff();

  logEvent(reason);
  logSample();
  logFile.flush();
  logFile.close();

  uint32_t total = millis() - runStartMs;
  state = State::Done;

  Serial.printf("TEST END: %s after %lu ms\n", reason,
                (unsigned long)total);
  Serial.println(F("  DONE - power cycle to repeat"));
}

// Non-blocking timing for the fire sequence.
static void serviceRun() {
  if (state != State::Run) return;

  const uint32_t t = millis() - runStartMs;

  if (!solOn && t >= kSolenoidOnMs) {
    solOn = true;
    applyOutputs();
    logEvent("solenoid ON");
  }

  if (!ignFired && t >= kIgniterOnMs) {
    ignFired  = true;
    ignOn     = true;
    ignOnAtMs = millis();
    applyOutputs();
    logEvent("igniter ON");
  }

  if (ignOn && (millis() - ignOnAtMs) >= kIgniterHoldMs) {
    ignOn = false;
    applyOutputs();
    logEvent("igniter OFF");
  }

  if (t >= kMaxRunMs) {
    endTest("max run time reached");
  }
}

// =====================================================================
//  SETUP / LOOP
// =====================================================================
void setup() {
  // Latch the outputs low before switching them to outputs so there is no
  // pulse on boot. Hardware pull-downs remain mandatory.
  digitalWrite(PIN_SOL_CMD, LOW);
  digitalWrite(PIN_IGN_CMD, LOW);
  pinMode(PIN_SOL_CMD, OUTPUT);
  pinMode(PIN_IGN_CMD, OUTPUT);

  pinMode(PIN_ARM_BTN, INPUT_PULLUP);
  pinMode(PIN_FIRE_BTN, INPUT_PULLUP);

  lcBegin();

  Serial.begin(115200);
  Serial.println(F("\nHIP test stand - one-shot prototype"));
  Serial.printf("fire outputs: %s\n",
                kFireEnabled ? "ENABLED" : "DISABLED (test mode)");

  if (kCalMassKg > 0.0f && kCalCounts != 0) {
    Serial.printf("calibration: %.2f counts/N  (mass %.3f kg, %ld counts)\n",
                  (double)kCountsPerNewton, (double)kCalMassKg,
                  (long)kCalCounts);
  } else {
    Serial.println(F("calibration: NOT SET - newton column is raw counts"));
  }

  Serial.println(F("press ARM to enter standby"));
}

void loop() {
  pollButton(armButton);
  pollButton(fireButton);

  if (armButton.pressed) {
    armButton.pressed = false;
    if (state == State::Idle) {
      enterStandby();
    } else if (state == State::Run) {
      endTest("operator ended test");
    }
  }

  if (fireButton.pressed) {
    fireButton.pressed = false;
    if (state == State::Standby) {
      startRun();
    } else if (state == State::Idle) {
      Serial.println(F("FIRE ignored: press ARM first"));
    }
  }

  pollLoadCell();

  if ((state == State::Standby || state == State::Run) &&
      (millis() - lastLogMs) >= kLogPeriodMs) {
    lastLogMs = millis();
    logSample();
    flushIfDue();
  }

  // Calibration readout: one line every kCalibrationPeriodMs, STANDBY only.
  // 'calibration number' is raw minus zero, i.e. counts above the tare point.
  if (kCalibrationOutput && state == State::Standby &&
      (millis() - lastPrintMs) >= kCalibrationPeriodMs) {
    lastPrintMs = millis();
    Serial.printf("calibration number=%ld  raw=%ld  zero=%ld\n",
                  (long)(lastRaw - tareOffset),
                  (long)lastRaw,
                  (long)tareOffset);
  }

  serviceRun();
  applyOutputs();   // keep the pins tied to the logical state
}
