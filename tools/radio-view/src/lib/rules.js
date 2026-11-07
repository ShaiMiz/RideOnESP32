// Safety and consistency checks on an edited model, before it may be exported.
//
// The checks run the mixer over every switch combination and the stick extremes, so they judge
// what the channels actually do, not how the mixes are spelled. Errors block the export; warnings
// say what else has to change (CHANNEL_MAP.md, tools/check_radio_files.py) for CI to pass.
import { SWITCHES, STICKS, mixesFor } from './model.js';
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
  const hornOn = combos.some((c) => allSticks.some((s) => switchPosition(us(config, run(s, c), ch.HORN), limits) === 2));
  if (!hornOn) warnings.push(`CH${ch.HORN} Horn: nothing turns it on.`);
  const hornIdle = combos.some((c) => switchPosition(us(config, run(neutral, c), ch.HORN), limits) !== 2);
  if (!hornIdle) warnings.push(`CH${ch.HORN} Horn: it is on with the sticks let go.`);

  return { errors, warnings, killers };
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
