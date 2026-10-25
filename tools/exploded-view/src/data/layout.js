// Where each part sits and what shape it gets. Hand-placed; everything else (names, status,
// notes, wiring) comes from project.json, which scripts/build_data.py makes from the BOM and the
// wire list. A BOM ref with no entry here still appears, on the "Not placed yet" tray.
//
// Units are metres. x points to the front of the car, y up, z to the right (left side is -z).
// Car-side positions are illustrative: they follow the listing size (147 x 86 x 88 cm) and the
// usual ride-on layout, not measurements. The box size waits on #18 and the box-edge plugs J1-J8
// on the stock plug photos (#17); both are drawn as TBD.

export const BOX = { center: [-0.08, 0.205, 0], size: [0.25, 0.1, 0.2], wall: 0.003, tbd: 'Size waits on the under-seat measurements (#18)' };
const FLOOR = BOX.center[1] - BOX.size[1] / 2 + BOX.wall;
const inBox = (x, y, z) => [BOX.center[0] + x, FLOOR + y, BOX.center[2] + z];

// Exploded layout: an assembly's explode vector applies to every part in it (and its children).
// Inside the box, parts also stack by tier: when exploded, each visible tier lifts one TIER_GAP
// above the last visible one, so hiding a tier closes the gap it leaves.
export const TIER_GAP = 0.13;

export const ASSEMBLIES = [
  { id: 'body', name: 'Car body (stock)', explode: [0, 0, 0] },
  { id: 'wheels', name: 'Wheels and drive motors (stock)', explode: [0, -0.05, 0] },
  { id: 'steering', name: 'Steering', explode: [0.35, 0.05, 0] },
  { id: 'power', name: 'Battery and main fuse', explode: [0.25, 0.45, 0] },
  { id: 'box', name: 'Electronics box (under the seat)', explode: [0, 0.25, 1.05], tiers: true },
  { id: 'box.shell', parent: 'box', name: 'Box shell (size TBD #18)' },
  { id: 'box.plugs', parent: 'box', name: 'Box-edge plugs J1-J12' },
  { id: 'box.cooling', parent: 'box', name: 'Cooling fans' },
  { id: 'box.power', parent: 'box', name: 'Contactor and power studs' },
  { id: 'box.drive', parent: 'box', name: 'Motor drivers' },
  { id: 'box.rails', parent: 'box', name: 'Buck converters' },
  { id: 'box.fuses', parent: 'box', name: 'Fuses' },
  { id: 'box.logic', parent: 'box', name: 'Interface board A1' },
  { id: 'box.controller', parent: 'box', name: 'ESP32 and receiver' },
  { id: 'box.io', parent: 'box', name: 'I2C modules and outputs' },
  { id: 'box.sound', parent: 'box', name: 'Sound modules' },
  { id: 'dash', name: 'Dash and controls', explode: [0.3, 0.55, 0] },
  { id: 'cabin', name: 'Pedal, shifter and seat', explode: [0.1, 0.25, -0.35] },
  { id: 'sensors', name: 'Wheel and steering sensors', explode: [0, 0.1, 0] },
  { id: 'audio', name: 'Music speakers', explode: [-0.25, 0.35, 0] },
  { id: 'lights', name: 'Lights (stock)', explode: [0.35, 0.1, 0] },
  { id: 'unplaced', name: 'Not placed yet', explode: [0, 0, 0] },
];

// Fuse colours by rating (blade fuse colour code).
const FUSE = { 1: 'fuseBlack', 2: 'fuseGrey', 3: 'fuseViolet', 10: 'fuseRed', 15: 'fuseBlue' };

const L = {};
const put = (id, o) => { L[id] = o; };

