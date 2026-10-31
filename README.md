# LEAP_HIP
This is a repo for the LEAP group's HIP project. Contains mostly code but could contain anything from the HIP Project.

# Code
Arduino IDE based, targeting a **Teensy 4.1**.

## Firmware

| Sketch | What it does |
|--------|--------------|
| [`firmware/HIP_TestStand`](firmware/HIP_TestStand/) | One-shot test-stand controller: ARM to tare the load cell and open the log, FIRE to run the solenoid/igniter sequence, ARM again to end the test. |

- [Pseudocode](firmware/HIP_TestStand/PSEUDOCODE.md) — one-page description of the system
- [Prototype sketch](firmware/HIP_TestStand/HIP_TestStand.ino) — the implementation
- [Wiring and safety notes](firmware/HIP_TestStand/README.md)

## Toolchain

- Arduino IDE 2.3.10 (`/Applications/Arduino IDE.app`)
- Teensy core `teensy:avr` **1.62.0**, installed via `arduino-cli`
- Board package URL: `https://www.pjrc.com/teensy/package_teensy_index.json`
- FQBN: `teensy:avr:teensy41`

### Build from the terminal

```sh
# compile
arduino-cli compile --fqbn teensy:avr:teensy41 firmware/HIP_TestStand

# find the board, then upload
arduino-cli board list
arduino-cli upload --fqbn teensy:avr:teensy41 -p /dev/cu.usbmodemXXXX firmware/HIP_TestStand

# serial console
arduino-cli monitor -p /dev/cu.usbmodemXXXX -c baudrate=115200
```

### Build in the IDE

Open `firmware/HIP_TestStand/HIP_TestStand.ino`, then select
**Tools → Board → Teensy 4.1**. The Teensy board package is already
installed system-wide, so it should appear in the board list directly.


# KiCad
Too tired to add anything to help you download KiCad, the fucking Teensy library etc j

#LTspice
More way too tired to help someone setup and replicate the LTspice rn
