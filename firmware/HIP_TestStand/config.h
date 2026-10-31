// =====================================================================
//  HIP Test Stand - configuration
//  Teensy 4.1
// =====================================================================
#pragma once

#include <Arduino.h>
#include <SD.h>

// ---------------------------------------------------------------- PIN MAP
// Teensy 4.1 pins are 3.3 V only; the load cell module runs from 3V3.
constexpr uint8_t PIN_HX_SCK   = 2;   // HX711 SCK   (HX_SCK)
constexpr uint8_t PIN_HX_DT    = 3;   // HX711 DOUT  (HX_DT)
constexpr uint8_t PIN_FIRE_BTN = 4;   // to GND, INPUT_PULLUP, active LOW
constexpr uint8_t PIN_SOL_CMD  = 5;   // solenoid MOSFET gate  (SOL_CMD)
constexpr uint8_t PIN_IGN_CMD  = 6;   // igniter MOSFET gate   (IGN_CMD)
constexpr uint8_t PIN_ARM_BTN  = 7;   // to GND, INPUT_PULLUP, active LOW

constexpr int PIN_SD_CS = BUILTIN_SDCARD;

// ------------------------------------------------------------------ Dry Run
// kFireEnabled = false boots the board in bench mode: the whole sequence
// and the SD logging runs and Load cell is read, but SOL_CMD / IGN_CMD are
// never driven high. Set it to true for the one live test.
constexpr bool kFireEnabled = false;

// ------------------------------------------------------------------ TIMING
constexpr uint32_t kDebounceMs    = 30;    // button debounce
constexpr uint32_t kSolenoidOnMs  = 300;   // fire press -> SOL_CMD high
constexpr uint32_t kIgniterOnMs   = 450;   // fire press -> IGN_CMD high
constexpr uint32_t kIgniterHoldMs = 1200;  // IGN_CMD high duration
constexpr uint32_t kMaxRunMs      = 10000;  // hard stop for the whole run

constexpr uint32_t kLogPeriodMs = 20;      // 50 Hz log rate
constexpr uint32_t kFlushMs     = 250;     // SD flush cadence

// --------------------------------------------------------- LOAD CELL
// HX711, channel A, gain 128.
constexpr uint8_t  kTareSamples     = 12;
constexpr uint32_t kTareTimeoutMs   = 5000;

// --------------------------------------------------------- CALIBRATION
// Bench procedure: press ARM, put a known mass on the stand, wait for the
// 'calibration number' to settle on the serial monitor, then enter both
// numbers below and re-flash.
//
//   kCalMassKg : the mass you used, in KILOGRAMS   (2.5 kg -> 2.5f)
//   kCalCounts : the 'calibration number' from the serial monitor
//
// Leave both at 0 to stay uncalibrated: the sketch still runs normally and
// the 'newton' column simply tracks raw counts above the tare point.
constexpr float   kCalMassKg = 0.0f;   // <-- ENTER MASS IN KG
constexpr int32_t kCalCounts = 0;      // <-- ENTER CALIBRATION NUMBER

// Derived from the two numbers above. Do not edit.
constexpr float kCountsPerNewton =
    (kCalMassKg > 0.0f && kCalCounts != 0)
        ? (kCalCounts / (kCalMassKg * 9.80665f))
        : 1.0f;

// While in STANDBY, print one line every 500 ms:
//     calibration number=<raw minus zero>  raw=<n>  zero=<n>
// Set to false for the live test so the monitor stays quiet.
constexpr bool     kCalibrationOutput   = true;
constexpr uint32_t kCalibrationPeriodMs = 500;
