# Lichtreaktor

I'm designing a small, round arc-reactor lookalike that produces light in cool patterns. The plan is to press the rotary encoder to switch patterns, and turn it to control the brightness.

The PCB is 90 mm across, with 24 blue LEDs around the edge, a knob in the middle and a XIAO near the top for USB-C access. Six groups of four LEDs will be controlled independently. Tiny reactor, zero fusion involved.

![Angled view of the Lichtreaktor PCB in KiCad's 3D viewer](assets/kicad/pcb-3d-angle.png)

The LEDs look red in KiCad's default 3D model, but the actual parts are blue. The XIAO and rotary encoder don't have 3D models loaded, so their positions look empty here.

## Where it's at

The schematic and PCB layout are done! All 24 LEDs are placed, the traces are routed, and both copper layers have a GND fill. The LEDs, transistors, encoder and XIAO sit on the front; the resistors sit on the back. Getting everything onto a round board took a few attempts, but it fits.

The latest KiCad check (12 October 2026) reports **0 errors, 0 unconnected items and 0 schematic/PCB mismatches**. There are still 24 silkscreen warnings about overlapping print, small text and print near the board edge. Those are accepted for this layout; parts of the print may be clipped during manufacturing. The USB connector intentionally sticks out a little at the top.

This is still a design, not a tested reactor. The [firmware](firmware/README.md) is written, with ten light patterns, encoder controls, adjustable speed, automatic playback, pause, a group test and USB diagnostics. Final part and mechanical checks, supplier prices, fabrication files, soldering and testing on the actual board are next.

## How it works

USB supplies 5 V through the XIAO. Each LED has its own 220 Ω resistor, and each group of four shares a BC337 transistor that switches its connection to GND. The XIAO drives the six transistors through 470 Ω base resistors; 100 kΩ pull-downs keep them off when the control pins aren't being driven. PWM will let the firmware dim each group separately.

The encoder has two rotation signals and a push button, with three 10 kΩ pull-ups to 3.3 V. Turning it will change the brightness, and pressing it will change the pattern. The GND fills keep clearance from the other nets and use thermal reliefs so the pads are easier to hand-solder.

## What will I need to build this?

- Seeed Studio XIAO RP2040 (same one I used for my super cool and sick Hackpad)
- 24 blue LEDs (WP7113QBD/D)
- 24 LED resistors (220 Ω, 1/4 W), one per LED
- A PCB (self-designed ofc)
- 6 BC337 NPN transistors, one per group of four LEDs
- 6 transistor base resistors (470 Ω, 1/4 W)
- 6 transistor pull-down resistors (100 kΩ, 1/4 W)
- Alps Alpine EC11E18244A5 rotary encoder with push button
- 3 encoder pull-up resistors (10 kΩ, 1/4 W)
- 100 nF ceramic capacitor (at least 25 V)
- 2 seven-pin female headers (2.54 mm pitch) for the XIAO
- 2 matching male headers, if the XIAO doesn't already have them
- USB cable for power and programming through the XIAO's USB-C port

[Design BOM with KiCad symbols and footprints](bom.csv). Exact resistor, capacitor and header parts, supplier prices and final mechanical checks are still pending.

## LED placement

The LEDs sit on a 38 mm radius circle, with a gap at the top for the XIAO and USB connector. I kept the [coordinate calculations here](docs/led-placement.md) so this page stays readable.

## Design files

Open [the KiCad project](hardware/Lichtreaktor/Lichtreaktor.kicad_pro) to see the schematic and PCB. The project includes its XIAO symbol and footprint libraries; their source and adaptations are documented [here](hardware/Lichtreaktor/libraries/SOURCES.md).

## A closer look

### PCB layout

![Routed PCB layout with GND fills in KiCad](assets/kicad/pcb-layout.png)

### Front and back

![Front of the PCB in KiCad's 3D viewer](assets/kicad/pcb-3d-front.png)

![Back of the PCB with the resistors in KiCad's 3D viewer](assets/kicad/pcb-3d-back.png)

### Schematic

![Complete Lichtreaktor schematic with six LED groups and the XIAO RP2040](assets/kicad/schematic.png)
