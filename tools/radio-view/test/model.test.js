import test from 'node:test';
import assert from 'node:assert/strict';
import { MODEL_YML, CHANNEL_MAP_H, CONFIG_H } from './helpers.js';
import { parse, stringify } from '../src/lib/edgetxYaml.js';
import { load, toYaml } from '../src/lib/model.js';
import { mix, toUs } from '../src/lib/mixer.js';
import { channelNumbers, configValues } from '../src/lib/repo.js';
import { decode, ModeMachine, switchPosition } from '../src/lib/firmware.js';
import { check } from '../src/lib/rules.js';

const ch = channelNumbers(CHANNEL_MAP_H);
const lim = configValues(CONFIG_H);
const { tree, config } = load(MODEL_YML);
const rest = { lh: 0, lv: 0, rh: 0, rv: 0 };
const start = { SA: 0, SB: 0, SC: 0, SD: 0 };
const run = (sticks = rest, switches = start) => mix(config, { sticks, switches, stickMode: 2 }).channels.map(toUs);

test('the model file reads and writes back byte for byte', () => {
  assert.equal(stringify(parse(MODEL_YML)), MODEL_YML);
  assert.equal(toYaml(tree, config), MODEL_YML);
});

test('the parser refuses comments and tabs, as EdgeTX does', () => {
  assert.throws(() => parse('a: 1\n# no\n'), /comment/);
  assert.throws(() => parse('a:\n\tb: 1\n'), /tab/);
});

test('the repo model drives the channels CHANNEL_MAP.md names', () => {
  const us = run();
  assert.ok(us[ch.KILL - 1] < 1100, 'SC up = KILL');
  assert.equal(switchPosition(us[ch.MODE - 1], lim), 0, 'SA up = Locked');
  assert.equal(switchPosition(us[ch.SPEED_CAP - 1], lim), 0, 'SB up = 30 %');
  assert.ok(us[ch.HORN - 1] < 1100, 'horn off');
  assert.equal(us[ch.STEERING - 1], 1500);
  assert.equal(us[ch.THROTTLE - 1], 1500);
  const down = run(rest, { SA: 2, SB: 1, SC: 1, SD: 0 });
  assert.ok(down[ch.KILL - 1] > 1900, 'SC down = run');
  assert.equal(switchPosition(down[ch.MODE - 1], lim), 2, 'SA down = Parent');
  assert.equal(switchPosition(down[ch.SPEED_CAP - 1], lim), 1, 'SB middle = 60 %');
  assert.ok(run({ ...rest, rv: 1 })[ch.THROTTLE - 1] > 2000, 'right stick up = forward');
  assert.ok(run({ ...rest, rh: -1 })[ch.STEERING - 1] < 1000, 'right stick left = left');
  assert.ok(run({ ...rest, lh: 0.6 })[ch.HORN - 1] > 1900, 'left stick sideways = horn');
  assert.ok(run({ ...rest, lh: -0.6 })[ch.HORN - 1] > 1900, 'either way');
  assert.ok(run({ ...rest, lh: 0.4 })[ch.HORN - 1] < 1100, 'not before half way');
  assert.equal(run({ ...rest, lv: 1 })[ch.THROTTLE - 1], 1500, 'left stick up/down drives nothing');
});

test('CRSF output range is 989..2011 us, centre 1500', () => {
  assert.equal(toUs(-1024), 989);
  assert.equal(toUs(0), 1500);
  assert.equal(toUs(1024), 2011);
});

test('the repo model passes every safety check', () => {
  const r = check(config, ch, lim);
  assert.deepEqual(r.errors, []);
  assert.deepEqual(r.warnings, []);
  assert.deepEqual(r.killers.map((k) => `${k.sw}${k.p}`), ['SC0']);
});

const edit = (fn) => {
  const c = structuredClone(config);
  fn(c);
  return check(c, ch, lim).errors.join('\n');
};

