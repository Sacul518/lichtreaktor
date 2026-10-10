# Lichtreaktor firmware

Arduino C++ for the **Seeed XIAO RP2040**, using Earle Philhower's **Arduino-Pico 6.3.0** core. No additional libraries are required.

## Controls

- Turn the encoder to change brightness in steps of 8, from 0 (off) to 255.
- Press and release to cycle through steady light, breathing, a rotating trail, alternating groups and a double pulse.
- Hold and turn to change effect speed from 25% to 300% in steps of 25%. A speed change restarts the current effect. Release without changing the pattern.
- Hold for 1.2 seconds without turning to enter or leave the LED group test. In the test, turning selects one of the six groups; a short press leaves the test. Brightness is retained, so turn it up before entering if it is currently zero.
- Starts in steady mode at brightness 80/255. Settings reset when power is removed.

The encoder is polled continuously, the button is debounced for 25 ms, and LED values are recalculated and written on every loop without blocking delays. PWM runs at 2 kHz. All LEDs within a group share the same output; this board cannot control individual LEDs separately.

## USB diagnostics

Open a serial monitor at 115200 baud. USB serial is optional; the board does not wait for a computer to connect. Commands are single characters, so a newline is not required (and is ignored).

| Command | Action |
| --- | --- |
| `?` | Show command help |
| `s` | Show pattern, brightness, speed, test mode and selected group |
| `n` | Next pattern and leave test mode |
| `+` / `-` | Increase / decrease brightness |
| `>` / `<` | Increase / decrease speed |
| `t` | Toggle the group test, starting at group 1 |
| `1` through `6` | Enter the test and select that LED group |

Pattern and group numbers in the status output start at 1. Only the selected group lights in test mode. For example, send `1`, then `2`, through `6` to check each transistor and its four LEDs. If brightness is zero, use `+` to raise it. `n` returns to an effect. USB processing is limited to eight incoming characters per loop, and pending replies wait for output buffer space.

Codex helped write the firmware; comments in the sketch identify that assistance. Hardware validation is still pending.

## Pin mapping

Arduino-Pico's raw pin numbers are the RP2040 GPIO numbers. They are **not** the D numbers printed on the module.

| PCB signal | XIAO pin | GPIO used in firmware |
| --- | --- | --- |
| PWM_G1 | D0 | 26 |
| PWM_G2 | D1 | 27 |
| PWM_G3 | D2 | 28 |
| PWM_G4 | D3 | 29 |
| PWM_G5 | D4 | 6 |
| PWM_G6 | D5 | 7 |
| ENC_A | D6 | 0 |
| ENC_B | D7 | 1 |
| ENC_SW | D8 | 2 |

The encoder inputs use the PCB's external 10 kΩ pull-ups to 3.3 V. LED outputs are active high because the BC337 transistors switch the groups to GND.

The EC11E18244A5 has 36 detents and 18 pulses per revolution, so `ENCODER_TRANSITIONS_PER_STEP` is 2. For an encoder substitute, check its datasheet. If clockwise rotation dims instead of brightening, change `ENCODER_DIRECTION` from `1` to `-1` and rebuild.

## Build

Install [Arduino CLI](https://docs.arduino.cc/arduino-cli/installation), then run these commands from the repository root:

```sh
arduino-cli core update-index --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli core install rp2040:rp2040@6.3.0 --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli compile --fqbn rp2040:rp2040:seeed_xiao_rp2040 --warnings all --output-dir firmware/build firmware/lichtreaktor
```

Alternatively, open `lichtreaktor/lichtreaktor.ino` in Arduino IDE, install the same Arduino-Pico core using its package URL, and select **Seeed XIAO RP2040**. Do not select the Plus or RP2350 board.

The build produces `build/lichtreaktor.ino.uf2`. To flash it, connect the XIAO by USB while holding **BOOT**, then copy that UF2 onto the **RPI-RP2** drive. This replaces its existing firmware. On a Mac, from the repository root:

```sh
cp -X firmware/build/lichtreaktor.ino.uf2 /Volumes/RPI-RP2/
```

The drive should disappear and the firmware should start. The build command does not flash a board.

## Verification

A successful build does **not** verify the assembled board. On first hardware bring-up, check each group, encoder direction and detent count, button operation, and brightness at both limits. The 2 kHz electrical PWM and the physical LED behaviour have not been measured yet.

## References

- [Seeed XIAO RP2040 pinout and getting started](https://wiki.seeedstudio.com/XIAO-RP2040/)
- [Arduino-Pico installation](https://arduino-pico.readthedocs.io/en/latest/install.html)
- [Arduino-Pico PWM API](https://arduino-pico.readthedocs.io/en/latest/analog.html)
- [Alps EC11E series specifications, including EC11E18244A5](https://tech.alpsalpine.com/e/products/category/encorders/sub/01/series/ec11e/)
