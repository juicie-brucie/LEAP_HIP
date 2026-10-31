# BURPG HIP — Rev DD LTspice simulation

Simulation set for the Rev DD schematic: Teensy 4.1 at 3.3 V logic, IRLB8721PBF
MOSFETs, relay removed, igniter driven directly from a 7 V bench supply.

Run date: 2026-10-31 · LTspice 26.0.2 for macOS · 27 °C

---

## Device model

`irlb8721_vdm.lib` is an LTspice VDMOS model calibrated against the Infineon
IRLB8721PBF datasheet:

| Parameter | Datasheet (typ, 25 °C) | Model |
| --- | --- | --- |
| R<sub>DS(on)</sub> @ V<sub>GS</sub> = 10 V | 6.5 mΩ | 6.56 mΩ |
| R<sub>DS(on)</sub> @ V<sub>GS</sub> = 4.5 V | 13.1 mΩ | 13.33 mΩ |
| V<sub>GS(th)</sub> | 1.8 V (1.35–2.35) | 1.8 V |
| Q<sub>g</sub> @ 4.5 V | 7.6 nC | 10.8 nC (model runs high) |
| V<sub>DS</sub> / I<sub>D</sub> / P<sub>tot</sub> | 30 V / 62 A / 65 W | — |

A second model, `IRLB8721_WC`, uses V<sub>GS(th)</sub> = 2.35 V (datasheet
maximum) to bracket worst-case gate drive.

Limitations: no self-heating, no avalanche model, no package inductance.
R<sub>DS(on)</sub> below 4.5 V is an extrapolation — the datasheet does not
characterise the device there.

---

## 1. Button debounce at 3.3 V

Circuit (as drawn): 10 kΩ pull-up to 3V3, 2.2 µF to GND, switch to GND.
**Active LOW** — pressed = 0 V.

Teensy 4.1 / i.MX RT1062 thresholds: V<sub>IH</sub> = 0.7 × 3.3 = **2.31 V**,
V<sub>IL</sub> = 0.3 × 3.3 = **0.99 V**.

| R<sub>pu</sub> | τ = R·C | press → V<sub>IL</sub> | peak during 0.5 ms bounce | peak during 5 ms bounce | release → V<sub>IH</sub> |
| --- | --- | --- | --- | --- | --- |
| 4.7 kΩ | 10.3 ms | 27 µs | 0.32 V | 1.27 V | 16.4 ms |
| 6.9 kΩ | 15.2 ms | 27 µs | 0.32 V | 0.93 V | 22.3 ms |
| 10 kΩ | 22.0 ms | 27 µs | 0.32 V | 0.67 V | 30.5 ms |

*6.9 kΩ is the external 10 kΩ in parallel with the Teensy's internal
`INPUT_PULLUP` (~22 kΩ).*

**Result: PASS.** Bounce never comes within 1 V of V<sub>IH</sub>, so no false
release is possible even with a 5 ms-open switch. Press detection is
instantaneous (27 µs) because the capacitor discharges through the 10 Ω
contact, not the pull-up. The pull-up only slows the *release*, which is the
harmless direction.

Firmware `kDebounceMs = 30` sits on top of a hardware release delay of
16–30 ms, giving ~46–60 ms total release debounce. That is conservative and
appropriate for a fire control.

> Rev B's debounce analysis modelled a **5 V pull-down / active-high** circuit.
> The board actually uses a **3.3 V pull-up / active-low** circuit, so those
> earlier numbers do not apply to this revision.

---

## 2. Gate drive

Teensy pin (≈50 Ω output) → 220 Ω → gate, 10 kΩ pulldown.

| Quantity | Value |
| --- | --- |
| Steady V<sub>GS</sub> | 3.213 V (10 k / 10.27 k divider) |
| V<sub>GS(th)</sub> max | 2.35 V → **0.86 V margin** |
| Peak gate current | 12 mA |
| Gate charge delivered | 10.8 nC |
| Turn-on 10–90 % | 912 ns |
| R<sub>DS(on)</sub> @ 3.21 V | **21.3 mΩ** |
| R<sub>DS(on)</sub> @ 4.5 V (datasheet point) | 13.2 mΩ |
| R<sub>DS(on)</sub> @ 10 V | 6.55 mΩ |

