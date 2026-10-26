// Procedural shapes for every part kind. A builder returns plain data: a list of primitives
// { t: type, a: args, p: position, r: rotation (radians), m: material key }, in metres, relative
// to the part's origin. Nothing here touches three.js, so the same list is what the JSON export
// carries and what <PowerWheelPart> turns into meshes (see primitives.js).

const PI = Math.PI;
const H = PI / 2;

const box = (w, h, d, p = [0, 0, 0], m = 'blackPlastic', r) => ({ t: 'box', a: [w, h, d], p, m, ...(r && { r }) });
const cyl = (rt, rb, h, p = [0, 0, 0], m = 'steel', r, seg = 24) => ({ t: 'cylinder', a: [rt, rb, h, seg], p, m, ...(r && { r }) });
const sph = (rad, p, m, a = []) => ({ t: 'sphere', a: [rad, 20, 14, ...a], p, m });
const tor = (rad, tube, p, m, r, arc = 2 * PI) => ({ t: 'torus', a: [rad, tube, 12, 32, arc], p, m, ...(r && { r }) });
const tube = (pts, radius, m = 'brushed', closed = false) => ({ t: 'tube', pts, a: [radius], closed, m });
const lathe = (pts, p, m, r) => ({ t: 'lathe', pts, p, m, ...(r && { r }) });
const extrude = (shape, depth, p, m, r) => ({ t: 'extrude', shape, a: [depth], p, m, ...(r && { r }) });
const gear = (teeth, ro, rr, depth, bore, p, m, r) => ({ t: 'gear', a: [teeth, ro, rr, depth, bore], p, m, ...(r && { r }) });

// Header pins in a row along x, on top of a board of thickness t.
function pins(n, pitch, p, m = 'gold') {
  const out = [];
  const x0 = p[0] - ((n - 1) * pitch) / 2;
  for (let i = 0; i < n; i++) out.push(box(0.0008, 0.008, 0.0008, [x0 + i * pitch, p[1], p[2]], m));
  return [box(n * pitch, 0.0025, 0.0025, [p[0], p[1] - 0.003, p[2]], 'blackPlastic'), ...out];
}

// Screw-terminal block, n ways, along x.
function terminals(n, p, m = 'blueTerminal') {
  const w = n * 0.005;
  const out = [box(w, 0.01, 0.008, p, m)];
  for (let i = 0; i < n; i++) out.push(cyl(0.0014, 0.0014, 0.002, [p[0] - w / 2 + 0.0025 + i * 0.005, p[1] + 0.0055, p[2]], 'steel'));
  return out;
}

function chip(w, d, p, m = 'blackPlastic') {
  return box(w, 0.0016, d, p, m);
}

// Generic breakout module: a board, a few chips, a header.
function module({ w = 0.03, d = 0.02, pcb = 'pcbBlue', chips = [[0.006, 0.006]], header = 6, extra = [] }) {
  const t = 0.0016;
  const out = [box(w, t, d, [0, t / 2, 0], pcb)];
  chips.forEach(([cw, cd, cx = 0, cz = 0], i) => out.push(chip(cw, cd, [cx || (i - (chips.length - 1) / 2) * (w / (chips.length + 1)) * 1.4, t + 0.0008, cz])));
  if (header) out.push(...pins(header, 0.00254, [0, t + 0.004, d / 2 - 0.002]));
  return out.concat(extra);
}

function heatsink(w, d, h, fins, p, m = 'aluminiumDark') {
  const out = [box(w, 0.003, d, [p[0], p[1] + 0.0015, p[2]], m)];
  const pitch = w / fins;
  for (let i = 0; i < fins; i++) out.push(box(0.0012, h, d, [p[0] - w / 2 + pitch * (i + 0.5), p[1] + 0.003 + h / 2, p[2]], m));
  return out;
}

// Knobby tyre on a red beadlock rim, axle along z.
function wheel({ r = 0.17, w = 0.13, side = 1 }) {
  const out = [
    cyl(r - 0.012, r - 0.012, w, [0, 0, 0], 'rubber', [H, 0, 0], 40),
    tor(r - 0.02, 0.018, [0, 0, w / 2 - 0.016], 'rubber'),
    tor(r - 0.02, 0.018, [0, 0, -w / 2 + 0.016], 'rubber'),
    cyl(r * 0.6, r * 0.6, w * 0.92, [0, 0, 0], 'rimBlack', [H, 0, 0], 36),
    tor(r * 0.6, 0.012, [0, 0, (w / 2) * side * 0.95], 'rimRed'), // beadlock ring
    cyl(r * 0.16, r * 0.16, w * 0.96, [0, 0, 0.004 * side], 'rimBlack', [H, 0, 0]),
    cyl(r * 0.06, r * 0.06, w, [0, 0, 0.008 * side], 'chrome', [H, 0, 0]),
  ];
  for (let i = 0; i < 6; i++) {
    const a = (i / 6) * 2 * PI;
    out.push(box(r * 0.42, 0.022, 0.01, [Math.cos(a) * r * 0.36, Math.sin(a) * r * 0.36, (w / 2) * side * 0.94], 'rimBlack', [0, 0, a]));
    out.push(cyl(0.004, 0.004, 0.006, [Math.cos(a + 0.5) * r * 0.6, Math.sin(a + 0.5) * r * 0.6, (w / 2) * side * 0.98], 'steel', [H, 0, 0], 8));
  }
  // staggered tread knobs, two rows, plus shoulder lugs
  for (let i = 0; i < 22; i++) {
    const a = (i / 22) * 2 * PI;
    const off = i % 2 ? 0.025 : -0.025;
    out.push(box(0.03, 0.016, w * 0.42, [Math.cos(a) * (r - 0.004), Math.sin(a) * (r - 0.004), off], 'rubber', [0, 0, a + H]));
    out.push(box(0.022, 0.014, 0.02, [Math.cos(a) * (r - 0.012), Math.sin(a) * (r - 0.012), (i % 2 ? 1 : -1) * (w / 2 + 0.002)], 'rubber', [0, 0, a + H]));
  }
  return out;
}

