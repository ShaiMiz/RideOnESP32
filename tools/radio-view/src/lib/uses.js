// What each physical control does in a config, and what a channel value means to the car.
import { SWITCHES, STICKS } from './model.js';
import { mix, toUs, axisOf } from './mixer.js';
import { switchPosition, stickFraction } from './firmware.js';
import { ROLE_LABEL } from './repo.js';

export function roleOf(chNum, CH) {
  return Object.keys(CH).find((k) => CH[k] === chNum);
}

export function channelLabel(chNum, CH, config) {
  const role = roleOf(chNum, CH);
  if (role) return ROLE_LABEL[role] || role;
  const named = config.mixes.find((m) => m.ch === chNum && m.name);
  if (!named) return 'spare';
  return /^unused$/i.test(named.name) ? 'unused' : `${named.name} (spare)`;
}

/** What the firmware makes of a channel value, in words. */
export function meaning(chNum, us, CH, L) {
  const role = roleOf(chNum, CH);
  const pos = switchPosition(us, L);
  if (pos < 0) return 'invalid';
  switch (role) {
    case 'KILL': return pos === 2 ? 'Run' : 'KILL';
    case 'MODE': return ['Locked', 'Kid', 'Parent'][pos];
    case 'SPEED_CAP': return `${Math.round(L.kidCaps[pos] * 100)} %`;
    case 'HORN': return pos === 2 ? 'horn on' : 'off';
    case 'THROTTLE': {
      const f = stickFraction(us, L);
      return f === 0 ? 'stop' : f > 0 ? `forward ${Math.round(f * 100)} %` : `back ${Math.round(-f * 100)} %`;
    }
    case 'STEERING': {
      const f = stickFraction(us, L);
      return f === 0 ? 'straight' : f > 0 ? `right ${Math.round(f * 100)} %` : `left ${Math.round(-f * 100)} %`;
    }
    default: return 'not read by the car';
  }
}

/**
 * For each control (SA..SD, and the four stick axes), the channels it moves, found by moving it
 * alone from the current state and watching the outputs.
 */
export function controlUses(config, input, CH, L) {
  const out = {};
  // channels whose value differs between the variants
  const changed = (variants) => {
    const outs = variants.map((v) => mix(config, v).channels.map(toUs));
    const set = [];
    for (let c = 0; c < outs[0].length; c++) {
      const vals = outs.map((o) => o[c]);
      if (Math.max(...vals) - Math.min(...vals) > 2) set.push(c + 1);
    }
    return set;
  };
  for (const [sw, n] of Object.entries(SWITCHES)) {
    const variants = [];
    for (let p = 0; p < n; p++) variants.push({ ...input, switches: { ...input.switches, [sw]: p } });
    out[sw] = changed(variants).map((c) => ({
      ch: c,
      label: channelLabel(c, CH, config),
      positions: variants.map((v) => {
        const us = toUs(mix(config, v).channels[c - 1]);
        return roleOf(c, CH) ? meaning(c, us, CH, L) : `${us}`;
      }),
    }));
  }
  for (const axis of ['lh', 'lv', 'rh', 'rv']) {
    const variants = [-1, -0.6, 0, 0.6, 1].map((v) => ({ ...input, sticks: { ...input.sticks, [axis]: v } }));
    out[axis] = changed(variants).map((c) => ({ ch: c, label: channelLabel(c, CH, config) }));
  }
  return out;
}

export const AXIS_NAME = {
  lh: 'Left stick sideways',
  lv: 'Left stick up/down',
  rh: 'Right stick sideways',
  rv: 'Right stick up/down',
};

export function axisSource(axis, stickMode) {
  return STICKS.find((s) => axisOf(s, stickMode) === axis);
}