**Result: turns on reliably, but is not "fully enhanced" in the datasheet sense.**
3.21 V is above V<sub>GS(th)</sub> max, so the device is solidly on, but the
datasheet only guarantees R<sub>DS(on)</sub> at 4.5 V and 10 V. At 3.3 V drive
the channel resistance is roughly 1.6× the 4.5 V figure. It is adequate for
both loads here (see below) but it is outside the characterised operating
point.

---

## 3. Q1 — solenoid drive

12 V rail, 100 µH / 5 Ω solenoid, 1N5819 flyback, 3.3 V gate, 9.7 s worst case.

| Quantity | Typical V<sub>th</sub> | Worst-case V<sub>th</sub> |
| --- | --- | --- |
| Coil current | 2.36 A | 2.36 A |
| V<sub>DS</sub> on | 53 mV | 84 mV |
| R<sub>DS(on)</sub> effective | 22.5 mΩ | 35.7 mΩ |
| Q1 dissipation | 125 mW | 198 mW |
| V<sub>DS</sub> peak (flyback) | 12.2 V | 12.2 V |
| Energy over 9.7 s | 1.2 J | 1.9 J |

**Result: PASS.** 12.2 V against a 30 V rating. Dissipation is 1/500th of the
package rating. Junction rise over a 9.7 s run ≈ 2.5 K.

---

## 4. Q2 — igniter drive

7 V bench supply, ~1 Ω igniter, 2 µH leads, no flyback diode, 1.2 s hold
(`kIgniterHoldMs`).

| Quantity | Typical V<sub>th</sub> | Worst-case V<sub>th</sub> |
| --- | --- | --- |
| Igniter current | 6.45 A | 6.36 A |
| V<sub>DS</sub> on | 149 mV | 250 mV |
| R<sub>DS(on)</sub> effective | 23.2 mΩ | 39.3 mΩ |
| Q2 dissipation | 964 mW | 1588 mW |
| Energy over 1.2 s | 1.16 J | 1.91 J |
| Rail at the igniter | 6.61 V | 6.61 V |

**Result: PASS.** 6.45 A against a 62 A rating, ~1.6 W against 65 W.
Junction rise over the 1.2 s pulse ≈ 4.5 K (7.5 K worst case).
Continuous worst case if the gate is stuck on: ≈ 102 K rise, T<sub>j</sub> ≈ 127 °C
— inside the 175 °C limit but hot.

---

## 5. Q2 turn-off — the only real risk

The igniter is essentially resistive, but the leads are not. At turn-off the
lead inductance drives the drain above the rail, and there is **no flyback
diode** in the current design.

| Lead inductance | No diode | With Schottky across igniter |
| --- | --- | --- |
| 1 µH | 10.0 V | 8.9 V |
| 3 µH | 13.6 V | 9.1 V |
| 10 µH | **27.5 V** | 9.2 V |

V<sub>DS</sub> rating is **30 V**.

**Result: MARGINAL.** With short leads (1–3 µH, i.e. under ~1 m of wire) the
peak is 10–14 V and there is plenty of margin. With long leads (10 µH, ~5 m)
the peak reaches 27.5 V — only 2.5 V from the rating. Adding a Schottky across
the igniter (cathode to +7 V) clamps it to ~9 V regardless of lead length.

The ringing after the first peak in the plots is an ideal-component artefact;
only the first peak is meaningful.

---

## Recommendations

1. **Add a Schottky across the igniter** (`Dfb`, cathode to +7 V). One part,
   removes the only parameter that can threaten the 30 V rating.
2. **Bench supply current limit** must exceed ~7 A. At 7 V into 1 Ω the supply
   is asked for 7 A; a 5 A supply will fold back and may not fire the igniter.
3. **Button pull-up**: either leave the firmware's `INPUT_PULLUP` (6.9 kΩ
   effective, still safe) or set the pin to `INPUT` to use only the external
   10 kΩ and get the full 22 ms time constant.
4. **Do not lengthen `kIgniterHoldMs`.** Everything passes at 1.2 s; the
   igniter and the FET both get hot quickly beyond that.

---

## Re-running

```sh
LTSPICE=/Applications/LTspice.app/Contents/SharedSupport/ltspice/bin/wine
$LTSPICE --bottle=default --workdir 'C:/Program Files/ADI/LTspice' \
         LTspice.exe -b -run -ascii 'Z:\path\to\q2_igniter_drive.cir'
```

Delete the `-ascii` flag for compact binary `.raw` files. The `.cir` files
reference `irlb8721_vdm.lib` relatively, so the whole folder can be moved.
