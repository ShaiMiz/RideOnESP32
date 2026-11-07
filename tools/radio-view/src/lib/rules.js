// Safety and consistency checks on an edited model, before it may be exported.
//
// The checks run the mixer over every switch combination and the stick extremes, so they judge
// what the channels actually do, not how the mixes are spelled. Errors block the export; warnings
// say what else has to change (CHANNEL_MAP.md, tools/check_radio_files.py) for CI to pass.
import { SWITCHES, STICKS, TRIM_NAMES, mixesFor } from './model.js';
import { mix, toUs, axisOf } from './mixer.js';
import { switchPosition, stickFraction } from './firmware.js';

// The T-Lite's left stick up/down does not spring back (radio/MODEL_SETUP.md).
export const NON_CENTRING_AXES = ['lv'];

function* switchCombos() {
  const names = Object.keys(SWITCHES);
  const total = names.reduce((n, s) => n * SWITCHES[s], 1);
  for (let i = 0; i < total; i++) {
    let k = i;
    const sw = {};
    for (const s of names) {
      sw[s] = k % SWITCHES[s];
      k = Math.floor(k / SWITCHES[s]);
    }
    yield sw;
  }
}

function* stickCombos(axes) {
  const vals = [-1, 0, 1];
  const n = axes.length;
  for (let i = 0; i < 3 ** n; i++) {
    let k = i;
    const s = { lh: 0, lv: 0, rh: 0, rv: 0 };
    for (const a of axes) {
      s[a] = vals[k % 3];
      k = Math.floor(k / 3);
    }
    yield s;
  }
}

const us = (config, input, ch) => toUs(mix(config, input).channels[ch - 1]);

/** Switch positions the start-up check asks for, as position indexes (or null when not checked). */
export function warningPositions(config) {
  const out = {};
  for (const [sw, n] of Object.entries(SWITCHES)) {
    const p = config.switchWarning[sw];
    out[sw] = p === 'u' ? 0 : p === 'd' ? n - 1 : p === 'm' && n === 3 ? 1 : null;
  }
  return out;
}