// Stock drive motor + gearbox at a wheel. Gearbox face towards the wheel (side = -1 left, +1 right).
function driveMotor({ side = 1 }) {
  const s = side;
  return [
    box(0.1, 0.1, 0.045, [0, 0, 0], 'blackPlastic'),
    gear(18, 0.032, 0.027, 0.006, 0.006, [0, 0.0, 0.026 * s], 'brass'),
    gear(10, 0.016, 0.012, 0.007, 0.003, [0.034, 0.02, 0.027 * s], 'copper'),
    cyl(0.008, 0.008, 0.03, [0, 0, 0.035 * s], 'steel', [H, 0, 0]),
    // motor can sticking out of the gearbox, towards the car centre
    cyl(0.019, 0.019, 0.065, [0.02, 0.02, -0.055 * s], 'steel', [H, 0, 0], 28),
    cyl(0.02, 0.02, 0.012, [0.02, 0.02, -0.092 * s], 'copper', [H, 0, 0], 28),
    box(0.004, 0.008, 0.006, [0.012, 0.03, -0.1 * s], 'brass'),
    box(0.004, 0.008, 0.006, [0.028, 0.03, -0.1 * s], 'brass'),
    cyl(0.004, 0.004, 0.006, [0.02, 0.02, -0.1 * s], 'steel', [H, 0, 0]),
  ];
}

function steeringMotor() {
  return [
    box(0.09, 0.06, 0.07, [0, 0, 0], 'blackPlastic'),
    cyl(0.02, 0.02, 0.07, [0, 0.0, -0.07], 'steel', [H, 0, 0], 28),
    cyl(0.021, 0.021, 0.012, [0, 0, -0.108], 'copper', [H, 0, 0], 28),
    gear(24, 0.028, 0.024, 0.006, 0.008, [0, 0.034, 0], 'brass', [H, 0, 0]),
    cyl(0.007, 0.007, 0.06, [0, 0.05, 0], 'steel'),
    box(0.3, 0.012, 0.012, [0, -0.03, 0.02], 'brushed', [0, H, 0]), // steering rack
  ];
}

function battery({ w = 0.18, h = 0.12, d = 0.1 }) {
  return [
    box(w, h, d, [0, h / 2, 0], 'blackPlastic'),
    box(w * 0.98, 0.012, d * 0.98, [0, h + 0.006, 0], 'greyPlastic'),
    box(w * 0.7, h * 0.5, 0.001, [0, h * 0.5, d / 2 + 0.0006], 'label'),
    cyl(0.008, 0.008, 0.014, [w * 0.35, h + 0.019, 0], 'red'),
    cyl(0.008, 0.008, 0.014, [-w * 0.35, h + 0.019, 0], 'blackPlastic'),
    cyl(0.004, 0.004, 0.006, [w * 0.35, h + 0.029, 0], 'brass'),
    cyl(0.004, 0.004, 0.006, [-w * 0.35, h + 0.029, 0], 'brass'),
  ];
}

function contactor() {
  return [
    cyl(0.027, 0.027, 0.06, [0, 0.03, 0], 'blackPlastic', null, 32),
    cyl(0.03, 0.03, 0.004, [0, 0.002, 0], 'steel', null, 32),
    box(0.075, 0.003, 0.02, [0, 0.0015, 0], 'steel'),
    cyl(0.024, 0.026, 0.02, [0, 0.07, 0], 'blackPlastic', null, 32),
    cyl(0.005, 0.005, 0.02, [0.012, 0.09, 0], 'copper'),
    cyl(0.005, 0.005, 0.02, [-0.012, 0.09, 0], 'copper'),
    cyl(0.009, 0.009, 0.002, [0.012, 0.081, 0], 'brass'),
    cyl(0.009, 0.009, 0.002, [-0.012, 0.081, 0], 'brass'),
    box(0.003, 0.008, 0.006, [0.02, 0.064, 0.016], 'brass'),
    box(0.003, 0.008, 0.006, [-0.02, 0.064, 0.016], 'brass'),
  ];
}

