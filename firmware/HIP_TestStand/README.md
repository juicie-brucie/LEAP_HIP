# HIP Test Stand

Teensy 4.1 controller for a test fire: arm the stand,
fire it, log thrust to the on-board microSD.

---

## Operation

| Press | From | Result |
|---|---|---|
| **ARM** | IDLE | init SD, tare the load cell, open `TEST.CSV` → **STANDBY** |
| **FIRE** | STANDBY | solenoid, then igniter, logging at 50 Hz → **RUN** |
| **ARM** | RUN | outputs off, log closed → **DONE** |

There is no restart path. Power cycle the board to run again.

`FIRE` is ignored in `IDLE` — the stand must be tared and logging first.
A 10 s hard limit ends the run even if ARM is never pressed to end recording.

## Pin map (Teensy 4.1)

| Schematic pin name | Pin | Function |
|---|---|---|
| `HX_SCK` | 2 | HX711 clock |
| `HX_DT` | 3 | HX711 data |
| `FIRE_BTN` | 4 | fire button, to GND |
| `SOL_CMD` | 5 | solenoid MOSFET gate |
| `IGN_CMD` | 6 | igniter MOSFET gate |
| `ARM_BTN` | 7 | arm button, to GND |
| — | on-board | microSD, `BUILTIN_SDCARD` |

Both buttons use `INPUT_PULLUP` and read active LOW. Pin 13 (LED) is not
used in this build.

## Build and upload

### Arduino IDE 2.x

Teensy Package/core
https://www.pjrc.com/store/teensy41.html

## Log format

`TEST.CSV`, 50 Hz, one file per power cycle:

```
boot_ms,run_ms,state,raw,newton,sol,ign
10521,0,STANDBY,412355,-0.001,0,0
# 18240 7719 RUN START
18260,20,RUN,412401,0.003,0,0
# 18540 7999 solenoid ON
```

## Calibration

1. Power up with no igniter connected, bench mode.
2. Press ARM with the stand unloaded — this tares it.
3. Apply a known mass, note `calibration number` from the serial output or the log.
4. Enter values in config.h `constexpr float   kCalMassKg = 5.0f;     // the known mass
constexpr int32_t kCalCounts = 632545;   // 'calibration number' from serial`.
5. Re-flash and repeat to confirm.
