# HIP Test Stand — Pseudocode

One board, one test, two buttons, one hardware switch.

---

## Hardware

| Signal | Pin | Notes |
|---|---|---|
| HX711 clock (`HX_SCK`) | 2 | load cell amplifier |
| HX711 data (`HX_DT`) | 3 | load cell amplifier |
| Fire button (`FIRE_BTN`) | 4 | to GND, internal pull-up |
| Solenoid drive (`SOL_CMD`) | 5 | logic-level MOSFET gate |
| Igniter drive (`IGN_CMD`) | 6 | logic-level MOSFET gate |
| Arm button (`ARM_BTN`) | 7 | to GND, internal pull-up |
| microSD | on-board | `BUILTIN_SDCARD` |
| Igniter power switch | — | **physical, in series with the igniter supply** |

The key switch is not read by the Teensy. It is the real off switch.

---

## States

```
IDLE  --ARM-->  STANDBY  --FIRE-->  RUN  --ARM-->  DONE
                  |                  |
                  |                  +-- 10 s timeout --> DONE
                  +-- SD or load cell fails --> IDLE
```

## Flow

```
setup
    drive SOL_CMD and IGN_CMD LOW
    configure buttons, HX711, serial

loop forever
    read ARM button
        IDLE      -> enter standby
        RUN       -> end test
    read FIRE button
        STANDBY   -> start run
        IDLE      -> ignore ("press ARM first")

    read load cell if a sample is ready
    service the run timers
    every 20 ms: write a log row
```

## Enter standby (ARM)

```
initialise SD card                    fail -> stay in IDLE
open TEST.CSV, write header
average 12 load cell readings (Tare)   fail -> stay in IDLE
store average as the zero reference
state = STANDBY
```

The stand is now sitting at zero, logging its zero reference.

## Calibration 

```
HIP test stand Firmware
fire outputs: DISABLED (test mode)
press ARM to enter standby

ARM: initialising SD card...
ARM: taring load cell...
  zero reference = 412355 counts
  STANDBY - press FIRE to run
  CALIBRATION: put a known mass on the stand and read
  'calibration number' below, once it settles.
calibration number=6  raw=412361  zero=412355  //Cal should be about 0
calibration number=632545  raw=1044900  zero=412355  //Known mass added
calibration number=632543  raw=1044898  zero=412355
```
Operator takes calibration number and known mass enters them into config.h

```
constexpr float   kCalMassKg = 5.0f;     // the known mass
constexpr int32_t kCalCounts = 632545;   // 'calibration number' from serial
```

## Start run (FIRE)

```
state = RUN
t0 = now
```

## Run timers

```
t = now - t0
at   300 ms   solenoid ON
at   450 ms   igniter ON        (logged, held 1200 ms)
at  1650 ms   igniter OFF
at  10000 ms   end test
```

## End test (ARM, or timeout)

```
solenoid OFF, igniter OFF
write the final log row
flush and close TEST.CSV
state = DONE
```

## Log file

`TEST.CSV` on the microSD, 50 Hz, one file for the whole test:

```
boot_ms,run_ms,state,raw,newton,sol,ign
10521,0,STANDBY,412355,-0.001,0,0
# 18240 7719 RUN START
18260,20,RUN,412401,0.003,0,0
# 18540 7999 solenoid ON
```

`run_ms = 0` before the fire press, so plotting against `run_ms`
puts thrust and valve timing on the same time axis.

---

## Safety interlocks

1. **Hardware switch** breaks the igniter supply independently of the Teensy.
2. **Gate pull-downs**  hold both MOSFETs off through reset, boot and power loss.
3. **Test mode** (`kFireEnabled = false`) runs the full sequence and
   logs normally while the drive pins stay low. Flip it only for the
   live test.
4. **No fire from IDLE** — the stand must be tared and logging first.
5. **Hard 10 s limit** ends the run even if the button is never pressed.
