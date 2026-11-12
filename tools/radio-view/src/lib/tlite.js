// Jumper T-Lite V2: measurements and layout for the 3D model, and its specification.
//
// Sources (checked 7 Nov 2026):
//  - Jumper's product page, "Components" diagram (jumper-rc.com/transmitters/t-lite-v2): where every
//    key, trim, switch, the LED, speaker and antenna sit. Positions below are scaled off that
//    drawing using the published body width (166 mm); expect about ±1.5 mm.
//  - Oscar Liang's T-Lite V2 review: 166 x 106 x 56 mm, 207 g (258 g with an 18650), ports.
//  - Brushless Whoop's T-Lite review: 25 mm sticks, 3-position switches on top and 2-position
//    below, power button with the LED above it, two reserved screw holes on the back.
//  - EdgeTX radio/src/boards/hw_defs/tlite.json: inputs LH LV RV RH, switches SA SB (3POS) and
//    SC SD (2POS), keys RTN ENT Up Down Left/SYS Right/MDL, trims T1-T4.
//
// Coordinates are millimetres. Origin: midway between the two stick centres. x to the right,
// y up the face (towards the antenna edge), z out of the face towards your eyes.

export const BODY = {
  width: 166,
  height: 106,
  depth: 56, // overall, sticks included
  shell: 36, // the shell alone, at the grips
  top: 48.5, // top (antenna) edge, centre
  bottom: -57.5, // lowest point of the grips
};

// Right half of the outline, clockwise from top centre; the left half is its mirror.
export const OUTLINE_RIGHT = [
  [0, 48.5], [16, 48.5], [22, 46.5], [28, 44.8], [50, 45.2], [64, 43.5], [74, 38],
  [80, 27], [82.5, 12], [83, -2], [82, -16], [79.5, -31], [74.5, -45], [66, -54.5],
  [56, -57.5], [46, -55.5], [37, -49], [26, -43], [12, -41], [0, -40.5],
];

export const STICK_X = 46; // each stick centre, mm from the middle
export const GIMBAL = {
  recessRadius: 25, // the round well each gimbal sits in
  openingSize: 30, // the square opening of the gimbal frame
  stickLength: 25, // stick above the gimbal plate (Brushless Whoop)
  capDiameter: 9, // knurled metal stick end
  throwDeg: 25, // each way
};

export const SCREEN = {
  y: 30.8, // centre, above the stick line
  bezel: [38, 24],
  window: [33, 17],
  active: [29.5, 14.8], // 1.3 in, 128 x 64
};

// Keys. EdgeTX key name -> where it is and its shape.
export const KEYS = [
  { key: 'ENTER', label: 'ENT', x: -29.5, y: 40, shape: 'pill', w: 8.6, h: 4.3, angle: 0.28 },
  { key: 'EXIT', label: 'RTN', x: -29.5, y: 33, shape: 'pill', w: 8.6, h: 4.3, angle: 0.28 },
  { key: 'UP', label: 'UP', x: 29.5, y: 40, shape: 'pill', w: 8.6, h: 4.3, angle: -0.28 },
  { key: 'DOWN', label: 'DN', x: 29.5, y: 33, shape: 'pill', w: 8.6, h: 4.3, angle: -0.28 },
  { key: 'LEFT', label: 'SYS', x: -10.5, y: 7.6, shape: 'pill', w: 10.7, h: 3.4, angle: 0 },
  { key: 'RIGHT', label: 'MDL', x: 10.5, y: 7.6, shape: 'pill', w: 10.7, h: 3.4, angle: 0 },
  { key: 'POWER', label: '', x: 0, y: -15.7, shape: 'oval', w: 7, h: 12, angle: 0 },
];

export const LED = { x: 0, y: 0.4, d: 3.6 };

// Trim buttons: two clusters between the sticks. Left: T3 (left stick up/down, the throttle in
// mode 2) above/below, T1 (left stick sideways, rudder) left/right. Right: T2 (elevator) and T4
// (aileron). Triangles point the way they trim.
export const TRIMS = [
  { name: 'T3+', x: -11, y: -6.8, dir: 'up' },
  { name: 'T3-', x: -11, y: -17.1, dir: 'down' },
  { name: 'T1-', x: -16.4, y: -27.5, dir: 'left' },
  { name: 'T1+', x: -6, y: -27.5, dir: 'right' },
  { name: 'T2+', x: 11, y: -6.8, dir: 'up' },
  { name: 'T2-', x: 11, y: -17.1, dir: 'down' },
  { name: 'T4-', x: 6, y: -27.5, dir: 'left' },
  { name: 'T4+', x: 16.4, y: -27.5, dir: 'right' },
];

export const SPEAKER = { x: -66, y: -40, r: 6 };

// Toggle switches stick straight out of the back (antenna) edge, two per shoulder, one above the
// other. The upper ones are 3-position (SA left, SB right), the lower ones 2-position (SC, SD).
// "Up" is the lever tip towards the sky when you hold the radio, i.e. towards the face (+z).
export const SWITCH_POS = {
  SA: { x: -60, z: 7, positions: 3 },
  SC: { x: -66, z: -7, positions: 2 },
  SB: { x: 60, z: 7, positions: 3 },
  SD: { x: 66, z: -7, positions: 2 },
};

// On the back edge, between the switches.
export const BACK_PORTS = [
  { name: 'Trainer port (3.5 mm jack)', x: -30, z: -2, kind: 'jack' },
  { name: 'Micro-SD slot', x: 0, z: 8, kind: 'slot' },
  { name: 'SMA antenna connector', x: 0, z: -3, kind: 'sma' },
  { name: 'USB-C: charging, SD card, joystick, EdgeTX updates', x: 30, z: -2, kind: 'usbc' },
];

export const SPECS = [
  ['Radio', 'Jumper T-Lite V2, ELRS version (internal 2.4 GHz ExpressLRS module)'],
  ['Firmware', 'EdgeTX 2.11.8, target tlite (EdgeTX 2.12 has no T-Lite build)'],
  ['Size', '166 x 106 x 56 mm'],
  ['Weight', '207 g, 258 g with the battery'],
  ['Battery', 'One 18650 Li-ion, flat top (button-top cells do not fit), charged over USB-C. No hot swap'],
  ['Screen', '1.3 in monochrome LCD, 128 x 64, backlit'],
  ['Sticks', 'Two hall-sensor gimbals, 25 mm sticks (tops unscrew for about 5 mm more). Mode 2: the left stick up/down has no spring and stays where it is left'],
  ['Switches', 'SA and SB 3-position (upper), SC and SD 2-position (lower), all on the back edge. No momentary switch, no knobs or sliders'],
  ['Trims', 'T1 to T4, a pair of buttons each, between the sticks. Off as trims in the RideOn model, which frees them as push-buttons: the T1 pair is the horn'],
  ['Keys', 'ENT and RTN left of the screen, UP and DOWN right of it, SYS and MDL under it'],
  ['Power', 'Oval button below the green status LED, hold to turn on or off'],
  ['Feedback', 'Speaker (lower left), vibration motor'],
  ['Ports', 'Trainer jack, micro-SD, SMA antenna, USB-C, all on the back edge; external module bay inside (needs the bottom shell off)'],
  ['Antenna', 'Removable dipole on SMA. Never power the radio on without it'],
  ['Back', 'Two reserved screw holes for a small FPV monitor holder'],
  ['Output', 'Internal ELRS module, up to 100 mW (RideOn ELRS setup sets 100 mW with dynamic power)'],
];
