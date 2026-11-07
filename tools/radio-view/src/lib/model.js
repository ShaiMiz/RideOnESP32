// The RideOn EdgeTX model as an editable config, and back to a model file.
//
// Only the parts the view edits are taken apart: mixes (mixData), logical switches (logicalSw)
// and the start-up switch check (switchWarningState). Everything else in the file (module,
// telemetry sensors, screens, trims, special functions) is carried through untouched.
import { parse, stringify, get, value, setKey, removeKey, scalar, quoted, map, clone } from './edgetxYaml.js';

export const STICKS = ['Rud', 'Ele', 'Thr', 'Ail'];
export const SWITCHES = { SA: 3, SB: 3, SC: 2, SD: 2 }; // positions, from EdgeTX hw_defs/tlite.json
export const SOURCES = [...STICKS, ...Object.keys(SWITCHES), 'MAX'];
export const MLTPX = ['ADD', 'MUL', 'REPL'];
export const LS_FUNCS = {
  FUNC_APOS: '|a| > x',
  FUNC_ANEG: '|a| < x',
  FUNC_VPOS: 'a > x',
  FUNC_VNEG: 'a < x',
};
export const LS_COUNT = 8;
export const CHANNELS = 16; // the CRSF frame carries 16
export const NAME_LEN = 6; // EdgeTX mix name length on 128x64 radios

export const SOURCE_LABEL = {
  Rud: 'Rudder (left stick sideways in mode 2)',
  Ele: 'Elevator (right stick up/down in mode 2)',
  Thr: 'Throttle (left stick up/down in mode 2, does not spring back)',
  Ail: 'Aileron (right stick sideways in mode 2)',
  SA: 'SA, left 3-position',
  SB: 'SB, right 3-position',
  SC: 'SC, left 2-position',
  SD: 'SD, right 2-position',
  MAX: 'MAX (a fixed +100 %)',
};

export function conditions() {
  const out = ['NONE'];
  for (let i = 1; i <= LS_COUNT; i++) out.push(`L${i}`, `!L${i}`);
  return out;
}

/** model YAML text -> { tree, config } */
export function load(text) {
  const tree = parse(text);
  return { tree, config: toConfig(tree) };
}

export function toConfig(tree) {
  const mixes = (get(tree, 'mixData')?.items || []).map((m) => ({
    ch: value(get(m, 'destCh')) + 1,
    src: value(get(m, 'srcRaw')),
    weight: value(get(m, 'weight')) ?? 100,
    offset: value(get(m, 'offset')) ?? 0,
    swtch: value(get(m, 'swtch')) ?? 'NONE',
    noTrim: value(get(m, 'carryTrim')) === 1,
    mltpx: value(get(m, 'mltpx')) ?? 'ADD',
    name: value(get(m, 'name')) ?? '',
  }));
  const logical = [];
  for (const [k, ls] of get(tree, 'logicalSw')?.entries || []) {
    const [a, x] = String(value(get(ls, 'def')) ?? '').split(',');
    logical.push({
      idx: Number(k),
      func: value(get(ls, 'func')),
      a,
      x: Number(x) || 0,
      andsw: value(get(ls, 'andsw')) ?? 'NONE',
      delay: value(get(ls, 'delay')) ?? 0,
      duration: value(get(ls, 'duration')) ?? 0,
    });
  }
  const warn = String(value(get(tree, 'switchWarningState')) ?? '');
  const switchWarning = {};
  for (const m of warn.matchAll(/([A-D])([udm])/g)) switchWarning[`S${m[1]}`] = m[2];
  const trims = get(tree, 'flightModeData', 0, 'trim');
  return {
    name: value(get(tree, 'header', 'name')),
    receiver: value(get(tree, 'header', 'modelId', 0, 'val')),
    mixes,
    logical,
    switchWarning,
    trimsOff: [0, 1, 2, 3].every((i) => value(get(trims, i, 'mode')) === 31),
    module: value(get(tree, 'moduleData', 0, 'type')),
  };
}

function mixNode(m) {
  const e = [
    ['destCh', scalar(m.ch - 1)],
    ['srcRaw', scalar(m.src)],
    ['weight', scalar(m.weight)],
  ];
  if (m.swtch && m.swtch !== 'NONE') e.push(['swtch', scalar(m.swtch)]);
  if (m.noTrim) e.push(['carryTrim', scalar(1)]);
  if (m.offset) e.push(['offset', scalar(m.offset)]);
  e.push(['mltpx', scalar(m.mltpx)]);
  if (m.name) e.push(['name', quoted(m.name.slice(0, NAME_LEN))]);
  return map(e);
}

function lsNode(ls) {
  return map([
    ['func', scalar(ls.func)],
    ['def', quoted(`${ls.a},${ls.x}`)],
    ['andsw', quoted(ls.andsw || 'NONE')],
    ['delay', scalar(ls.delay || 0)],
    ['duration', scalar(ls.duration || 0)],
  ]);
}

/** Write config back into a copy of the original tree. Unchanged config gives the same text. */
export function toTree(baseTree, config) {
  const tree = clone(baseTree);
  const mixes = [...config.mixes].sort((a, b) => a.ch - b.ch); // EdgeTX keeps them by channel
  setKey(tree, 'mixData', { t: 'list', items: mixes.map(mixNode) });
  if (!mixes.length) removeKey(tree, 'mixData');
  const ls = [...config.logical].filter((l) => l.func).sort((a, b) => a.idx - b.idx);
  if (ls.length) setKey(tree, 'logicalSw', map(ls.map((l) => [String(l.idx), lsNode(l)])));
  else removeKey(tree, 'logicalSw');
  const warn = Object.entries(config.switchWarning)
    .filter(([, p]) => p)
    .map(([sw, p]) => `${sw[1]}${p}`)
    .join('');
  if (warn) setKey(tree, 'switchWarningState', scalar(warn));
  else removeKey(tree, 'switchWarningState');
  return tree;
}

export function toYaml(baseTree, config) {
  return stringify(toTree(baseTree, config));
}

export function mixesFor(config, ch) {
  return config.mixes.filter((m) => m.ch === ch);
}
