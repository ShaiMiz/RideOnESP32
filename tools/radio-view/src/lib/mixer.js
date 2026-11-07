// A small EdgeTX mixer: sticks and switches -> logical switches -> mixes -> channel outputs ->
// CRSF -> the microseconds the ESP32 reads. Enough of EdgeTX for the RideOn model and the
// edits the view allows (sources, weight, offset, ADD / MUL / REPL, a logical-switch condition).
import { SWITCHES, LS_COUNT, CHANNELS, TRIM_SWITCH } from './model.js';

const TRIM_BY_EDGETX = Object.fromEntries(Object.entries(TRIM_SWITCH).map(([k, v]) => [v, k]));

export const RESOLUTION = 1024; // EdgeTX's internal full scale

/** Physical stick axes (-1..1) -> EdgeTX stick sources, for stick mode 1 or 2. */
export function stickSources(sticks, stickMode = 2) {
  const { lh, lv, rh, rv } = sticks;
  return stickMode === 1
    ? { Rud: lh, Ele: lv, Thr: rv, Ail: rh }
    : { Rud: lh, Thr: lv, Ele: rv, Ail: rh };
}

/** Which physical axis an EdgeTX stick source sits on. */
export function axisOf(src, stickMode = 2) {
  const m = stickMode === 1
    ? { Rud: 'lh', Ele: 'lv', Thr: 'rv', Ail: 'rh' }
    : { Rud: 'lh', Thr: 'lv', Ele: 'rv', Ail: 'rh' };
  return m[src];
}

/** Switch position index (0 = up) -> EdgeTX value. */
export function switchValue(sw, pos) {
  const n = SWITCHES[sw];
  if (n === 2) return pos > 0 ? RESOLUTION : -RESOLUTION;
  return (pos - 1) * RESOLUTION;
}

function sourceValues(input) {
  const v = { MAX: RESOLUTION };
  const s = stickSources(input.sticks, input.stickMode);
  for (const k of Object.keys(s)) v[k] = Math.round(s[k] * RESOLUTION);
  for (const sw of Object.keys(SWITCHES)) v[sw] = switchValue(sw, input.switches[sw] ?? 0);
  return v;
}

function lsTrue(ls, src, lsState, trims) {
  const a = src[ls.a];
  if (a === undefined) return false;
  const x = (ls.x * RESOLUTION) / 100; // the def value is in percent for sticks and switches
  let on;
  switch (ls.func) {
    case 'FUNC_APOS': on = Math.abs(a) > x; break;
    case 'FUNC_ANEG': on = Math.abs(a) < x; break;
    case 'FUNC_VPOS': on = a > x; break;
    case 'FUNC_VNEG': on = a < x; break;
    default: on = false;
  }
  if (on && ls.andsw && ls.andsw !== 'NONE') on = conditionTrue(ls.andsw, lsState, trims);
  return on;
}

function conditionTrue(sw, lsState, trims = {}) {
  if (!sw || sw === 'NONE' || sw === 'ON') return true;
  const bare = sw.replace(/^!/, '');
  if (TRIM_BY_EDGETX[bare]) return !!trims[TRIM_BY_EDGETX[bare]] !== sw.startsWith('!');
  const neg = sw.startsWith('!');
  const m = /^!?L(\d+)$/.exec(sw);
  const on = m ? !!lsState[Number(m[1])] : false;
  return neg ? !on : on;
}

/**
 * Run the mixer. input = { sticks: {lh, lv, rh, rv} (-1..1), switches: {SA: 0..2, ...}, trims: {'T1+': true, ...} (held), stickMode }.
 * Returns { channels: [16 values, -1024..1024], ls: {1: bool, ...}, active: [bool per mix line] }.
 */
export function mix(config, input) {
  const src = sourceValues(input);
  const ls = {};
  // Logical switches in order; an AND switch may name an earlier one.
  for (const l of [...config.logical].sort((a, b) => a.idx - b.idx)) {
    if (l.idx < LS_COUNT) ls[l.idx + 1] = lsTrue(l, src, ls, input.trims);
  }
  const channels = new Array(CHANNELS).fill(0);
  const touched = new Array(CHANNELS).fill(false);
  const active = config.mixes.map(() => false);
  config.mixes.forEach((m, i) => {
    const c = m.ch - 1;
    if (c < 0 || c >= CHANNELS) return;
    if (!conditionTrue(m.swtch, ls, input.trims)) return;
    active[i] = true;
    const v = ((src[m.src] ?? 0) * m.weight) / 100 + ((m.offset || 0) * RESOLUTION) / 100;
    if (m.mltpx === 'REPL' || !touched[c]) channels[c] = m.mltpx === 'MUL' && !touched[c] ? 0 : v;
    else if (m.mltpx === 'MUL') channels[c] = (channels[c] * v) / RESOLUTION;
    else channels[c] += v;
    touched[c] = true;
  });
  for (let c = 0; c < CHANNELS; c++) {
    channels[c] = Math.max(-RESOLUTION, Math.min(RESOLUTION, Math.round(channels[c])));
  }
  return { channels, ls, active, sources: src };
}

/** EdgeTX channel value -> CRSF ticks (172..1811), as the CRSF driver packs it. */
export function toCrsf(v) {
  return Math.max(0, Math.min(2047, Math.trunc((v * 4) / 5) + 992));
}

/** CRSF ticks -> microseconds, as AlfredoCRSF hands them to the firmware. */
export function crsfToUs(ticks) {
  return Math.trunc(((ticks - 992) * 5) / 8) + 1500;
}

export const toUs = (v) => crsfToUs(toCrsf(v));