export function check(config, ch, limits, stickMode = 2) {
  const errors = [];
  const warnings = [];
  const run = (sticks, switches) => ({ sticks, switches, stickMode });
  const isKill = (u) => switchPosition(u, limits) !== 2;
  const allSticks = [...stickCombos(['lh', 'lv', 'rh', 'rv'])];
  const combos = [...switchCombos()];

  // 1. One switch on its own must kill: some position of a single switch gives KILL whatever
  //    the other switches and the sticks do.
  const killers = [];
  for (const [sw, n] of Object.entries(SWITCHES)) {
    for (let p = 0; p < n; p++) {
      const kills = combos.filter((c) => c[sw] === p).every((c) => allSticks.every((s) => isKill(us(config, run(s, c), ch.KILL))));
      // ...and the switch really is the control: moving it away lets the car run.
      const releases = combos.some((c) => c[sw] !== p && !isKill(us(config, run({ lh: 0, lv: 0, rh: 0, rv: 0 }, c), ch.KILL)));
      if (kills && releases) killers.push({ sw, p, n });
    }
  }
  const twoPos = killers.filter((k) => k.n === 2);
  const runPossible = combos.some((c) => !isKill(us(config, run({ lh: 0, lv: 0, rh: 0, rv: 0 }, c), ch.KILL)));
  if (!killers.length) errors.push(`CH${ch.KILL} Kill: no switch position kills on its own. One 2-position switch must give KILL whatever else is moved.`);
  else if (!twoPos.length) errors.push(`CH${ch.KILL} Kill: only a 3-position switch kills. Put Kill on a 2-position switch (SC or SD) so it can be found by feel, and so ELRS Hybrid's on/off CH5 carries it.`);
  if (!runPossible) errors.push(`CH${ch.KILL} Kill: no switch position gives Run, so the car could never drive.`);
  const killStick = STICKS.filter((src) => mixesFor(config, ch.KILL).some((m) => m.src === src));
  if (killStick.length) errors.push(`CH${ch.KILL} Kill must come from a switch only, not from a stick (${killStick.join(', ')}).`);

  // 2. Start-up check: in the positions the radio insists on at power-on, Kill and Locked.
  const wp = warningPositions(config);
  const unchecked = Object.entries(wp).filter(([, p]) => p === null).map(([s]) => s);
  const freeCombos = combos.filter((c) => Object.entries(wp).every(([s, p]) => p === null || c[s] === p));
  const neutral = { lh: 0, lv: 0, rh: 0, rv: 0 };
  const startKill = freeCombos.every((c) => isKill(us(config, run(neutral, c), ch.KILL)));
  const startLocked = freeCombos.every((c) => switchPosition(us(config, run(neutral, c), ch.MODE), limits) === 0);
  if (!startKill) errors.push(`Start-up check: with the switches where the radio asks for them at power-on (${describeWarning(config)}), CH${ch.KILL} is not KILL. Ask for the kill switch's KILL position.`);
  if (!startLocked) errors.push(`Start-up check: at power-on CH${ch.MODE} Mode is not Locked. Ask for the mode switch's Locked position.`);
  if (unchecked.length && (startKill && startLocked)) {
    // fine: the unchecked switches don't matter for Kill and Mode
  }

  // 3. Letting go stops: with the spring-back axes released, steering and throttle are centred
  //    (inside the firmware deadband) for every switch position and wherever the non-centring
  //    axis was left.
  const released = [...stickCombos(NON_CENTRING_AXES)];
  for (const [role, label] of [['THROTTLE', 'Throttle'], ['STEERING', 'Steering']]) {
    const bad = combos.some((c) => released.some((s) => stickFraction(us(config, run(s, c), ch[role]), limits) !== 0));
    if (bad) errors.push(`CH${ch[role]} ${label}: not centred with the sticks let go. It must come only from a stick axis that springs back (the T-Lite's left stick up/down does not), with no offset.`);
    const live = combos.every((c) => {
      const hi = Math.max(...allSticks.map((s) => us(config, run(s, c), ch[role])));
      const lo = Math.min(...allSticks.map((s) => us(config, run(s, c), ch[role])));
      return hi > 1900 && lo < 1100;
    });
    if (!bad && !live) warnings.push(`CH${ch[role]} ${label}: doesn't reach full travel both ways in every switch position.`);
  }
  const thrSrc = mixesFor(config, ch.THROTTLE).map((m) => m.src).filter((s) => STICKS.includes(s));
  if (thrSrc.some((s) => NON_CENTRING_AXES.includes(axisOf(s, stickMode)))) {
    errors.push(`CH${ch.THROTTLE} Throttle uses ${thrSrc.join(', ')}, which sits on the left stick up/down in mode ${stickMode}: it does not spring back.`);
  }

  // 4. Mode: some switch position gives Locked.
  const lockable = combos.some((c) => switchPosition(us(config, run(neutral, c), ch.MODE), limits) === 0);
  if (!lockable) errors.push(`CH${ch.MODE} Mode: no switch position gives Locked.`);
  if (mixesFor(config, ch.MODE).some((m) => STICKS.includes(m.src))) errors.push(`CH${ch.MODE} Mode must come from a switch only.`);

  // 5. Trims, module, name: carried through from the file, but the export must still be one the
  //    installer and the firmware accept.
  if (!config.trimsOff) errors.push('Trims must stay off: a trimmed throttle that is not centred stops the car arming.');
  if (config.module !== 'TYPE_CROSSFIRE') errors.push('The internal module must be CRSF (ELRS).');
  if (config.name !== 'RideOn') errors.push('The model must be called RideOn: install_radio.py finds it by name.');

  // 6. Horn: a warning only.
  const presses = [{}, ...TRIM_NAMES.map((t) => ({ [t]: true }))];
  const hornOn = combos.some((c) => allSticks.some((s) => presses.some((trims) => switchPosition(us(config, { ...run(s, c), trims }, ch.HORN), limits) === 2)));
  if (!hornOn) warnings.push(`CH${ch.HORN} Horn: nothing turns it on.`);
  const hornIdle = combos.some((c) => switchPosition(us(config, run(neutral, c), ch.HORN), limits) !== 2);
  if (!hornIdle) warnings.push(`CH${ch.HORN} Horn: it is on with the sticks let go.`);

  // 7. One control, one job: each control the firmware listens to moves exactly one of its
  //    channels, and each of those channels follows exactly one control, whatever the other
  //    switches and sticks are doing. The only cross-effects allowed are the firmware's own
  //    (kill stops everything, the mode picks who drives, the speed cap applies in Kid), and
  //    those are tested in test/test_controller, not made in the radio.
  const reach = controlReach(config, ch, limits, stickMode);
  for (const [control, chans] of Object.entries(reach)) {
    if (chans.length > 1) errors.push(`${controlName(control)} moves ${chans.map((c) => channelRole(c, ch)).join(' and ')}. One control must drive one function only.`);
  }
  for (const role of FIRMWARE_ROLES) {
    // a trim pair (T1- and T1+) counts as one control
    const by = [...new Set(Object.entries(reach).filter(([, chans]) => chans.includes(ch[role])).map(([c]) => (TRIM_NAMES.includes(c) ? `Trim buttons ${c.slice(0, 2)}` : controlName(c))))];
    if (by.length > 1) errors.push(`CH${ch[role]} ${channelRole(ch[role], ch).replace(/^CH\d+ /, '')} follows ${by.join(' and ')}. A function must come from one control only.`);
  }

  return { errors, warnings, killers };
}