function anlFuse() {
  return [
    box(0.08, 0.012, 0.03, [0, 0.006, 0], 'blackPlastic'),
    box(0.06, 0.002, 0.012, [0, 0.014, 0], 'copper'),
    box(0.02, 0.006, 0.012, [0, 0.016, 0], 'red'),
    cyl(0.004, 0.004, 0.012, [0.026, 0.02, 0], 'brass'),
    cyl(0.004, 0.004, 0.012, [-0.026, 0.02, 0], 'brass'),
    box(0.078, 0.016, 0.028, [0, 0.022, 0], 'glass'),
  ];
}

function bladeFuse(m = 'fuseRed') {
  return [box(0.019, 0.012, 0.005, [0, 0.01, 0], m), box(0.003, 0.004, 0.0008, [0.004, 0.002, 0], 'steel'), box(0.003, 0.004, 0.0008, [-0.004, 0.002, 0], 'steel')];
}

function inlineFuse({ m = 'fuseRed' }) {
  return [
    box(0.035, 0.014, 0.012, [0, 0.007, 0], 'blackPlastic'),
    box(0.022, 0.012, 0.0055, [0, 0.019, 0], m),
    cyl(0.0025, 0.0025, 0.03, [0.03, 0.007, 0], 'red', [0, 0, H]),
    cyl(0.0025, 0.0025, 0.03, [-0.03, 0.007, 0], 'red', [0, 0, H]),
  ];
}

function fuseBlock() {
  const out = [box(0.085, 0.012, 0.045, [0, 0.006, 0], 'blackPlastic'), box(0.08, 0.002, 0.008, [0, 0.013, 0.017], 'copper')];
  for (let i = 0; i < 6; i++) out.push(box(0.011, 0.006, 0.007, [-0.033 + i * 0.0132, 0.015, -0.004], 'greyPlastic'));
  return out;
}

function busbar({ n = 6, m = 'brass' }) {
  const out = [box(0.012 * n + 0.01, 0.006, 0.014, [0, 0.008, 0], m), box(0.012 * n + 0.016, 0.005, 0.018, [0, 0.0025, 0], 'blackPlastic')];
  for (let i = 0; i < n; i++) {
    const x = -0.006 * (n - 1) + i * 0.012;
    out.push(cyl(0.003, 0.003, 0.012, [x, 0.016, 0], 'steel'), cyl(0.005, 0.005, 0.004, [x, 0.014, 0], 'steel', null, 6));
  }
  return out;
}

function ibt2() {
  return [
    box(0.05, 0.0016, 0.05, [0, 0.0008, 0], 'pcbRed'),
    ...heatsink(0.045, 0.045, 0.022, 9, [0, 0.0016, 0]),
    chip(0.009, 0.009, [0.012, -0.001, 0.012]),
    chip(0.009, 0.009, [-0.012, -0.001, 0.012]),
    ...terminals(2, [0.018, 0.006, -0.022]),
    ...terminals(2, [-0.018, 0.006, -0.022]),
    ...pins(8, 0.00254, [0, 0.004, 0.023]),
  ];
}

function mdd20a() {
  return [
    box(0.084, 0.0016, 0.062, [0, 0.0008, 0], 'pcbBlack'),
    ...Array.from({ length: 8 }, (_, i) => box(0.006, 0.009, 0.005, [-0.028 + i * 0.008, 0.006, 0.0], 'blackPlastic')),
    cyl(0.005, 0.005, 0.012, [0.03, 0.007, 0.02], 'blueCap'),
    ...terminals(6, [0, 0.006, -0.026], 'greenTerminal'),
    ...pins(6, 0.00254, [0.0, 0.004, 0.027]),
  ];
}

function esp32() {
  return [
    // MRD068A carrier with screw terminals both sides
    box(0.06, 0.0016, 0.07, [0, 0.0008, 0], 'pcbGreen'),
    ...Array.from({ length: 2 }, (_, s) => box(0.008, 0.01, 0.07, [s ? 0.025 : -0.025, 0.0066, 0], 'blueTerminal')),
    // DevKit on top
    box(0.028, 0.0016, 0.052, [0, 0.016, 0], 'pcbBlack'),
    box(0.018, 0.0028, 0.018, [0, 0.0182, -0.012], 'steel'),
    box(0.018, 0.0008, 0.006, [0, 0.0172, -0.024], 'gold'),
    box(0.009, 0.0032, 0.007, [0, 0.0185, 0.024], 'steel'),
    box(0.0025, 0.012, 0.038, [0.0125, 0.0105, 0.002], 'blackPlastic'),
    box(0.0025, 0.012, 0.038, [-0.0125, 0.0105, 0.002], 'blackPlastic'),
    box(0.003, 0.0012, 0.003, [0.007, 0.0175, 0.015], 'blueLed'),
  ];
}

