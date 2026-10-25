// Turns project.json (BOM, wire list, design nets) plus layout.js into the part and connection
// lists the viewer draws. Pure data in, pure data out, so the JSON export and the tests use the
// same thing the canvas does.
import { buildPrimitives } from '../geometry/builders.js';
import { ASSEMBLIES, LAYOUT, COMPACT, A1, A1_C, ON_A1, VARIANT_A_ONLY, RAIL_ANCHOR, TYPE_BY_KIND, KIND_BY_PREFIX } from './layout.js';

const ISSUE_RE = /#(\d+)/g;
const issuesIn = (...t) => [...new Set(t.join(' ').match(ISSUE_RE) || [])].map((s) => s.slice(1));

export const VARIANTS = {
  A: { id: 'A', name: 'Variant A: 4 + 1 IBT-2', blurb: 'One IBT-2 per wheel plus the steering IBT-2 (main design).' },
  B: { id: 'B', name: 'Variant B: 2 x MDD20A', blurb: '2 x Cytron MDD20A with a 74LVC08 PWM gate and external current sensors. Undecided (#32).' },
  C: { id: 'C', name: 'Compact: 2 x MDD20A, split boxes', blurb: 'Variant B drive in a ~18 x 12 x 6 cm main box; the contactor sits at the battery and the extras go to a small accessory box. Sizes are estimates until #18.' },
};

function bomFor(data, ref) {
  const idx = data.partIndex[ref];
  return idx ? data.bom[idx.bomIndex] : null;
}

function tbdReasons(...texts) {
  const out = [];
  for (const t of texts) if (t && /TBD/.test(t)) out.push(t);
  return out;
}

// Place parts that live on the A1 board on a grid over it.
function boardSlots(refs, board) {
  const cols = 8;
  const [w, d] = board.size;
  return Object.fromEntries(refs.map((r, i) => {
    const c = i % cols;
    const row = Math.floor(i / cols);
    return [r, [board.pos[0] - w / 2 + 0.008 + c * ((w - 0.03) / (cols - 1)), board.pos[1] + 0.0016, board.pos[2] - d / 2 + 0.014 + row * 0.011]];
  }));
}

export function buildModel(data, variant = 'A') {
  // variant C (compact) uses variant B's drive parts and wiring, packed differently
  const drive = variant === 'A' ? 'A' : 'B';
  const layout = variant === 'C' ? { ...LAYOUT, ...COMPACT } : LAYOUT;
  const designB = Object.fromEntries((data.variantB || []).map((p) => [p.ref, p]));
  const removed = new Set(drive === 'B' ? VARIANT_A_ONLY : []);
  const refs = new Set(Object.keys(data.partIndex));
  for (const w of data.wires) for (const e of [w.from, w.to]) if (/^[A-Z]+\d+$/.test(e.ref)) refs.add(e.ref);
  if (drive === 'B') Object.keys(designB).forEach((r) => refs.add(r));
  else Object.keys(designB).forEach((r) => refs.delete(r));
  removed.forEach((r) => refs.delete(r));

  const parts = [];

  // procedural stock body parts and the box shell
  for (const [id, lay] of Object.entries(layout)) {
    if (!id.includes('.') && !/^BOX\d?$/.test(id)) continue;
    let bom = null;
    if (lay.match) bom = data.bom.find((b) => lay.match.test(b.part)) || null;
    parts.push(makePart(data, id, lay, bom, null, bom ? 'bom' : 'procedural'));
  }

  // board parts without a hand-placed spot
  const boardRefs = [...refs].filter((r) => !layout[r] && ((designB[r]?.where || data.design[r]?.where || bomFor(data, r)?.where) === 'A1')).sort(naturalRef);
  const slots = boardSlots(boardRefs, variant === 'C' ? A1_C : A1);

  let unplaced = 0;
  for (const ref of [...refs].sort(naturalRef)) {
    let lay = layout[ref];
    if (!lay && slots[ref]) lay = { kind: KIND_BY_PREFIX[ref[0]] || 'module', assembly: 'box.logic', pos: slots[ref], tier: 3, onBoard: ON_A1 };
    if (!lay) {
      lay = { kind: KIND_BY_PREFIX[ref[0]] || 'module', assembly: 'unplaced', pos: [-1.05 - 0.08 * (unplaced % 4), 0.02, -0.4 + 0.12 * Math.floor(unplaced / 4)], unplaced: true };
      unplaced++;
    }
    const bom = bomFor(data, ref);
    parts.push(makePart(data, ref, lay, bom, designB[ref] || data.design[ref] || null, designB[ref] ? 'variantB' : bom ? 'bom' : 'wireList'));
  }

  const connections = buildConnections(data, drive, removed, designB, new Set(parts.map((p) => p.id)));
  return { parts, connections, assemblies: ASSEMBLIES, variant: VARIANTS[variant] };
}

function makePart(data, id, lay, bom, design, source) {
  const kind = lay.kind;
  const params = lay.params || {};
  const name = lay.name || bom?.part || design?.part || (id === 'LP1' ? 'Stock lights' : id);
  const status = bom?.status || design?.source || (source === 'procedural' ? 'Stock car' : '');
  const note = bom?.note || design?.note || '';
  const tbd = [...(lay.tbd || []), ...tbdReasons(bom?.part, bom?.model && `Part number: ${bom.model}`, bom?.status === 'TBD' ? `Status TBD${note ? ': ' + note : ''}` : '')];
  if (lay.unplaced) tbd.push('No 3D placement yet: add it to src/data/layout.js');
  const primitives = buildPrimitives(kind, params);
  return {
    id,
    ref: /^[A-Z]+\d+$/.test(id) ? id : null,
    name,
    model: bom?.model || design?.part || '',
    category: bom?.category || '',
    type: lay.type || TYPE_BY_KIND[kind] || 'Part',
    usage: lay.usage || note || '',
    kind,
    params,
    assembly: lay.assembly,
    position: lay.pos,
    rotation: lay.rot || [0, 0, 0],
    explode: lay.explode || null,
    tier: lay.tier ?? null,
    source,
    status,
    where: design?.where || bom?.where || '',
    qty: bom?.qty || '',
    bomLine: bom?.line || null,
    bom: bom ? bom.fields : null,
    value: design?.value || '',
    pins: design?.pins || null,
    sheet: design?.sheet || '',
    issues: issuesIn(bom?.part || '', bom?.model || '', note, bom?.status || '', design?.issue || '', ...(lay.tbd || [])),
    tbd: [...new Set(tbd)],
    badge: lay.badge ?? true,
    primitives,
    radius: approxRadius(primitives),
  };
}