// ---- stock body (not in the BOM: no part to buy, drawn so the electronics have context) ----
put('body.tub', { kind: 'tub', assembly: 'body', pos: [0, 0, 0], name: 'Chassis tub', type: 'Stock structure', usage: 'Holds everything; the electronics box sits on its floor under the seats.' });
put('body.shell', { kind: 'bodyShell', assembly: 'body', pos: [0, 0, 0], explode: [0, 0.75, 0], name: 'Body shell: hood, fenders, side panels', type: 'Stock molded plastic', usage: 'Outer housing. Hide it to see the battery, the motors and the box.' });
put('body.cage', { kind: 'rollCage', assembly: 'body', pos: [0, 0, 0], explode: [0, 1.05, 0], name: 'Roll cage (open, no roof)', type: 'Stock black tube frame', usage: 'Open tube cage with brackets to the tub; the stock car has no roof panel.' });
put('body.seatL', { kind: 'seat', assembly: 'body', pos: [-0.12, 0.36, -0.15], explode: [0, 0.55, -0.12], name: 'Driver seat (left)', type: 'Stock seat', usage: 'Sits over the electronics box. The seat switch SW5 goes under its cushion.' });
put('body.seatR', { kind: 'seat', assembly: 'body', pos: [-0.12, 0.36, 0.15], explode: [0, 0.55, 0.12], name: 'Passenger seat (right)', type: 'Stock seat', usage: 'Sits over the electronics box.' });
put('body.dash', { kind: 'dashPanel', assembly: 'dash', pos: [0.24, 0.5, 0], name: 'Dash panel', type: 'Stock molded plastic', usage: 'Carries the key, the E-stop, the OLED and the alert speaker.' });

// ---- wheels and motors ----
const WHEELS = { FL: [0.47, -1], FR: [0.47, 1], RL: [-0.47, -1], RR: [-0.47, 1] };
const MOTOR = { FL: 'M1', FR: 'M2', RL: 'M3', RR: 'M4' };
const CAP = { FL: 'C201', FR: 'C202', RL: 'C203', RR: 'C204' };
for (const [w, [x, s]] of Object.entries(WHEELS)) {
  put(`body.wheel${w}`, { kind: 'wheel', params: { side: s }, assembly: 'wheels', pos: [x, 0.16, 0.37 * s], explode: [0, 0, 0.42 * s], name: `Wheel, ${w}`, type: 'Stock wheel', usage: 'Driven by its own motor and gearbox.' });
  put(MOTOR[w], { kind: 'driveMotor', params: { side: s }, assembly: 'wheels', pos: [x, 0.16, 0.27 * s], explode: [0, 0, 0.2 * s] });
  put(CAP[w], { kind: 'ceramicCap', assembly: 'wheels', pos: [x + 0.02, 0.19, 0.17 * s], explode: [0, 0.03, 0.12 * s] });
}

// ---- steering ----
put('M5', { kind: 'steeringMotor', assembly: 'steering', pos: [0.5, 0.22, 0.04] });
put('C205', { kind: 'ceramicCap', assembly: 'steering', pos: [0.5, 0.25, -0.07], explode: [0, 0.04, -0.04] });
put('U25', { kind: 'module', params: { w: 0.023, d: 0.023, pcb: 'pcbPurple', header: 7 }, assembly: 'steering', pos: [0.5, 0.29, 0.04], explode: [0, 0.12, 0] });
put('body.shocks', { kind: 'shocks', assembly: 'wheels', pos: [0, 0, 0], explode: [0, 0.2, 0], name: 'Coil-over shocks (red)', type: 'Stock suspension', usage: 'Four red coil-overs between the A-arms and the body.' });
put('body.steer', { kind: 'steeringWheel', assembly: 'steering', pos: [0.12, 0.62, -0.15], explode: [-0.1, 0.25, 0], name: 'Steering wheel and column', type: 'Stock', usage: 'The child steers by hand in Kid mode; the steering motor M5 turns it under radio control.' });