function receiver() {
  return [
    box(0.018, 0.006, 0.012, [0, 0.003, 0], 'pcbBlack'),
    box(0.016, 0.004, 0.01, [0, 0.008, 0], 'glass'),
    tube([[0, 0.004, 0.006], [0, 0.02, 0.02], [0, 0.06, 0.03], [0, 0.11, 0.03]], 0.0011, 'blackPlastic'),
    cyl(0.0022, 0.0022, 0.03, [0, 0.105, 0.03], 'white'),
  ];
}

function buck({ w = 0.043, d = 0.021, pcb = 'pcbBlue', big = false }) {
  return [
    box(w, 0.0016, d, [0, 0.0008, 0], pcb),
    cyl(big ? 0.008 : 0.006, big ? 0.008 : 0.006, 0.007, [-0.006, 0.0051, 0], 'blackPlastic', null, 18),
    tor(big ? 0.006 : 0.0045, 0.0017, [-0.006, 0.006, 0], 'copper', [H, 0, 0]),
    cyl(0.004, 0.004, 0.01, [0.012, 0.0066, 0.004], 'blueCap'),
    cyl(0.004, 0.004, 0.01, [-0.018, 0.0066, 0.004], 'blueCap'),
    box(0.006, 0.006, 0.004, [0.012, 0.0046, -0.006], 'blueTerminal'),
    chip(0.008, 0.009, [0.003, 0.0024, -0.003]),
  ];
}

function sealedBuck() {
  return [
    box(0.07, 0.03, 0.05, [0, 0.015, 0], 'brushed'),
    ...heatsink(0.06, 0.05, 0.01, 8, [0, 0.03, 0], 'brushed'),
    box(0.084, 0.003, 0.06, [0, 0.0015, 0], 'brushed'),
    cyl(0.003, 0.003, 0.03, [0.045, 0.012, 0.012], 'red', [0, 0, H]),
    cyl(0.003, 0.003, 0.03, [0.045, 0.012, -0.012], 'blackPlastic', [0, 0, H]),
    cyl(0.003, 0.003, 0.03, [-0.045, 0.012, 0.012], 'yellow', [0, 0, H]),
    cyl(0.003, 0.003, 0.03, [-0.045, 0.012, -0.012], 'blackPlastic', [0, 0, H]),
  ];
}

// 80 mm fan facing z; the part's rotation turns it to its wall.
function fan() {
  const blade = [[0, 0], [0.03, 0.006], [0.034, 0.016], [0.006, 0.01]];
  const out = [
    box(0.08, 0.006, 0.025, [0, 0.037, 0], 'blackPlastic'), box(0.08, 0.006, 0.025, [0, -0.037, 0], 'blackPlastic'),
    box(0.006, 0.08, 0.025, [0.037, 0, 0], 'blackPlastic'), box(0.006, 0.08, 0.025, [-0.037, 0, 0], 'blackPlastic'),
  ];
  for (let i = 0; i < 7; i++) out.push(extrude(blade, 0.004, [0, 0, 0.012], 'greyPlastic', [0, 0, (i / 7) * 2 * PI]));
  out.push(cyl(0.016, 0.016, 0.02, [0, 0, 0.004], 'greyPlastic', [H, 0, 0]));
  out.push(tor(0.036, 0.0012, [0, 0, 0.0135], 'chrome'));
  out.push(tor(0.024, 0.0012, [0, 0, 0.0135], 'chrome'));
  return out;
}

// Sealed Deutsch-style plug on the box wall, axis along x.
function deutsch({ ways = 4, tbd = false }) {
  const m = tbd ? 'tbd' : 'greyPlastic';
  const r = 0.008 + ways * 0.0012;
  return [
    cyl(r, r, 0.03, [0.015, 0, 0], m, [0, 0, H], 28),
    box(0.004, r * 2.6, r * 2.6, [0.002, 0, 0], m),
    cyl(r * 0.8, r * 0.8, 0.002, [0.031, 0, 0], tbd ? 'tbd' : 'orangeSeal', [0, 0, H], 28),
    ...(tbd ? [] : [box(0.012, 0.004, 0.006, [0.02, r + 0.002, 0], 'greyPlastic')]),
  ];
}

function estop() {
  return [
    box(0.04, 0.04, 0.04, [0, 0.02, 0], 'yellow'),
    cyl(0.011, 0.011, 0.01, [0, 0.045, 0], 'chrome'),
    cyl(0.02, 0.02, 0.012, [0, 0.055, 0], 'red', null, 32),
    { ...sph(0.02, [0, 0.06, 0], 'red', [0, 2 * PI, 0, H * 0.6]) },
  ];
}

function keySwitch() {
  return [
    cyl(0.012, 0.012, 0.006, [0, 0.003, 0], 'chrome', null, 28),
    cyl(0.009, 0.009, 0.03, [0, -0.015, 0], 'steel'),
    box(0.003, 0.02, 0.012, [0, 0.016, 0], 'brass'),
    box(0.012, 0.012, 0.004, [0, 0.03, 0], 'blackPlastic'),
  ];
}