// The channels the firmware reads (ChannelMap.h), and how it reads each one.
export const FIRMWARE_ROLES = ['STEERING', 'THROTTLE', 'KILL', 'MODE', 'SPEED_CAP', 'HORN'];
const STICK_ROLES = ['STEERING', 'THROTTLE'];
const ROLE_NAME = { STEERING: 'Steering', THROTTLE: 'Throttle', KILL: 'Kill', MODE: 'Mode', SPEED_CAP: 'Kid speed cap', HORN: 'Horn' };
const AXIS_LABEL = { lh: 'Left stick sideways', lv: 'Left stick up/down', rh: 'Right stick sideways', rv: 'Right stick up/down' };
const channelRole = (c, ch) => {
  const role = FIRMWARE_ROLES.find((r) => ch[r] === c);
  return `CH${c} ${role ? ROLE_NAME[role] : ''}`.trim();
};
const controlName = (c) => AXIS_LABEL[c] || (TRIM_NAMES.includes(c) ? `Trim button ${c}` : c);

/**
 * Which firmware channels each control reaches, judged the way the firmware reads them (a stick
 * fraction for steering and throttle, a switch position for the rest). A control reaches a
 * channel if moving it alone changes that reading for any position of all the other controls.
 * Returns { SA: [6], ..., lh: [], ..., 'T1+': [8], ... } (channel numbers, sorted).
 */
export function controlReach(config, ch, limits, stickMode = 2) {
  const read = (role, u) => (STICK_ROLES.includes(role) ? stickFraction(u, limits) : switchPosition(u, limits));
  const out = {};
  const axes = ['lh', 'lv', 'rh', 'rv'];
  const controls = [
    ...Object.entries(SWITCHES).map(([sw, n]) => ({ name: sw, values: [...Array(n).keys()], set: (i, p) => ({ ...i, switches: { ...i.switches, [sw]: p } }) })),
    ...axes.map((a) => ({ name: a, values: [-1, -0.6, 0, 0.6, 1], set: (i, v) => ({ ...i, sticks: { ...i.sticks, [a]: v } }) })),
    ...TRIM_NAMES.map((t) => ({ name: t, values: [false, true], set: (i, v) => ({ ...i, trims: v ? { [t]: true } : {} }) })),
  ];
  const combos = [...switchCombos()];
  const sticks = [...stickCombos(axes)];
  for (const c of controls) {
    const hit = new Set();
    for (const sw of combos) {
      for (const st of sticks) {
        const base = { sticks: st, switches: sw, stickMode };
        const readings = c.values.map((v) => {
          const u = mix(config, c.set(base, v)).channels.map(toUs);
          return FIRMWARE_ROLES.map((r) => read(r, u[ch[r] - 1]));
        });
        FIRMWARE_ROLES.forEach((r, k) => {
          if (readings.some((x) => x[k] !== readings[0][k])) hit.add(ch[r]);
        });
      }
    }
    out[c.name] = [...hit].sort((a, b) => a - b);
  }
  return out;
}

export function describeWarning(config) {
  const names = { u: 'up', m: 'middle', d: 'down' };
  const parts = Object.entries(config.switchWarning).filter(([, p]) => p).map(([s, p]) => `${s} ${names[p]}`);
  return parts.length ? parts.join(', ') : 'none';
}

/**
 * How the edited model differs from the repo's, channel by channel, so the PR can say what else
 * must change (CHANNEL_MAP.md, check_radio_files.py EXPECTED).
 */
export function diffFromRepo(config, repoConfig) {
  const out = [];
  const fmt = (m) => `${m.src} ${m.weight}%${m.offset ? ` ${m.offset > 0 ? '+' : ''}${m.offset}` : ''}${m.swtch !== 'NONE' ? ` if ${m.swtch}` : ''} ${m.mltpx}`;
  for (let c = 1; c <= 16; c++) {
    const a = mixesFor(repoConfig, c).map(fmt).join('; ');
    const b = mixesFor(config, c).map(fmt).join('; ');
    if (a !== b) out.push({ ch: c, from: a || '(none)', to: b || '(none)' });
  }
  const la = JSON.stringify(repoConfig.logical.map(({ idx, func, a, x, andsw }) => ({ idx, func, a, x, andsw })));
  const lb = JSON.stringify(config.logical.map(({ idx, func, a, x, andsw }) => ({ idx, func, a, x, andsw })));
  if (la !== lb) out.push({ ch: 'L', from: 'logical switches', to: 'changed' });
  if (describeWarning(config) !== describeWarning(repoConfig)) out.push({ ch: 'W', from: describeWarning(repoConfig), to: describeWarning(config) });
  return out;
}