// ---- battery and main fuse ----
put('BT1', { kind: 'battery', assembly: 'power', pos: [0.3, 0.14, 0], params: { w: 0.18, h: 0.12, d: 0.1 } });
put('F1', { kind: 'anlFuse', assembly: 'power', pos: [0.3, 0.28, 0.12], explode: [0, 0.12, 0.12] });

// ---- electronics box ----
put('BOX', { kind: 'boxTray', params: { w: BOX.size[0], h: BOX.size[1], d: BOX.size[2] }, assembly: 'box.shell', pos: BOX.center, tier: -1, name: 'Electronics box: flame-retardant (UL94 V-0) ABS or metal, IP54', type: 'Enclosure', match: /^box:/i, tbd: [BOX.tbd], badge: false });
put('BOX.lid', { kind: 'boxLid', params: { w: BOX.size[0], d: BOX.size[2] }, assembly: 'box.shell', pos: [BOX.center[0], BOX.center[1] + BOX.size[1] / 2 + 0.002, BOX.center[2]], tier: 'top', name: 'Electronics box lid', type: 'Enclosure', usage: 'Gasketed lid (IP54).', tbd: [BOX.tbd], badge: 'Box size · #18' });

// Plugs J1-J8 face the stock harness on the front wall, J9-J12 are ours on the back wall.
['J1', 'J2', 'J3', 'J4', 'J5', 'J6', 'J7', 'J8'].forEach((j, i) => {
  put(j, { kind: 'deutsch', params: { ways: 4, tbd: true }, assembly: 'box.plugs', pos: inBox(BOX.size[0] / 2, 0.03 + (i % 2) * 0.04, -0.075 + Math.floor(i / 2) * 0.05), tier: -1, explode: [0.14, 0, 0],
    badge: i === 0 ? 'J1-J8 · #17' : false });
});
['J9', 'J10', 'J11', 'J12'].forEach((j, i) => {
  put(j, { kind: 'deutsch', params: { ways: [4, 8, 6, 4][i] }, assembly: 'box.plugs', pos: inBox(-BOX.size[0] / 2, 0.045, -0.07 + i * 0.047), rot: [0, Math.PI, 0], tier: -1, explode: [-0.14, 0, 0] });
});
put('FAN1', { kind: 'fan', assembly: 'box.cooling', pos: inBox(0.05, 0.048, -BOX.size[2] / 2 - 0.013), tier: -1, explode: [0, 0, -0.16] });
put('FAN2', { kind: 'fan', assembly: 'box.cooling', pos: inBox(-0.05, 0.048, BOX.size[2] / 2 + 0.013), tier: -1, explode: [0, 0, 0.16] });

// tier 0: contactor and studs on the floor
put('K1', { kind: 'contactor', assembly: 'box.power', pos: inBox(-0.09, 0, -0.06), tier: 0 });
put('D1', { kind: 'diode', params: { big: true }, assembly: 'box.power', pos: inBox(-0.09, 0.09, -0.035), tier: 0 });
put('TB1', { kind: 'busbar', params: { n: 7, m: 'brass' }, assembly: 'box.power', pos: inBox(-0.01, 0, -0.035), tier: 0 });
put('TB2', { kind: 'busbar', params: { n: 6, m: 'copper' }, assembly: 'box.power', pos: inBox(0.07, 0, -0.035), tier: 0 });
put('R5', { kind: 'resistor', assembly: 'box.power', pos: inBox(0.07, 0, -0.06), tier: 0 });
put('R7', { kind: 'resistor', params: { watts: 3 }, assembly: 'box.power', pos: inBox(0.07, 0, -0.08), tier: 0 });
put('D2', { kind: 'tvs', assembly: 'box.power', pos: inBox(0.03, 0, -0.07), tier: 0 });

