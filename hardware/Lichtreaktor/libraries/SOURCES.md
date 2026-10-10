# Seeed XIAO RP2040 library

Source: [Seeed-Studio/OPL_Kicad_Library](https://github.com/Seeed-Studio/OPL_Kicad_Library/tree/b0035c51eb0348bb3e165fdb2f2765fa3d1d17bd/Seeed%20Studio%20XIAO%20Series%20Library)

Revision: `b0035c51eb0348bb3e165fdb2f2765fa3d1d17bd`, downloaded 2026-10-10.
Author: Seeed Studio and upstream contributors.
License: CC BY-SA 4.0; see `Seeed-LICENSE`.

Unmodified originals are retained in `upstream/`.

Project adaptations:

- The upstream `XIAO-RP2040-SMD` symbol has 20 pins. The project uses the original 14 side pins only, for socket mounting. Pins 15-20 (underside connections) were removed in the derived symbol, named `XIAO-RP2040-DIP`.
- The derived symbol points to `Seeed_XIAO:XIAO-RP2040-DIP`.
- The official DIP footprint retains its original pad geometry. References to unavailable `${AMZPATH}` 3D models were removed. No module 3D model is bundled yet.
- Symbol and footprint tables use `${KIPRJMOD}` relative paths.

Verified: pin numbers 1-14 match between symbol and footprint. Through-hole rows have 2.54 mm pin pitch and 15.24 mm separation. Final socket dimensions, module height and USB clearance still need checking before fabrication.