function button({ m = 'red' }) {
  return [cyl(0.012, 0.012, 0.006, [0, 0.003, 0], 'blackPlastic'), cyl(0.009, 0.009, 0.006, [0, 0.009, 0], m)];
}

function oled() {
  return [box(0.038, 0.0016, 0.012, [0, 0.0008, 0], 'pcbBlue'), box(0.03, 0.0012, 0.0085, [0.002, 0.0022, 0], 'screen'), ...pins(4, 0.00254, [-0.016, 0.004, 0])];
}

// Speaker cone from a lathe profile, facing +y.
function speaker({ d = 0.076 }) {
  const r = d / 2;
  const cone = [[0.008, 0.0], [r * 0.5, 0.008], [r * 0.85, 0.016], [r * 0.9, 0.018]];
  return [
    lathe(cone, [0, 0.0, 0], 'speakerCone'),
    tor(r * 0.92, 0.003, [0, 0.018, 0], 'rubber', [H, 0, 0]),
    cyl(r, r, 0.003, [0, 0.019, 0], 'steel', null, 36),
    cyl(0.018, 0.018, 0.016, [0, -0.01, 0], 'blackPlastic', null, 28),
    { ...sph(0.009, [0, 0.004, 0], 'blackPlastic', [0, 2 * PI, 0, H]) },
  ];
}

function hall() {
  return [
    box(0.005, 0.004, 0.0016, [0, 0.002, 0], 'blackPlastic'),
    ...[-0.0013, 0, 0.0013].map((x) => box(0.0004, 0.012, 0.0004, [x, -0.004, 0], 'steel')),
    box(0.016, 0.02, 0.003, [0, 0.003, -0.004], 'greyPlastic'),
    cyl(0.005, 0.005, 0.005, [0, 0.003, 0.02], 'steel', [H, 0, 0]),
  ];
}

function pedal() {
  return [
    box(0.07, 0.006, 0.11, [0, 0.03, 0], 'blackPlastic', [-0.35, 0, 0]),
    box(0.06, 0.012, 0.025, [0, 0.006, -0.05], 'greyPlastic'),
    cyl(0.004, 0.004, 0.06, [0, 0.012, -0.05], 'steel', [0, 0, H]),
    box(0.02, 0.02, 0.02, [0.03, 0.012, -0.05], 'blackPlastic'),
  ];
}

function shifter() {
  return [
    box(0.06, 0.02, 0.08, [0, 0.01, 0], 'blackPlastic'),
    cyl(0.004, 0.004, 0.09, [0, 0.06, 0], 'chrome', [0.25, 0, 0]),
    { ...sph(0.012, [0.011, 0.104, 0], 'red') },
  ];
}

function microSwitch() {
  return [box(0.02, 0.01, 0.006, [0, 0.005, 0], 'blackPlastic'), box(0.016, 0.0008, 0.004, [0, 0.011, 0], 'steel', [0, 0, 0.2])];
}

function seatSwitch() {
  return [box(0.12, 0.004, 0.12, [0, 0.002, 0], 'greyPlastic'), box(0.1, 0.002, 0.1, [0, 0.005, 0], 'blackPlastic')];
}

function probe() {
  return [
    cyl(0.003, 0.003, 0.035, [0, 0.003, 0], 'steel', [0, 0, H]),
    tube([[0.017, 0.003, 0], [0.04, 0.003, 0.01], [0.05, 0.003, 0.04], [0.02, 0.003, 0.05]], 0.0016, 'blackPlastic'),
  ];
}

// Slim angled LED headlight bars either side of the grille.
function headlights() {
  const out = [];
  for (const s of [-1, 1]) {
    out.push(box(0.04, 0.035, 0.13, [0, 0.0, 0.18 * s], 'bodyAccent', [0, -0.35 * s, 0.25]));
    out.push(box(0.006, 0.012, 0.12, [0.021, 0.006, 0.18 * s], 'lens', [0, -0.35 * s, 0.25]));
    out.push(box(0.005, 0.02, 0.06, [0.021, -0.01, 0.2 * s], 'glass', [0, -0.35 * s, 0.25]));
  }
  return out;
}

// Passives, lying flat on a board.
function resistor({ watts = 0.25 }) {
  const big = watts >= 1;
  if (big) return [box(0.022, 0.009, 0.009, [0, 0.0045, 0], 'white'), cyl(0.0006, 0.0006, 0.034, [0, 0.004, 0], 'steel', [0, 0, H])];
  return [
    cyl(0.0012, 0.0012, 0.0062, [0, 0.0012, 0], 'beige', [0, 0, H], 12),
    ...[-0.0018, -0.0006, 0.0006].map((x, i) => cyl(0.00125, 0.00125, 0.0005, [x, 0.0012, 0], ['bandBrown', 'bandBlack', 'bandRed'][i], [0, 0, H], 12)),
    cyl(0.0003, 0.0003, 0.011, [0, 0.0012, 0], 'steel', [0, 0, H], 6),
  ];
}