// tier 1: motor drivers. Variant A: one IBT-2 per wheel + steering. Variant B: 2 x MDD20A (#32).
['U10', 'U11', 'U12', 'U13', 'U14'].forEach((u, i) => {
  const x = -0.1 + i * 0.05;
  put(u, { kind: 'ibt2', assembly: 'box.drive', pos: inBox(x, 0.004, 0.045), tier: 1 });
  put(`C11${i + 1}`, { kind: 'electrolytic', assembly: 'box.drive', pos: inBox(x - 0.01, 0.004, 0.085), tier: 1 });
  put(`R11${i + 1}`, { kind: 'resistor', assembly: 'box.drive', pos: inBox(x + 0.01, 0.004, 0.085), tier: 1 });
});
put('U41', { kind: 'mdd20a', assembly: 'box.drive', pos: inBox(-0.075, 0.004, 0.04), tier: 1 });
put('U42', { kind: 'mdd20a', assembly: 'box.drive', pos: inBox(0.02, 0.004, 0.04), tier: 1 });
put('C41', { kind: 'electrolytic', assembly: 'box.drive', pos: inBox(-0.075, 0.004, 0.085), tier: 1 });
put('C42', { kind: 'electrolytic', assembly: 'box.drive', pos: inBox(0.02, 0.004, 0.085), tier: 1 });
['U43', 'U44', 'U45', 'U46'].forEach((u, i) => {
  put(u, { kind: 'module', params: { w: 0.02, d: 0.014, pcb: 'pcbGreen', header: 5, chips: [[0.005, 0.004]] }, assembly: 'box.drive', pos: inBox(-0.1 + i * 0.03, 0.004, -0.005), tier: 1, tbd: ['Current sensor part TBD (#32)'] });
});

// tier 2: buck converters and fuses
put('U1', { kind: 'buck', assembly: 'box.rails', pos: inBox(-0.08, 0.004, -0.075), tier: 2 });
put('U3', { kind: 'buck', params: { w: 0.05, d: 0.026, big: true }, assembly: 'box.rails', pos: inBox(-0.025, 0.004, -0.075), tier: 2 });
put('U4', { kind: 'sealedBuck', assembly: 'box.rails', pos: inBox(0.06, 0.004, -0.065), tier: 2 });
put('FB1', { kind: 'fuseBlock', assembly: 'box.fuses', pos: inBox(0.0, 0.004, 0.04), tier: 2 });
const FB_SLOTS = { F3: 1, F4: 2, F5: 3, F6: 10, F7: 1, F8: 1 };
Object.entries(FB_SLOTS).forEach(([f, amps], i) => put(f, { kind: 'bladeFuse', params: { m: FUSE[amps] }, assembly: 'box.fuses', pos: inBox(-0.033 + i * 0.0132, 0.012, 0.036), tier: 2 }));
put('F2', { kind: 'inlineFuse', params: { m: FUSE[15] }, assembly: 'box.fuses', pos: inBox(-0.075, 0.004, 0.035), tier: 2 });
put('F9', { kind: 'inlineFuse', params: { m: FUSE[3] }, assembly: 'box.fuses', pos: inBox(0.075, 0.004, 0.025), tier: 2 });
put('F10', { kind: 'inlineFuse', params: { m: FUSE[10] }, assembly: 'box.fuses', pos: inBox(0.075, 0.004, 0.06), tier: 2 });

// tier 3: interface board A1 and what is soldered on it (passives laid out in a grid at build time)
export const A1 = { pos: inBox(-0.06, 0.012, 0.0), size: [0.09, 0.07] };
put('A1', { kind: 'protoBoard', params: { w: A1.size[0], d: A1.size[1] }, assembly: 'box.logic', pos: A1.pos, tier: 3 });
put('Q1', { kind: 'to220', assembly: 'box.logic', pos: [A1.pos[0] + 0.035, A1.pos[1] + 0.0016, A1.pos[2] + 0.02], tier: 3 });
put('U2', { kind: 'module', params: { w: 0.025, d: 0.012, pcb: 'pcbBlue', header: 3 }, assembly: 'box.logic', pos: inBox(0.075, 0.012, -0.07), tier: 3 });
put('U40', { kind: 'module', params: { w: 0.012, d: 0.009, pcb: 'blackPlastic', header: 0 }, assembly: 'box.logic', pos: [A1.pos[0] + 0.035, A1.pos[1] + 0.0016, A1.pos[2] - 0.005], tier: 3 });
// these A1 parts get a grid slot on the board; any other part whose BOM "Where" is A1 joins them
export const ON_A1 = 'board';