// Rough bounding radius of a primitive list, for label and wire-end placement.
export function approxRadius(prims) {
  let r = 0.005;
  for (const p of prims) {
    const c = Math.hypot(...(p.p || [0, 0, 0]));
    let e = 0;
    if (p.t === 'box') e = Math.hypot(...p.a) / 2;
    else if (p.t === 'cylinder') e = Math.hypot(Math.max(p.a[0], p.a[1]), p.a[2] / 2);
    else if (p.t === 'tube') e = Math.max(...p.pts.map((v) => Math.hypot(...v)));
    else if (p.t === 'extrude') e = Math.max(...p.shape.map((v) => Math.hypot(...v))) + p.a[0];
    else if (p.t === 'lathe') e = Math.max(...p.pts.map((v) => Math.hypot(...v)));
    else e = p.a?.[1] && p.t === 'gear' ? p.a[1] : p.a?.[0] || 0.01;
    r = Math.max(r, c + e);
  }
  return r;
}

// Wires from WIRE_LIST.csv, direction as the list writes it (From = upstream, To = downstream).
// In variant B, wires to the removed IBT-2 parts go and the variant B pins are wired by net.
export function buildConnections(data, variant, removed, designB, present) {
  const wires = data.wires
    .filter((w) => !removed.has(w.from.ref) && !removed.has(w.to.ref))
    .map((w) => ({ ...w, synthetic: false }));
  if (variant !== 'B') return wires.filter((w) => present.has(w.from.ref) && present.has(w.to.ref));

  // net -> endpoints in the full wire list that survive the swap (first seen first)
  const onNet = {};
  for (const w of data.wires) for (const e of [w.from, w.to]) {
    if (designB[e.ref] || removed.has(e.ref)) continue;
    (onNet[w.net] ||= []).push(e);
  }
  const bPins = {};
  for (const p of Object.values(designB)) for (const [pin, net] of Object.entries(p.pins)) (bPins[net] ||= []).push({ ref: p.ref, pin });

  let n = 1;
  const add = (from, to, net, note) => wires.push({ id: `B${String(n++).padStart(3, '0')}`, from, to, net, awg: '', colour: '', protectedBy: '', route: 'box', note, synthetic: true });
  const done = new Set();
  for (const [net, ends] of Object.entries(bPins)) {
    const rail = RAIL_ANCHOR[net];
    if (rail) { ends.forEach((e) => add(rail, e, net, 'Variant B, by net (rail pick-up)')); continue; }
    // chain the variant B pins on this net
    for (let i = 1; i < ends.length; i++) add(ends[i - 1], ends[i], net, 'Variant B, by net');
    const outside = (onNet[net] || []).find((e) => !removed.has(e.ref));
    if (outside && !done.has(net)) {
      done.add(net);
      const bDrives = /^[JM]/.test(outside.ref) || ends[0].pin === 'OUT';
      if (bDrives) add(ends[0], outside, net, 'Variant B, by net');
      else add(outside, ends[0], net, 'Variant B, by net');
    }
  }
  return wires.filter((w) => present.has(w.from.ref) && present.has(w.to.ref));
}

export function naturalRef(a, b) {
  const pa = a.match(/^([A-Z]+)(\d+)$/);
  const pb = b.match(/^([A-Z]+)(\d+)$/);
  if (!pa || !pb) return a.localeCompare(b);
  return pa[1] === pb[1] ? Number(pa[2]) - Number(pb[2]) : pa[1].localeCompare(pb[1]);
}

// Upstream (wires that end here) and downstream (wires that start here), plus nets for
// parts that are soldered rather than wired (most of the A1 board).
export function partConnections(model, data, part) {
  const pinInfo = (e) => (e.ref === 'U20' && data.esp32Pins[e.pin] ? data.esp32Pins[e.pin] : null);
  const upstream = model.connections.filter((w) => w.to.ref === part.id).map((w) => ({ wire: w, other: w.from, pin: w.to, esp: pinInfo(w.to) || pinInfo(w.from) }));
  const downstream = model.connections.filter((w) => w.from.ref === part.id).map((w) => ({ wire: w, other: w.to, pin: w.from, esp: pinInfo(w.from) || pinInfo(w.to) }));
  let nets = [];
  if (part.pins) {
    nets = Object.entries(part.pins).map(([pin, net]) => {
      const peers = new Set();
      for (const p of model.parts) if (p.id !== part.id && p.pins && Object.values(p.pins).includes(net)) peers.add(p.id);
      return { pin, net, peers: [...peers].sort(naturalRef) };
    });
  }
  return { upstream, downstream, nets };
}

export function assemblyChain(id) {
  const out = [];
  let a = ASSEMBLIES.find((x) => x.id === id);
  while (a) { out.unshift(a); a = a.parent ? ASSEMBLIES.find((x) => x.id === a.parent) : null; }
  return out;
}
