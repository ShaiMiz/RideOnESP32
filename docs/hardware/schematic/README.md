# Schematic

**Superseded by the KiCad schematic v0.3 in [../kicad/](../kicad/)**, which adds net names, connector and pin tables, a wire list and a BOM matched to the 24 Oct order. This SVG draft stays for reference.

Draft v0.2 of the wiring diagram. Start with [SCHEMATIC.md](SCHEMATIC.md): it lists the sheets, every reference designator, the open TBDs and where the draft deliberately differs from [WIRING.md](../WIRING.md) and [POWER_CHAIN.md](../POWER_CHAIN.md) ([#31](https://github.com/ShaiMiz/RideOnESP32/issues/31) tracks reconciling them).

The SVGs are generated: edit [generate.py](generate.py), then run `python3 generate.py .` and `./render.sh .` (needs Chromium and Pillow) for the PNGs. Don't edit the SVGs by hand.
