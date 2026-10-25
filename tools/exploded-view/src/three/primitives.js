// Primitive data (see geometry/builders.js) -> cached three.js geometry.
import * as THREE from 'three';

const cache = new Map();

function gearShape(teeth, ro, rr, bore) {
  const s = new THREE.Shape();
  const step = (2 * Math.PI) / teeth;
  for (let i = 0; i < teeth; i++) {
    const a = i * step;
    const pts = [[rr, a], [ro, a + step * 0.15], [ro, a + step * 0.45], [rr, a + step * 0.6]];
    pts.forEach(([r, t], j) => {
      const x = Math.cos(t) * r;
      const y = Math.sin(t) * r;
      if (i === 0 && j === 0) s.moveTo(x, y);
      else s.lineTo(x, y);
    });
  }
  s.closePath();
  if (bore > 0) {
    const h = new THREE.Path();
    h.absarc(0, 0, bore, 0, Math.PI * 2, true);
    s.holes.push(h);
  }
  return s;
}

function make(p) {
  const a = p.a || [];
  switch (p.t) {
    case 'box': return new THREE.BoxGeometry(...a);
    case 'cylinder': return new THREE.CylinderGeometry(a[0], a[1], a[2], a[3] || 24);
    case 'sphere': return new THREE.SphereGeometry(...a);
    case 'torus': return new THREE.TorusGeometry(...a);
    case 'cone': return new THREE.ConeGeometry(...a);
    case 'tube': {
      const curve = new THREE.CatmullRomCurve3(p.pts.map((v) => new THREE.Vector3(...v)), !!p.closed);
      return new THREE.TubeGeometry(curve, Math.max(8, p.pts.length * 16), a[0], 10, !!p.closed);
    }
    case 'lathe': return new THREE.LatheGeometry(p.pts.map(([r, y]) => new THREE.Vector2(r, y)), 32);
    case 'extrude': {
      const s = new THREE.Shape(p.shape.map(([x, y]) => new THREE.Vector2(x, y)));
      return new THREE.ExtrudeGeometry(s, { depth: a[0], bevelEnabled: false });
    }
    case 'gear': {
      const [teeth, ro, rr, depth, bore] = a;
      const g = new THREE.ExtrudeGeometry(gearShape(teeth, ro, rr, bore), { depth, bevelEnabled: false, curveSegments: 6 });
      g.translate(0, 0, -depth / 2);
      return g;
    }
    default: return new THREE.BoxGeometry(0.01, 0.01, 0.01);
  }
}

export function geometryFor(p) {
  const key = JSON.stringify([p.t, p.a, p.pts, p.shape, p.closed]);
  let g = cache.get(key);
  if (!g) {
    const geo = make(p);
    geo.computeBoundingSphere();
    g = { geo, edges: null };
    cache.set(key, g);
  }
  return g;
}

export function edgesFor(p) {
  const g = geometryFor(p);
  if (!g.edges) g.edges = new THREE.EdgesGeometry(g.geo, 25);
  return g.edges;
}
