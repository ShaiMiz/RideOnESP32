// Checks that the viewer's model covers the project records: every BOM ref is drawn, every
// wire in WIRE_LIST.csv joins two drawn parts, and variant B swaps the drive boards.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { buildModel } from '../src/data/model.js';
import { explodeDirections } from '../src/data/explode.js';

const data = JSON.parse(readFileSync(new URL('../src/data/project.json', import.meta.url), 'utf8'));

test('every BOM ref becomes a part, placed by layout.js', () => {
  const m = buildModel(data, 'A');
  const ids = new Set(m.parts.map((p) => p.id));
  for (const ref of Object.keys(data.partIndex)) {
    if (data.variantB.some((b) => b.ref === ref)) continue;
    assert.ok(ids.has(ref), `${ref} missing`);
  }
  const unplaced = m.parts.filter((p) => p.assembly === 'unplaced').map((p) => p.id);
  assert.deepEqual(unplaced, [], `parts with no layout: ${unplaced.join(' ')}`);
});

test('every wire joins two parts in variant A', () => {
  const m = buildModel(data, 'A');
  assert.equal(m.connections.length, data.wires.length);
});

test('variant B removes the drive IBT-2s and wires the MDD20As by net', () => {
  const m = buildModel(data, 'B');
  const ids = new Set(m.parts.map((p) => p.id));
  for (const r of ['U10', 'U11', 'U12', 'U13']) assert.ok(!ids.has(r));
  for (const r of ['U14', 'U41', 'U42', 'U43']) assert.ok(ids.has(r));
  const fl = m.connections.filter((w) => w.from.ref === 'U41' || w.to.ref === 'U41');
  assert.ok(fl.some((w) => w.net === 'FL_M_NEG' && w.to.ref === 'J6'), 'U41 M1B should run to J6');
  assert.ok(m.connections.some((w) => w.net === 'FL_RPWM' && w.to.ref === 'U40'), 'ESP32 FL_RPWM should feed the PWM gate');
});

test('hiding a box tier closes its gap', () => {
  const m = buildModel(data, 'A');
  const all = explodeDirections(m.parts, new Set());
  const noDrivers = explodeDirections(m.parts, new Set(m.parts.filter((p) => p.assembly === 'box.drive').map((p) => p.id)));
  assert.ok(noDrivers.U1[1] < all.U1[1]);
});

test('car-facing plugs are flagged TBD', () => {
  const m = buildModel(data, 'A');
  for (const j of ['J1', 'J5', 'J8']) assert.ok(m.parts.find((p) => p.id === j).tbd.length > 0, j);
});
