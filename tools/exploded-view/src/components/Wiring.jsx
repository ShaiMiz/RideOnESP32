import React, { useEffect, useMemo, useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';

// Wire colour codes from WIRE_LIST.md.
export const WIRE_COLOURS = {
  RD: '#d62828', BK: '#2b2b2b', OG: '#f77f00', YE: '#f2c200', BU: '#1e6bd6', BN: '#8b5a2b', WH: '#f4f4f4',
  GY: '#9a9a9a', GN: '#2a9d4b', VT: '#8e44ad', 'RD/YE': '#ff6a00', 'BK/WH': '#6d6d6d', stock: '#b7b7b7', '': '#c77dff',
};
export const COLOUR_NAMES = { RD: 'red', BK: 'black', OG: 'orange', YE: 'yellow', BU: 'blue', BN: 'brown', WH: 'white', GY: 'grey', GN: 'green', VT: 'violet', 'RD/YE': 'red/yellow', 'BK/WH': 'black/white', stock: 'stock harness' };

const SEG = 14;
const a = new THREE.Vector3();
const b = new THREE.Vector3();
const c = new THREE.Vector3();
const q = new THREE.Vector3();

// Small fixed offset per pin so wires into one part don't all meet at one point.
function pinOffset(pin, r) {
  let h = 0;
  for (const ch of pin) h = (h * 31 + ch.charCodeAt(0)) >>> 0;
  const t = (h % 360) * (Math.PI / 180);
  const k = r * 0.35;
  return [Math.cos(t) * k, ((h >> 9) % 100) / 100 * k * 0.5, Math.sin(t) * k];
}

// All visible wires as one line set, re-routed every frame from the parts' live positions:
// a curve from Point A (upstream) to Point B (downstream), lifted in the middle.
export function Wiring({ connections, registry, selectedId, mode, visualStyle, groupRef }) {
  const shown = useMemo(() => {
    if (mode === 'none') return [];
    if (mode === 'selected') return connections.filter((w) => w.from.ref === selectedId || w.to.ref === selectedId);
    return connections;
  }, [connections, mode, selectedId]);

  const geo = useMemo(() => {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(Math.max(1, shown.length) * SEG * 6), 3));
    g.setAttribute('color', new THREE.BufferAttribute(new Float32Array(Math.max(1, shown.length) * SEG * 6), 3));
    return g;
  }, [shown]);
  useEffect(() => () => geo.dispose(), [geo]);

  const colours = useMemo(() => {
    const col = new THREE.Color();
    const out = new Float32Array(shown.length * SEG * 6);
    shown.forEach((w, i) => {
      col.set(WIRE_COLOURS[w.colour] ?? WIRE_COLOURS['']);
      if (visualStyle === 'blueprint' && (w.colour === 'BK' || w.colour === 'BK/WH')) col.set('#7f8fa6');
      const dim = selectedId && mode === 'all' && w.from.ref !== selectedId && w.to.ref !== selectedId;
      if (dim) col.lerp(new THREE.Color(visualStyle === 'blueprint' ? '#0c2340' : '#e9edf1'), 0.8);
      for (let k = 0; k < SEG * 2; k++) out.set([col.r, col.g, col.b], (i * SEG * 2 + k) * 3);
    });
    return out;
  }, [shown, selectedId, mode, visualStyle]);

  useEffect(() => {
    geo.attributes.color.array.set(colours);
    geo.attributes.color.needsUpdate = true;
  }, [geo, colours]);

  useFrame(() => {
    const pos = geo.attributes.position.array;
    let n = 0;
    for (const w of shown) {
      const ra = registry.get(w.from.ref);
      const rb = registry.get(w.to.ref);
      if (!ra || !rb) {
        for (let k = 0; k < SEG * 6; k++) pos[n++] = 0;
        continue;
      }
      a.copy(ra.pos).add(q.set(...pinOffset(w.from.pin, ra.radius)));
      b.copy(rb.pos).add(q.set(...pinOffset(w.to.pin, rb.radius)));
      const d = a.distanceTo(b);
      c.copy(a).add(b).multiplyScalar(0.5);
      c.y += 0.02 + d * 0.22;
      let px = a.x, py = a.y, pz = a.z;
      for (let s = 1; s <= SEG; s++) {
        const t = s / SEG;
        const u = 1 - t;
        const x = u * u * a.x + 2 * u * t * c.x + t * t * b.x;
        const y = u * u * a.y + 2 * u * t * c.y + t * t * b.y;
        const z = u * u * a.z + 2 * u * t * c.z + t * t * b.z;
        pos[n++] = px; pos[n++] = py; pos[n++] = pz;
        pos[n++] = x; pos[n++] = y; pos[n++] = z;
        px = x; py = y; pz = z;
      }
    }
    geo.attributes.position.needsUpdate = true;
    geo.computeBoundingSphere();
  });

  return (
    <group ref={groupRef} name="Wiring (WIRE_LIST.csv)">
      <lineSegments geometry={geo} raycast={() => null} frustumCulled={false}>
        <lineBasicMaterial vertexColors transparent opacity={visualStyle === 'blueprint' ? 0.9 : 0.95} />
      </lineSegments>
    </group>
  );
}

// Faint dashed lines from where each part sits assembled to where it is now.
export function ProjectionLines({ ids, registry, visualStyle }) {
  const ref = useRef();
  const geo = useMemo(() => {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(Math.max(1, ids.length) * 6), 3));
    return g;
  }, [ids]);
  useEffect(() => () => geo.dispose(), [geo]);
  useFrame(() => {
    const pos = geo.attributes.position.array;
    let n = 0;
    for (const id of ids) {
      const r = registry.get(id);
      if (!r || r.pos.distanceToSquared(r.origin) < 0.0004) { for (let k = 0; k < 6; k++) pos[n++] = 0; continue; }
      pos[n++] = r.origin.x; pos[n++] = r.origin.y; pos[n++] = r.origin.z;
      pos[n++] = r.pos.x; pos[n++] = r.pos.y; pos[n++] = r.pos.z;
    }
    geo.attributes.position.needsUpdate = true;
    geo.computeBoundingSphere();
    ref.current?.computeLineDistances();
  });
  return (
    <lineSegments ref={ref} geometry={geo} raycast={() => null} frustumCulled={false} name="Disassembly paths">
      <lineDashedMaterial color={visualStyle === 'blueprint' ? '#9cc7ff' : '#5b6675'} dashSize={0.012} gapSize={0.01} transparent opacity={visualStyle === 'blueprint' ? 0.45 : 0.35} />
    </lineSegments>
  );
}
