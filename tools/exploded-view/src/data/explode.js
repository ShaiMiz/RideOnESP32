// Exploded-layout maths. For each part: position = origin + factor * direction, where the
// direction sums the part's assembly vectors, its own vector, a radial spread inside the box,
// and its tier lift. Tiers are ranked among the visible parts only, so hiding a whole tier
// (say, the motor drivers) lets the tiers above it settle down into the gap.
import { ASSEMBLIES, BOX, TIER_GAP } from './layout.js';

const byId = Object.fromEntries(ASSEMBLIES.map((a) => [a.id, a]));

function chain(id) {
  const out = [];
  for (let a = byId[id]; a; a = a.parent ? byId[a.parent] : null) out.unshift(a);
  return out;
}

const add = (a, b, k = 1) => [a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k];

export function explodeDirections(parts, hidden) {
  const visible = parts.filter((p) => !hidden.has(p.id));
  // visible tiers per tiered root assembly
  const tiers = {};
  for (const p of visible) {
    const root = chain(p.assembly)[0];
    if (!root?.tiers || typeof p.tier !== 'number' || p.tier < 0) continue;
    (tiers[root.id] ||= new Set()).add(p.tier);
  }
  const rank = Object.fromEntries(Object.entries(tiers).map(([k, s]) => [k, [...s].sort((a, b) => a - b)]));

  const out = {};
  for (const p of parts) {
    const ch = chain(p.assembly);
    let d = [0, 0, 0];
    for (const a of ch) if (a.explode) d = add(d, a.explode);
    const root = ch[0];
    if (root?.tiers && p.tier !== null && p.tier !== undefined) {
      const list = rank[root.id] || [];
      let lift = 0;
      if (p.tier === 'top') lift = (list.length + 1) * TIER_GAP;
      else if (p.tier >= 0) lift = (list.indexOf(p.tier) + 1) * TIER_GAP;
      d = add(d, [0, lift, 0]);
      if (!p.explode && typeof p.tier === 'number' && p.tier >= 0) {
        // spread the parts of a tier apart around the box centre
        d = add(d, [(p.position[0] - BOX.center[0]) * 0.9, 0, (p.position[2] - BOX.center[2]) * 0.9]);
      }
    }
    if (p.explode) d = add(d, p.explode);
    out[p.id] = d;
  }
  return out;
}

export function explodedPosition(part, dir, factor, offset) {
  const o = offset || [0, 0, 0];
  return [part.position[0] + dir[0] * factor + o[0], part.position[1] + dir[1] * factor + o[1], part.position[2] + dir[2] * factor + o[2]];
}