// tier 4: controller
put('U20', { kind: 'esp32', assembly: 'box.controller', pos: inBox(0.035, 0.024, 0.0), tier: 4 });
put('U21', { kind: 'receiver', assembly: 'box.controller', pos: inBox(0.095, 0.024, 0.02), tier: 4 });

// tier 5: I2C modules, outputs and sound
const io = [
  ['U22', { w: 0.028, d: 0.018, pcb: 'pcbBlue', header: 10 }],
  ['U23', { w: 0.028, d: 0.018, pcb: 'pcbBlue', header: 10 }],
  ['U24', { w: 0.04, d: 0.016, pcb: 'pcbBlue', header: 4, chips: [[0.01, 0.006]] }],
  ['U26', { w: 0.021, d: 0.016, pcb: 'pcbBlue', header: 8 }],
  ['U28', { w: 0.062, d: 0.025, pcb: 'pcbBlue', header: 6, chips: [[0.008, 0.008]] }],
  ['U29', { w: 0.05, d: 0.035, pcb: 'pcbRed', header: 6, chips: [[0.006, 0.006], [0.006, 0.006], [0.006, 0.006], [0.006, 0.006]] }],
];
io.forEach(([u, p], i) => put(u, { kind: 'module', params: p, assembly: 'box.io', pos: inBox(-0.085 + (i % 3) * 0.07, 0.06, -0.06 + Math.floor(i / 3) * 0.06), tier: 5 }));
put('U31', { kind: 'probe', assembly: 'box.io', pos: inBox(0.07, 0.06, 0.07), tier: 5 });
put('U30', { kind: 'module', params: { w: 0.021, d: 0.021, pcb: 'pcbBlack', header: 8, chips: [[0.008, 0.008]] }, assembly: 'box.sound', pos: inBox(-0.085, 0.06, 0.07), tier: 5 });
put('U32', { kind: 'module', params: { w: 0.06, d: 0.04, pcb: 'pcbGreen', header: 0, chips: [[0.01, 0.01], [0.012, 0.012]], extra: [] }, assembly: 'box.sound', pos: inBox(-0.015, 0.06, 0.07), tier: 5 });

// ---- dash ----
put('S2', { kind: 'estop', assembly: 'dash', pos: [0.22, 0.6, 0.0], explode: [0, 0.1, 0] });
put('SW1', { kind: 'keySwitch', assembly: 'dash', pos: [0.22, 0.59, 0.14], explode: [0, 0.08, 0.06] });
put('U27', { kind: 'oled', assembly: 'dash', pos: [0.22, 0.6, -0.06], explode: [0, 0.08, -0.03] });
put('LS1', { kind: 'speaker', assembly: 'dash', pos: [0.24, 0.58, 0.24], explode: [0, 0.1, 0.08] });
put('SW4', { kind: 'button', params: {}, assembly: 'dash', pos: [0.12, 0.64, -0.15], rot: [0, 0, 1.2], explode: [-0.05, 0.12, 0] });

// ---- cabin inputs ----
put('PED1', { kind: 'pedal', assembly: 'cabin', pos: [0.4, 0.14, -0.15] });
put('SW2', { kind: 'shifter', assembly: 'cabin', pos: [0.05, 0.36, 0.0], explode: [0, 0.15, 0.35] });
put('SW3', { kind: 'microSwitch', assembly: 'cabin', pos: [0.05, 0.38, 0.04], explode: [0, 0.15, 0.4] });
put('SW5', { kind: 'seatSwitch', assembly: 'cabin', pos: [-0.12, 0.35, -0.15], explode: [0, 0.3, 0] });