function ceramicCap() {
  return [{ ...cyl(0.0025, 0.0025, 0.0015, [0, 0.003, 0], 'yellowCap', [H, 0, 0], 16) }, box(0.0004, 0.004, 0.0004, [0.001, 0.0005, 0], 'steel'), box(0.0004, 0.004, 0.0004, [-0.001, 0.0005, 0], 'steel')];
}

function electrolytic({ r = 0.005, h = 0.013 }) {
  return [cyl(r, r, h, [0, h / 2, 0], 'blueCap', null, 20), cyl(r * 0.96, r * 0.96, 0.0005, [0, h + 0.0002, 0], 'brushed', null, 20), box(0.0016, h * 0.98, 0.0005, [0, h / 2, r], 'white')];
}

function diode({ big = false }) {
  const r = big ? 0.0014 : 0.0009;
  return [cyl(r, r, big ? 0.0052 : 0.0038, [0, r, 0], big ? 'blackPlastic' : 'glass', [0, 0, H], 12), cyl(r * 1.05, r * 1.05, 0.0006, [big ? 0.0018 : 0.0013, r, 0], big ? 'steel' : 'blackPlastic', [0, 0, H], 12), cyl(0.0003, 0.0003, 0.011, [0, r, 0], 'steel', [0, 0, H], 6)];
}

function to220() {
  return [box(0.010, 0.009, 0.0045, [0, 0.0105, 0], 'blackPlastic'), box(0.010, 0.0065, 0.0013, [0, 0.0185, -0.0016], 'steel'), ...[-0.00254, 0, 0.00254].map((x) => box(0.0008, 0.013, 0.0005, [x, 0.0, 0], 'steel'))];
}

function tvs() {
  return [cyl(0.0027, 0.0027, 0.009, [0, 0.0027, 0], 'tbd', [0, 0, H], 16)];
}

function protoBoard({ w = 0.09, d = 0.07 }) {
  const out = [box(w, 0.0016, d, [0, 0.0008, 0], 'pcbProto')];
  for (let i = 0; i < 4; i++) out.push(...terminals(4, [-w / 2 + 0.013 + i * 0.022, 0.0056, -d / 2 + 0.005], 'greenTerminal'));
  for (const [x, z] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) out.push(cyl(0.0025, 0.0025, 0.012, [x * (w / 2 - 0.004), -0.006, z * (d / 2 - 0.004)], 'whiteNylon', null, 6));
  return out;
}

// Electronics box: open tray (floor + 4 walls) and a separate lid. Size TBD (#18).
function boxTray({ w = 0.25, h = 0.1, d = 0.2, t = 0.003 }) {
  return [
    box(w, t, d, [0, -h / 2 + t / 2, 0], 'boxAbs'),
    box(t, h, d, [w / 2 - t / 2, 0, 0], 'boxAbs'),
    box(t, h, d, [-w / 2 + t / 2, 0, 0], 'boxAbs'),
    box(w, h, t, [0, 0, d / 2 - t / 2], 'boxAbs'),
    box(w, h, t, [0, 0, -d / 2 + t / 2], 'boxAbs'),
    box(w + 0.03, 0.004, 0.02, [0, -h / 2 + 0.002, d / 2 + 0.01], 'boxAbs'),
    box(w + 0.03, 0.004, 0.02, [0, -h / 2 + 0.002, -d / 2 - 0.01], 'boxAbs'),
  ];
}

function boxLid({ w = 0.25, d = 0.2 }) {
  return [box(w + 0.004, 0.004, d + 0.004, [0, 0, 0], 'boxAbs'), box(w - 0.01, 0.002, d - 0.01, [0, -0.003, 0], 'rubber'), box(0.06, 0.0006, 0.03, [0.06, 0.0023, 0.05], 'label')];
}

// ---- stock car body: Can-Am Maverick X3 24V Ride-On Buggy, from the listing photos and size
// (147 x 86 x 88 cm). Gloss black body, sand "RS" door panels, red grilles, red coil-overs,
// black tube cage, knobby tyres on red beadlock rims. Shapes are approximate.

function tub() {
  return [
    box(1.26, 0.03, 0.56, [0, 0.135, 0], 'bodyAccent'),
    box(1.16, 0.1, 0.012, [0, 0.19, 0.28], 'bodyAccent'),
    box(1.16, 0.1, 0.012, [0, 0.19, -0.28], 'bodyAccent'),
    box(0.012, 0.1, 0.56, [0.6, 0.19, 0], 'bodyAccent'),
    box(0.012, 0.1, 0.56, [-0.6, 0.19, 0], 'bodyAccent'),
    cyl(0.01, 0.01, 0.74, [0.47, 0.17, 0], 'steel', [H, 0, 0]),
    cyl(0.01, 0.01, 0.74, [-0.47, 0.17, 0], 'steel', [H, 0, 0]),
    // A-arms out to the front hubs, trailing arms at the rear
    ...[-1, 1].flatMap((s) => [
      box(0.03, 0.015, 0.2, [0.47, 0.15, 0.2 * s], 'cageBlack', [0, 0.25 * s, 0]),
      box(0.03, 0.015, 0.2, [0.47, 0.2, 0.2 * s], 'cageBlack', [0, -0.25 * s, 0]),
      box(0.05, 0.03, 0.18, [-0.47, 0.17, 0.2 * s], 'cageBlack'),
    ]),
    box(0.1, 0.012, 0.3, [0.6, 0.1, 0], 'brushed'), // skid plate
  ];
}