test('safety checks block the dangerous edits', () => {
  const kill = (c) => c.mixes.find((m) => m.ch === ch.KILL);
  const thr = (c) => c.mixes.find((m) => m.ch === ch.THROTTLE);
  assert.match(edit((c) => { kill(c).weight = -100; }), /Start-up check.*not KILL/, 'inverted kill');
  assert.match(edit((c) => { kill(c).src = 'SA'; c.mixes.find((m) => m.ch === ch.MODE).src = 'SC'; }), /3-position/, 'kill on a 3-pos switch');
  assert.match(edit((c) => { c.mixes = c.mixes.filter((m) => m.ch !== ch.KILL); }), /no switch position kills/, 'kill cleared');
  assert.match(edit((c) => { thr(c).src = 'Thr'; }), /not centred|does not spring back/, 'throttle on the left stick');
  assert.match(edit((c) => { thr(c).offset = 10; }), /not centred/, 'throttle offset');
  assert.match(edit((c) => { c.switchWarning.SC = ''; }), /Start-up check/, 'no start-up check on kill');
  assert.match(edit((c) => { kill(c).src = 'Ail'; }), /not from a stick|no switch position kills/, 'kill on a stick');
  assert.equal(edit((c) => { c.mixes.find((m) => m.ch === ch.SPEED_CAP).src = 'SD'; }), '', 'moving the speed cap is fine');
  assert.equal(edit((c) => { kill(c).src = 'SD'; c.switchWarning.SD = 'u'; delete c.switchWarning.SC; }), '', 'kill on SD with its start-up check is fine');
});

test('mode 1 puts the repo throttle on the non-centring stick, and the check says so', () => {
  const r = check(config, ch, lim, 1);
  assert.ok(r.errors.some((e) => /does not spring back|not centred/.test(e)));
});

test('an edit writes a model the reader takes back unchanged', () => {
  const c = structuredClone(config);
  c.mixes.push({ ch: 9, src: 'SD', weight: 100, offset: 0, swtch: 'NONE', noTrim: false, mltpx: 'ADD', name: 'Lights' });
  c.logical[0].x = 40;
  const text = toYaml(tree, c);
  assert.match(text, /destCh: 8\n    srcRaw: SD/);
  assert.match(text, /def: "Rud,40"/);
  const again = load(text);
  assert.equal(toYaml(again.tree, again.config), text);
});

test('the firmware arms only after Locked and centred sticks, and kill wins', () => {
  const m = new ModeMachine();
  const link = { up: true, lq: 100 };
  const step = (sticks, sw) => decode([0, ...run(sticks, sw)], ch, lim, link, m);
  assert.equal(step(rest, { SA: 2, SB: 0, SC: 1, SD: 0 }).mode, 'LOCKED', 'Parent at boot without Locked first');
  assert.equal(step(rest, { SA: 0, SB: 0, SC: 1, SD: 0 }).mode, 'LOCKED');
  assert.equal(step({ ...rest, rv: 0.5 }, { SA: 2, SB: 0, SC: 1, SD: 0 }).mode, 'LOCKED', 'throttle not centred');
  assert.equal(step(rest, { SA: 2, SB: 0, SC: 1, SD: 0 }).mode, 'PARENT');
  const d = step({ ...rest, rv: 1 }, { SA: 2, SB: 0, SC: 1, SD: 0 });
  assert.equal(d.mode, 'PARENT');
  assert.ok(d.command > 0.99);
  assert.ok(step({ ...rest, rv: -1 }, { SA: 2, SB: 0, SC: 1, SD: 0 }).command >= -lim.reverseCap - 1e-9, 'reverse capped');
  assert.equal(step(rest, { SA: 2, SB: 0, SC: 0, SD: 0 }).mode, 'KILLED');
  assert.equal(step(rest, { SA: 2, SB: 0, SC: 1, SD: 0 }).mode, 'LOCKED', 'kill released: Locked, not straight back');
  const lost = decode([0, ...run(rest, { SA: 2, SB: 0, SC: 1, SD: 0 })], ch, lim, { up: false, lq: 0 }, m);
  assert.equal(lost.mode, 'LOCKED');
});