// ---- sensors on the rear wheels ----
put('H1', { kind: 'hall', assembly: 'sensors', pos: [-0.47, 0.23, -0.31], explode: [0, 0.05, -0.2] });
put('H2', { kind: 'hall', assembly: 'sensors', pos: [-0.47, 0.23, 0.31], explode: [0, 0.05, 0.2] });

// ---- audio and lights ----
put('LS2', { kind: 'speaker', assembly: 'audio', pos: [-0.45, 0.45, -0.25], explode: [0, 0, -0.25] });
put('LS3', { kind: 'speaker', assembly: 'audio', pos: [-0.45, 0.45, 0.25], explode: [0, 0, 0.25] });
put('LP1', { kind: 'headlights', assembly: 'lights', pos: [0.72, 0.31, 0], name: 'Stock lights (headlights)', type: 'Stock lamp', usage: 'Switched low-side by U29 OUT1 through J8.', tbd: ['Lamp type and pin use wait on the stock plug photos (#17)'] });

export const LAYOUT = L;

// Variant B (2 x MDD20A, #32) replaces the four drive IBT-2s and their current-sense networks.
// Steering keeps U14 and its parts. Source: docs/hardware/kicad/variant_b and gen/design.py.
export const VARIANT_A_ONLY = ['U10', 'U11', 'U12', 'U13', 'R101', 'R102', 'R103', 'R104', 'C101', 'C102', 'C103', 'C104',
  'D101', 'D102', 'D103', 'D104', 'C111', 'C112', 'C113', 'C114', 'R111', 'R112', 'R113', 'R114'];

// Net anchors for variant B wiring: where a rail is picked up.
export const RAIL_ANCHOR = { GND: { ref: 'TB1', pin: 'stud' }, MOTOR_24V: { ref: 'TB2', pin: 'stud' }, '3V3_SENSE': { ref: 'A1', pin: '3V3' } };

// Part type by kind, for the info card.
export const TYPE_BY_KIND = {
  wheel: 'Wheel', driveMotor: 'Brushed DC drive motor + gearbox', steeringMotor: 'Brushed DC steering motor', battery: 'Traction battery',
  contactor: 'Main contactor', anlFuse: 'Main fuse', inlineFuse: 'Inline fuse', bladeFuse: 'Blade fuse', fuseBlock: 'Fuse block',
  busbar: 'Bus bar / stud', ibt2: 'H-bridge motor driver', mdd20a: 'Dual motor driver', esp32: 'Microcontroller', receiver: 'Radio receiver',
  buck: 'DC-DC buck converter', sealedBuck: 'DC-DC buck converter (sealed)', fan: 'Cooling fan', deutsch: 'Connector', estop: 'Emergency stop',
  keySwitch: 'Key switch', button: 'Push button', oled: 'Display', speaker: 'Speaker', hall: 'Hall sensor', pedal: 'Throttle pedal',
  shifter: 'Gear shifter', microSwitch: 'Switch contact', seatSwitch: 'Seat switch', probe: 'Temperature probe', headlights: 'Lamps',
  resistor: 'Resistor', ceramicCap: 'Capacitor', electrolytic: 'Electrolytic capacitor', diode: 'Diode', to220: 'MOSFET', tvs: 'TVS diode',
  protoBoard: 'Interface board', boxTray: 'Enclosure', boxLid: 'Enclosure', module: 'Module',
};

// Kind for a BOM ref with no layout entry, from its designator letter.
export const KIND_BY_PREFIX = { R: 'resistor', C: 'ceramicCap', D: 'diode', Q: 'to220', F: 'bladeFuse', U: 'module', J: 'deutsch' };