// Nose, rear deck, side panels and fender flares.
function bodyShell() {
  const nose = [[0.26, 0.0], [0.74, 0.0], [0.77, 0.06], [0.72, 0.13], [0.55, 0.19], [0.36, 0.22], [0.26, 0.23]];
  const rear = [[-0.75, 0.02], [-0.32, 0.0], [-0.32, 0.2], [-0.45, 0.21], [-0.68, 0.17], [-0.75, 0.11]];
  const out = [
    extrude(nose, 0.5, [0, 0.2, -0.25], 'bodyPlastic'),
    extrude(rear, 0.5, [0, 0.2, -0.25], 'bodyPlastic'),
    // hood scoop and bulges
    box(0.18, 0.025, 0.14, [0.47, 0.4, 0], 'bodyAccent', [0, 0, -0.18]),
    box(0.22, 0.02, 0.08, [0.5, 0.39, 0.17], 'bodyPlastic', [0, 0, -0.2]),
    box(0.22, 0.02, 0.08, [0.5, 0.39, -0.17], 'bodyPlastic', [0, 0, -0.2]),
    // red mesh grilles, front and rear
    box(0.012, 0.11, 0.22, [0.765, 0.29, 0], 'grilleRed'),
    box(0.03, 0.03, 0.26, [0.76, 0.355, 0], 'bodyPlastic'),
    box(0.012, 0.09, 0.2, [-0.755, 0.28, 0], 'grilleRed'),
    // front bumper, tow hook
    box(0.05, 0.05, 0.46, [0.78, 0.2, 0], 'bodyAccent'),
    tor(0.018, 0.005, [0.81, 0.17, 0], 'red', [0, H, 0]),
    // side sills
    box(0.62, 0.05, 0.03, [-0.03, 0.22, 0.3], 'bodyAccent'),
    box(0.62, 0.05, 0.03, [-0.03, 0.22, -0.3], 'bodyAccent'),
  ];
  // sand "RS" door panels and rear quarters
  for (const s of [-1, 1]) {
    out.push(box(0.5, 0.18, 0.02, [-0.02, 0.35, 0.305 * s], 'doorSand'));
    out.push(box(0.2, 0.15, 0.02, [-0.44, 0.34, 0.3 * s], 'sand'));
    out.push(box(0.5, 0.012, 0.024, [-0.02, 0.445, 0.305 * s], 'bodyAccent'));
  }
  // fender flares
  for (const [x, z] of [[0.47, 0.37], [0.47, -0.37], [-0.47, 0.37], [-0.47, -0.37]]) {
    out.push(tor(0.205, 0.022, [x, 0.17, z], 'bodyAccent', [0, 0, 0.35], PI * 0.62));
    out.push(box(0.1, 0.012, 0.12, [x, 0.395, z * 0.86], 'bodyAccent'));
  }
  return out;
}

// Red coil-over shocks, one per wheel: chrome body, red helical spring, top and bottom eyes.
function coilover({ len = 0.2 }) {
  const turns = 7;
  const r = 0.016;
  const pts = [];
  for (let i = 0; i <= turns * 12; i++) {
    const t = i / (turns * 12);
    pts.push([Math.cos(t * turns * 2 * PI) * r, 0.03 + t * (len - 0.06), Math.sin(t * turns * 2 * PI) * r]);
  }
  return [
    cyl(0.011, 0.011, len * 0.55, [0, len * 0.3, 0], 'chrome'),
    cyl(0.006, 0.006, len * 0.5, [0, len * 0.72, 0], 'steel'),
    tube(pts, 0.0035, 'shockRed'),
    cyl(0.021, 0.021, 0.008, [0, 0.03, 0], 'shockRed'),
    cyl(0.021, 0.021, 0.008, [0, len - 0.03, 0], 'shockRed'),
    cyl(0.013, 0.013, 0.05, [0.0, len * 0.55, 0.03], 'shockRed', [H, 0, 0]), // remote reservoir
    tor(0.01, 0.004, [0, 0, 0], 'steel', [0, H, 0]),
    tor(0.01, 0.004, [0, len, 0], 'steel', [0, H, 0]),
  ];
}

function shocks() {
  const out = [];
  for (const [x, z] of [[0.47, 0.25], [0.47, -0.25], [-0.47, 0.25], [-0.47, -0.25]]) {
    for (const p of coilover({ len: 0.22 })) {
      // tubes carry their own points and no position
      if (p.pts) out.push({ ...p, pts: p.pts.map(([a, b, c]) => [a + x, b + 0.17, c + z]) });
      else out.push({ ...p, p: [p.p[0] + x, p.p[1] + 0.17, p.p[2] + z] });
    }
  }
  return out;
}

