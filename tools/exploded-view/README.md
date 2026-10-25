# 3D exploded view

An interactive 3D exploded view of the car and the electronics box, built with React Three Fiber and `@react-three/drei`. Every part comes from the project's own records, and every mesh is made in code: there are no model or image files.

## What it reads

[`scripts/build_data.py`](scripts/build_data.py) turns these into [`src/data/project.json`](src/data/project.json):

| Source | Gives |
| --- | --- |
| `docs/hardware/kicad/BOM.csv` | every part: description, manufacturer / part number, status, BOM line, notes |
| `docs/hardware/kicad/WIRE_LIST.csv` | every wire: From (Point A, upstream), To (Point B, downstream), net, AWG, colour, fuse, route |
| `docs/hardware/kicad/CONNECTORS.md` | the box-edge plugs J1-J12 and which are TBD |
| `docs/hardware/PINOUT.md` | ESP32 header pin to GPIO and signal, shown on U20's wires |
| `docs/hardware/CAR.md` | car size and model |
| `docs/hardware/kicad/gen/design.py` | pins and nets of every part (for the soldered parts with no wire row), and the variant B drive parts (#32) |

Where each part sits and what shape it gets is hand-placed in [`src/data/layout.js`](src/data/layout.js). A BOM ref with no layout entry still appears, on a "Not placed yet" tray, and `npm test` fails until it is placed. Parts that live on the A1 board are laid out on it automatically.

**After the BOM, the wire list or the pin map changes**, regenerate and commit the JSON:

```sh
cd tools/exploded-view
npm run data                          # from the working tree
python3 scripts/build_data.py --git-ref origin/some-branch   # or from another branch
```

CI runs `python3 scripts/build_data.py --check` and fails if the committed JSON is stale.

## What it does

- **Visual styles:** `realistic` (PBR: molded matte plastic body, brushed and chrome cage brackets, polished steel and copper motors and gears, studio light panels, soft shadows) and `blueprint` (edge line-art on an engineering grid with a title block).
- **Exploded layout:** each part has an origin and an explode vector (its assembly's plus its own). Inside the box the parts also stack by tier; tiers are ranked among the visible parts, so hiding one closes its gap. Faint dashed lines trace each part's disassembly path.
- **Show and hide:** a component tree by assembly, an eye per assembly and a checkbox per part. Hidden parts leave the canvas at once, and so do their wires.
- **Info card:** click a part for Model (manufacturer / part number), Name, Type and Usage from the BOM; status, BOM line and TBD flags with issue links; then "Connected from" (Point A, upstream) and "Connected to" (Point B, downstream) from the wire list, with gauge, colour, fuse and route. ESP32 wires show the GPIO. Soldered parts with no wire row show their nets instead.
- **Wiring:** all 230 wires routed live between the parts, coloured by the wire colour code.
- **Variant swap:** variant A (4 + 1 IBT-2) or variant B (2 x MDD20A, PWM gate, current sensors). Variant B wiring is derived from the nets in design.py and marked as such.
- **Compact packaging:** the third option keeps variant B's drive but packs it into a main box of about 18 x 12 x 6 cm (against about 25 x 20 x 10 cm), moves the contactor K1, D1, TB2, R7 and D2 next to the battery, and puts the extras driving doesn't need (12 V buck U4, music amp U32, MOSFET board U29, F9, F10, J12) in a small accessory box behind the seats. The overrides are `COMPACT` in `layout.js`. The sizes are estimates from the module sizes, not a fit check; that waits on #18.
- **Move:** select a part and drag it with the gizmo; the offset is kept as a revision.
- **Exports:** `.glb` and `.gltf` (visible meshes, hierarchy and current explode positions, part metadata in `extras`), `.png` at 1-4x, a `.pdf` data sheet (snapshot, state, visible parts, connection routing), and `.json` (the procedural parts data, wiring, state and history).
- **Revision history:** every show/hide, explode, style, variant, wiring and move change is a timestamped revision. Click one to jump to it; Ctrl+Z and Ctrl+Shift+Z step back and forward. Kept in this browser's local storage.

## What is not real yet

Car-side positions are illustrative, from the listing size (147 x 86 x 88 cm). The box size waits on the under-seat measurements (#18), the car-facing plugs J1-J8 on the stock plug photos (#17), the battery on #13, the motor labels on #14 and the pedal on #16. Those parts carry an orange TBD marker and hazard stripes, and the PDF says so.

## Run it

```sh
cd tools/exploded-view
npm ci
npm run dev            # http://localhost:5173
npm test               # model checks (every BOM ref drawn, every wire joined, variant B)
npm run build          # static site in dist/
npm run build:single   # one self-contained dist-single/index.html
```

## Code map

| File | What |
| --- | --- |
| `src/geometry/builders.js` | a procedural builder per part kind (motor, IBT-2, contactor, gear, fan, Deutsch plug...), returning plain primitive data |
| `src/three/primitives.js`, `materials.js` | primitive data to cached three.js geometry; materials for both styles and the canvas-drawn textures |
| `src/data/model.js` | project.json + layout.js to the part and connection lists |
| `src/data/explode.js` | explode vectors and tier ranking |
| `src/components/PowerWheelPart.jsx` | one part: origin, direction, `explodeFactor`, visibility, pointer and click handling |
| `src/components/Wiring.jsx` | live wire routing and the disassembly paths |
| `src/export/exporters.js` | GLB/glTF, PNG, PDF and JSON export |