function rollCage() {
  const m = 'cageBlack';
  const out = [];
  for (const z of [-0.3, 0.3]) {
    // A-pillar, roof rail, rear pillar, all one bent tube
    out.push(tube([[0.3, 0.42, z], [0.2, 0.62, z * 0.97], [0.06, 0.83, z * 0.93], [-0.14, 0.86, z * 0.92], [-0.32, 0.84, z * 0.93], [-0.4, 0.66, z * 0.97], [-0.43, 0.44, z]], 0.014, m));
    // door bar
    out.push(tube([[0.27, 0.47, z * 1.03], [-0.38, 0.47, z * 1.03]], 0.01, m));
  }
  out.push(tube([[0.06, 0.83, -0.279], [0.06, 0.83, 0.279]], 0.012, m)); // front roof hoop
  out.push(tube([[-0.32, 0.84, -0.279], [-0.32, 0.84, 0.279]], 0.012, m)); // rear roof hoop
  out.push(tube([[-0.14, 0.86, -0.276], [-0.14, 0.86, 0.276]], 0.01, m));
  out.push(tube([[0.2, 0.62, -0.291], [0.13, 0.6, 0], [0.2, 0.62, 0.291]], 0.011, m)); // dash bar
  // rear X brace behind the seats, and the harness bar
  out.push(tube([[-0.39, 0.8, -0.28], [-0.41, 0.5, 0.29]], 0.009, m));
  out.push(tube([[-0.39, 0.8, 0.28], [-0.41, 0.5, -0.29]], 0.009, m));
  out.push(tube([[-0.38, 0.68, -0.29], [-0.38, 0.68, 0.29]], 0.01, m));
  // brackets where the cage meets the body
  for (const [x, z] of [[0.3, 0.3], [0.3, -0.3], [-0.43, 0.3], [-0.43, -0.3]]) out.push(box(0.05, 0.02, 0.04, [x, 0.43, z], 'brushed'));
  return out;
}

function seat() {
  return [
    box(0.24, 0.05, 0.22, [0, 0.025, 0], 'seatFabric'),
    box(0.05, 0.32, 0.22, [-0.12, 0.18, 0], 'seatFabric', [0, 0, 0.18]),
    box(0.035, 0.09, 0.16, [-0.14, 0.38, 0], 'seatFabric', [0, 0, 0.18]),
    // red side bolsters, as on the listing photos
    box(0.24, 0.06, 0.03, [0, 0.05, 0.11], 'seatRed'),
    box(0.24, 0.06, 0.03, [0, 0.05, -0.11], 'seatRed'),
    box(0.06, 0.24, 0.03, [-0.115, 0.17, 0.11], 'seatRed', [0, 0, 0.18]),
    box(0.06, 0.24, 0.03, [-0.115, 0.17, -0.11], 'seatRed', [0, 0, 0.18]),
    box(0.24, 0.02, 0.22, [0, -0.01, 0], 'blackPlastic'),
  ];
}

function dashPanel() {
  return [box(0.12, 0.14, 0.58, [0, 0, 0], 'bodyAccent', [0, 0, -0.5]), box(0.02, 0.02, 0.58, [-0.05, 0.07, 0], 'chrome')];
}

function steeringWheel() {
  return [
    tor(0.11, 0.012, [0, 0, 0], 'blackPlastic', [0, H, 0]),
    cyl(0.025, 0.025, 0.03, [0, 0, 0], 'bodyAccent', [0, 0, H]),
    box(0.008, 0.2, 0.012, [0, 0, 0], 'blackPlastic', [0, 0, 0]),
    box(0.008, 0.012, 0.2, [0, 0, 0], 'blackPlastic'),
    cyl(0.01, 0.01, 0.3, [0.13, -0.08, 0], 'steel', [0, 0, 1.0]),
  ];
}

// ---------------------------------------------------------------------------------------------

export const BUILDERS = {
  wheel, driveMotor, steeringMotor, battery, contactor, anlFuse, inlineFuse, fuseBlock, busbar, ibt2, mdd20a,
  esp32, receiver, buck, sealedBuck, fan, deutsch, estop, keySwitch, button, oled, speaker, hall, pedal, shifter,
  microSwitch, seatSwitch, probe, headlights, resistor, ceramicCap, electrolytic, diode, to220, tvs, protoBoard,
  boxTray, boxLid, tub, bodyShell, rollCage, shocks, seat, dashPanel, steeringWheel, module,
  bladeFuse: ({ m }) => bladeFuse(m),
};

export function buildPrimitives(kind, params = {}) {
  const fn = BUILDERS[kind];
  if (!fn) return [box(0.03, 0.02, 0.03, [0, 0.01, 0], 'tbd')];
  return fn(params);
}
