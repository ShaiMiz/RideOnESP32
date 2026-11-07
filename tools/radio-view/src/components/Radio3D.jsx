// The Jumper T-Lite V2, built in code from the measurements in src/lib/tlite.js (millimetres;
// the group is scaled to metres). Layout follows Jumper's own components diagram: ENT and RTN left
// of the screen, UP and DOWN right of it, SYS and MDL under it, the LED and the oval power key in
// the middle with the trim buttons around it, the speaker lower left, and the switches, antenna
// and ports on the back edge.
import React, { useMemo, useRef, useState, useEffect } from 'react';
import { useFrame } from '@react-three/fiber';
import { Html } from '@react-three/drei';
import * as THREE from 'three';
import { BODY, OUTLINE_RIGHT, STICK_X, GIMBAL, SCREEN, KEYS, LED, TRIMS, SPEAKER, SWITCH_POS, BACK_PORTS } from '../lib/tlite.js';

const SHELL = '#55595f'; // Jumper's grey
const SHELL_DARK = '#3d4045';
const WELL = '#17191c';
const CHROME = '#c9ccd0';
const ACCENT = '#e2531f';
const FACE_Z = BODY.shell / 2; // the face plane
const BEVEL = 6;

const smoothOutline = (inset) => {
  const right = OUTLINE_RIGHT;
  const left = [...right].reverse().map(([x, y]) => [-x, y]);
  const pts = [...right, ...left.slice(1, -1)];
  const cy = (BODY.top + BODY.bottom) / 2;
  const sx = (BODY.width / 2 - inset) / (BODY.width / 2);
  const sy = ((BODY.top - BODY.bottom) / 2 - inset) / ((BODY.top - BODY.bottom) / 2);
  const curve = new THREE.CatmullRomCurve3(pts.map(([x, y]) => new THREE.Vector3(x * sx, cy + (y - cy) * sy, 0)), true, 'centripetal');
  return curve.getPoints(160).map((p) => new THREE.Vector2(p.x, p.y));
};

/** The outline's top edge height at x (for placing parts on the back edge). */
export function topEdgeAt(x) {
  const pts = OUTLINE_RIGHT.filter(([, y]) => y > 30);
  const ax = Math.abs(x);
  for (let i = 0; i < pts.length - 1; i++) {
    const [x0, y0] = pts[i];
    const [x1, y1] = pts[i + 1];
    if (ax >= x0 && ax <= x1) return y0 + ((y1 - y0) * (ax - x0)) / (x1 - x0);
  }
  return pts[pts.length - 1][1];
}

function roundedRect(w, h, r) {
  const s = new THREE.Shape();
  const x = -w / 2;
  const y = -h / 2;
  r = Math.min(r, w / 2, h / 2);
  s.moveTo(x + r, y);
  s.lineTo(x + w - r, y);
  s.quadraticCurveTo(x + w, y, x + w, y + r);
  s.lineTo(x + w, y + h - r);
  s.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
  s.lineTo(x + r, y + h);
  s.quadraticCurveTo(x, y + h, x, y + h - r);
  s.lineTo(x, y + r);
  s.quadraticCurveTo(x, y, x + r, y);
  return s;
}

function roundedTriangle(size) {
  // points up; corners softened
  const s = new THREE.Shape();
  const h = size * 0.86;
  const p = [[0, h / 2], [size / 2, -h / 2], [-size / 2, -h / 2]];
  const k = 0.28;
  const lerp = (a, b, t) => [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t];
  for (let i = 0; i < 3; i++) {
    const a = p[i];
    const b = p[(i + 1) % 3];
    const prev = p[(i + 2) % 3];
    const start = lerp(a, prev, k);
    const end = lerp(a, b, k);
    if (i === 0) s.moveTo(...start);
    else s.lineTo(...start);
    s.quadraticCurveTo(a[0], a[1], end[0], end[1]);
  }
  s.closePath();
  return s;
}

const shellMat = <meshStandardMaterial color={SHELL} roughness={0.78} metalness={0.02} />;

function Body() {
  const geo = useMemo(() => {
    const shape = new THREE.Shape(smoothOutline(BEVEL));
    const g = new THREE.ExtrudeGeometry(shape, {
      depth: BODY.shell - 2 * BEVEL, bevelEnabled: true, bevelThickness: BEVEL, bevelSize: BEVEL, bevelSegments: 8, curveSegments: 12,
    });
    g.translate(0, 0, -(BODY.shell - 2 * BEVEL) / 2);
    g.computeVertexNormals();
    return g;
  }, []);
  // a seam line around the middle of the shell, where the two halves meet
  const seam = useMemo(() => {
    const pts = smoothOutline(-0.15).map((p) => new THREE.Vector3(p.x, p.y, -3));
    return new THREE.BufferGeometry().setFromPoints([...pts, pts[0]]);
  }, []);
  return (
    <group>
      <mesh geometry={geo} castShadow receiveShadow>{shellMat}</mesh>
      <line geometry={seam}><lineBasicMaterial color="#2c2f33" /></line>
      {/* battery door on the back (one 18650), with its latch */}
      <mesh position={[0, -6, -FACE_Z - 0.2]}>
        <shapeGeometry args={[roundedRect(78, 28, 5)]} />
        <meshStandardMaterial color={SHELL_DARK} roughness={0.8} side={THREE.DoubleSide} />
      </mesh>
      <mesh position={[0, -21.5, -FACE_Z - 0.3]}>
        <boxGeometry args={[14, 2.5, 0.6]} />
        <meshStandardMaterial color="#2a2c30" />
      </mesh>
    </group>
  );
}

function Label({ position, children, show }) {
  if (!show) return null;
  return (
    <Html position={position} center zIndexRange={[20, 0]} style={{ pointerEvents: 'none' }}>
      <div className="tag">{children}</div>
    </Html>
  );
}

function useHover() {
  const [h, setH] = useState(false);
  return [h, {
    onPointerOver: (e) => { e.stopPropagation(); setH(true); document.body.style.cursor = 'pointer'; },
    onPointerOut: () => { setH(false); document.body.style.cursor = ''; },
  }];
}

const textures = new Map();
// Small printed text on the face, drawn once per string.
function textTexture(text, color = '#e6e8eb') {
  const k = `${text}|${color}`;
  if (!textures.has(k)) {
    const c = document.createElement('canvas');
    c.width = 128;
    c.height = 48;
    const g = c.getContext('2d');
    g.fillStyle = color;
    g.font = 'bold 30px Inter, system-ui, sans-serif';
    g.textAlign = 'center';
    g.textBaseline = 'middle';
    g.fillText(text, 64, 26);
    const t = new THREE.CanvasTexture(c);
    t.colorSpace = THREE.SRGBColorSpace;
    t.anisotropy = 4;
    textures.set(k, t);
  }
  return textures.get(k);
}

function Print({ text, position, width = 8, color }) {
  return (
    <mesh position={position}>
      <planeGeometry args={[width, width * 0.375]} />
      <meshBasicMaterial map={textTexture(text, color)} transparent depthWrite={false} toneMapped={false} />
    </mesh>
  );
}

// ---- gimbals and sticks ----------------------------------------------------------------------

function Knurl() {
  // the knurled metal stick end: a cylinder with fine ridges and a crowned top
  const r = GIMBAL.capDiameter / 2;
  const ridges = 22;
  return (
    <group>
      <mesh rotation={[Math.PI / 2, 0, 0]}>
        <cylinderGeometry args={[r - 0.25, r - 0.25, 9, 32]} />
        <meshStandardMaterial color={CHROME} metalness={0.95} roughness={0.25} />
      </mesh>
      {[...Array(ridges).keys()].map((i) => {
        const a = (i / ridges) * Math.PI * 2;
        return (
          <mesh key={i} position={[Math.cos(a) * (r - 0.2), Math.sin(a) * (r - 0.2), 0]} rotation={[0, 0, a]}>
            <boxGeometry args={[0.55, 0.55, 8.6]} />
            <meshStandardMaterial color="#e3e5e8" metalness={0.95} roughness={0.2} />
          </mesh>
        );
      })}
      <mesh position={[0, 0, 4.6]} rotation={[Math.PI / 2, 0, 0]}>
        <cylinderGeometry args={[r * 0.55, r - 0.2, 1.4, 24]} />
        <meshStandardMaterial color={CHROME} metalness={0.95} roughness={0.25} />
      </mesh>
    </group>
  );
}

function Stick({ side, x, y, onDown, label, show, active }) {
  const g = useRef();
  const [hover, hv] = useHover();
  const throw_ = (GIMBAL.throwDeg * Math.PI) / 180;
  useFrame(() => {
    if (!g.current) return;
    g.current.rotation.x += (-y * throw_ - g.current.rotation.x) * 0.45;
    g.current.rotation.y += (x * throw_ - g.current.rotation.y) * 0.45;
  });
  const cx = side === 'left' ? -STICK_X : STICK_X;
  const o = GIMBAL.openingSize;
  return (
    <group position={[cx, 0, FACE_Z]}>
      {/* the round well */}
      <mesh position={[0, 0, 0.05]}>
        <circleGeometry args={[GIMBAL.recessRadius, 64]} />
        <meshStandardMaterial color={WELL} roughness={0.9} />
      </mesh>
      <mesh position={[0, 0, 0.2]}>
        <ringGeometry args={[GIMBAL.recessRadius - 0.9, GIMBAL.recessRadius, 64]} />
        <meshStandardMaterial color="#2a2d31" roughness={0.6} />
      </mesh>
      {/* the gimbal frame: square opening, side rails, the ratchet strip on the throttle side */}
      <mesh position={[0, 0, 0.3]}>
        <shapeGeometry args={[roundedRect(o + 4, o + 4, 4)]} />
        <meshStandardMaterial color="#222428" roughness={0.7} />
      </mesh>
      <mesh position={[0, 0, 0.4]}>
        <shapeGeometry args={[roundedRect(o, o, 3)]} />
        <meshStandardMaterial color="#0b0c0e" roughness={1} />
      </mesh>
      {[-1, 1].map((d) => (
        <mesh key={d} position={[d * (o / 2 + 0.6), 0, 0.6]}>
          <boxGeometry args={[1.2, o - 6, 0.6]} />
          <meshStandardMaterial color="#34373c" />
        </mesh>
      ))}
      {[...Array(9).keys()].map((i) => (
        <mesh key={i} position={[(side === 'left' ? 1 : -1) * (o / 2 - 2.2), -o / 2 + 5 + i * 2.5, 0.55]}>
          <boxGeometry args={[2.4, 0.5, 0.3]} />
          <meshStandardMaterial color="#3b3e43" />
        </mesh>
      ))}
      {/* stick: pivot below the face, dust plate, shaft, knurled end */}
      <group ref={g} position={[0, 0, -4]}>
        <mesh position={[0, 0, 5.2]}>
          <boxGeometry args={[o - 9, o - 9, 1]} />
          <meshStandardMaterial color="#1c1e21" roughness={0.6} />
        </mesh>
        <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, 0, 14]}>
          <cylinderGeometry args={[1.9, 3, 18, 20]} />
          <meshStandardMaterial color="#141517" roughness={0.45} />
        </mesh>
        <group position={[0, 0, 27.5]} onPointerDown={(e) => { e.stopPropagation(); onDown(side, e); }} {...hv}>
          <Knurl />
          <mesh rotation={[Math.PI / 2, 0, 0]} visible={false}>
            <cylinderGeometry args={[7, 7, 12, 12]} />
            <meshBasicMaterial />
          </mesh>
        </group>
        {(hover || active) && (
          <mesh position={[0, 0, 33]}>
            <ringGeometry args={[5.4, 6.4, 32]} />
            <meshBasicMaterial color={ACCENT} />
          </mesh>
        )}
      </group>
      <Label position={[0, -GIMBAL.recessRadius - 6, 4]} show={show}>{label}</Label>
    </group>
  );
}

// ---- switches on the back edge -----------------------------------------------------------------

function Switch({ name, pos, onCycle, label, show }) {
  const s = SWITCH_POS[name];
  const g = useRef();
  const [hover, hv] = useHover();
  const target = s.positions === 2 ? (pos ? -0.42 : 0.42) : (1 - pos) * 0.42; // up = tip towards the face
  useFrame(() => {
    if (g.current) g.current.rotation.x += (target - g.current.rotation.x) * 0.35;
  });
  const y0 = topEdgeAt(s.x) - 1.2;
  const click = (e) => { e.stopPropagation(); onCycle(name, e.shiftKey ? -1 : 1); };
  return (
    <group position={[s.x, y0, s.z]}>
      {/* boss, hex nut and chrome bushing */}
      <mesh position={[0, 0.6, 0]}>
        <cylinderGeometry args={[5.2, 5.6, 2.2, 32]} />
        <meshStandardMaterial color={SHELL} roughness={0.78} />
      </mesh>
      <mesh position={[0, 2.6, 0]}>
        <cylinderGeometry args={[4, 4, 2, 6]} />
        <meshStandardMaterial color={CHROME} metalness={0.9} roughness={0.3} />
      </mesh>
      <mesh position={[0, 4.6, 0]}>
        <cylinderGeometry args={[2.8, 2.8, 2.4, 24]} />
        <meshStandardMaterial color={CHROME} metalness={0.9} roughness={0.25} />
      </mesh>
      <group ref={g} position={[0, 5.4, 0]}>
        <mesh position={[0, 4.5, 0]} onClick={click} {...hv} castShadow>
          <cylinderGeometry args={[1.05, 1.5, 9, 16]} />
          <meshStandardMaterial color={hover ? ACCENT : CHROME} metalness={0.9} roughness={0.25} />
        </mesh>
        {/* flat paddle tip */}
        <mesh position={[0, 10.8, 0]} onClick={click} {...hv} castShadow>
          <boxGeometry args={[4.6, 5.2, 1.3]} />
          <meshStandardMaterial color={hover ? ACCENT : '#e2e4e7'} metalness={0.85} roughness={0.25} />
        </mesh>
      </group>
      <Label position={[0, s.positions === 3 ? 34 : 20, 0]} show={show}>{label}</Label>
    </group>
  );
}

// ---- back edge: antenna, reserved screw holes, ports ------------------------------------------

function BackEdge() {
  const top = topEdgeAt(0);
  return (
    <group>
      {BACK_PORTS.map((p) => {
        const y = topEdgeAt(p.x);
        if (p.kind === 'jack') return (
          <mesh key={p.name} position={[p.x, y + 0.05, p.z]}>
            <cylinderGeometry args={[2.4, 2.4, 0.4, 24]} />
            <meshStandardMaterial color="#08090a" />
          </mesh>
        );
        if (p.kind === 'usbc') return (
          <mesh key={p.name} position={[p.x, y + 0.05, p.z]}>
            <boxGeometry args={[9, 0.4, 3.4]} />
            <meshStandardMaterial color="#08090a" />
          </mesh>
        );
        if (p.kind === 'slot') return (
          <mesh key={p.name} position={[p.x, y + 0.05, p.z]}>
            <boxGeometry args={[12, 0.4, 1.2]} />
            <meshStandardMaterial color="#08090a" />
          </mesh>
        );
        return null;
      })}
      {/* the two reserved screw holes for an FPV monitor holder, either side of the antenna */}
      {[-14, 14].map((x) => (
        <group key={x} position={[x, topEdgeAt(x), -3]}>
          <mesh position={[0, 0.1, 0]}>
            <cylinderGeometry args={[3.4, 3.4, 0.3, 32]} />
            <meshStandardMaterial color="#8d939a" metalness={0.8} roughness={0.35} />
          </mesh>
          <mesh position={[0, 0.3, 0]}>
            <cylinderGeometry args={[1.3, 1.3, 0.3, 16]} />
            <meshStandardMaterial color="#060607" />
          </mesh>
        </group>
      ))}
      {/* SMA connector, knurled hinge and the dipole antenna, tilted a little back */}
      <group position={[0, top, -3]}>
        <mesh position={[0, 2.5, 0]}>
          <cylinderGeometry args={[3.2, 3.2, 5, 6]} />
          <meshStandardMaterial color="#c9a54a" metalness={0.9} roughness={0.3} />
        </mesh>
        <group position={[0, 5, 0]} rotation={[-0.12, 0, 0]}>
          <mesh position={[0, 5, 0]}>
            <cylinderGeometry args={[4.2, 4.2, 10, 24]} />
            <meshStandardMaterial color="#1b1c1e" roughness={0.6} />
          </mesh>
          {[...Array(16).keys()].map((i) => {
            const a = (i / 16) * Math.PI * 2;
            return (
              <mesh key={i} position={[Math.cos(a) * 4.2, 5, Math.sin(a) * 4.2]} rotation={[0, -a, 0]}>
                <boxGeometry args={[0.5, 9, 0.5]} />
                <meshStandardMaterial color="#26272a" />
              </mesh>
            );
          })}
          <mesh position={[0, 58, 0]} castShadow>
            <cylinderGeometry args={[3.6, 4.3, 86, 24]} />
            <meshStandardMaterial color="#151618" roughness={0.55} />
          </mesh>
          <mesh position={[0, 101, 0]}>
            <sphereGeometry args={[3.6, 20, 12]} />
            <meshStandardMaterial color="#151618" roughness={0.55} />
          </mesh>
        </group>
      </group>
    </group>
  );
}

// ---- face: screen, keys, LED, trims, speaker ----------------------------------------------------

function Screen({ canvas, version }) {
  const tex = useMemo(() => {
    const t = new THREE.CanvasTexture(canvas);
    t.colorSpace = THREE.SRGBColorSpace;
    t.anisotropy = 4;
    return t;
  }, [canvas]);
  useEffect(() => { tex.needsUpdate = true; }, [tex, version]);
  const [bw, bh] = SCREEN.bezel;
  const [ww, wh] = SCREEN.window;
  const [aw, ah] = SCREEN.active;
  return (
    <group position={[0, SCREEN.y, FACE_Z]}>
      <mesh position={[0, 0, 0.1]}>
        <shapeGeometry args={[roundedRect(bw + 3, bh + 3, 4)]} />
        <meshStandardMaterial color="#45484d" roughness={0.6} />
      </mesh>
      <mesh position={[0, 0, 0.2]}>
        <shapeGeometry args={[roundedRect(bw, bh, 3)]} />
        <meshStandardMaterial color="#0d0f12" roughness={0.25} metalness={0.2} />
      </mesh>
      <mesh position={[0, 0, 0.3]}>
        <planeGeometry args={[ww, wh]} />
        <meshStandardMaterial color="#1a2129" roughness={0.3} />
      </mesh>
      <mesh position={[0, 0, 0.4]}>
        <planeGeometry args={[aw, ah]} />
        <meshBasicMaterial map={tex} toneMapped={false} />
      </mesh>
    </group>
  );
}

function Key({ k, onKey, pressed, showText }) {
  const [hover, hv] = useHover();
  const down = pressed === k.key;
  const geo = useMemo(() => {
    const shape = k.shape === 'oval' ? roundedRect(k.w, k.h, k.w / 2) : roundedRect(k.w, k.h, k.h / 2);
    const g = new THREE.ExtrudeGeometry(shape, { depth: 1.2, bevelEnabled: true, bevelThickness: 0.5, bevelSize: 0.4, bevelSegments: 3 });
    return g;
  }, [k]);
  const isPower = k.key === 'POWER';
  return (
    <group position={[k.x, k.y, FACE_Z]} rotation={[0, 0, k.angle]}>
      <mesh position={[0, 0, 0.05]}>
        <shapeGeometry args={[roundedRect(k.w + 1.6, k.h + 1.6, (k.shape === 'oval' ? k.w : k.h) / 2 + 0.8)]} />
        <meshStandardMaterial color="#2b2e32" roughness={0.9} />
      </mesh>
      <mesh geometry={geo} position={[0, 0, down ? -0.6 : 0]} onClick={(e) => { e.stopPropagation(); onKey(k.key); }} {...hv}>
        <meshStandardMaterial color={hover ? '#6c7178' : '#3f4247'} roughness={0.55} />
      </mesh>
      {isPower && <Print text="⏻" position={[0, 0, 2.3 + (down ? -0.6 : 0)]} width={6} color="#c8ccd1" />}
      {showText && k.label && (
        <group rotation={[0, 0, -k.angle]}>
          <Print text={k.label} position={[k.x < 0 ? -8.5 : 8.5, 0, 0.3]} width={7} color="#b8bec5" />
        </group>
      )}
    </group>
  );
}

function Trim({ t, onTrim, pressed }) {
  const [hover, hv] = useHover();
  const down = pressed === t.name;
  const rot = { up: 0, down: Math.PI, left: Math.PI / 2, right: -Math.PI / 2 }[t.dir];
  const geo = useMemo(() => new THREE.ExtrudeGeometry(roundedTriangle(6), { depth: 1, bevelEnabled: true, bevelThickness: 0.5, bevelSize: 0.35, bevelSegments: 3 }), []);
  return (
    <group position={[t.x, t.y, FACE_Z]} rotation={[0, 0, rot]}>
      <mesh position={[0, 0, 0.05]}>
        <shapeGeometry args={[roundedTriangle(8)]} />
        <meshStandardMaterial color="#2b2e32" roughness={0.9} />
      </mesh>
      <mesh geometry={geo} position={[0, 0, down ? -0.5 : 0]} onClick={(e) => { e.stopPropagation(); onTrim(t.name); }} {...hv}>
        <meshStandardMaterial color={hover ? '#6c7178' : '#3f4247'} roughness={0.55} />
      </mesh>
    </group>
  );
}

function Speaker() {
  const holes = [];
  for (let r = 0; r <= 3; r++) {
    const n = r === 0 ? 1 : r * 6;
    for (let i = 0; i < n; i++) {
      const a = (i / n) * Math.PI * 2;
      holes.push([SPEAKER.x + Math.cos(a) * r * 1.9, SPEAKER.y + Math.sin(a) * r * 1.9]);
    }
  }
  return holes.map(([x, y], i) => (
    <mesh key={i} position={[x, y, FACE_Z + 0.05]}>
      <circleGeometry args={[0.55, 12]} />
      <meshBasicMaterial color="#1c1d20" />
    </mesh>
  ));
}

function Led({ on }) {
  return (
    <group position={[LED.x, LED.y, FACE_Z]}>
      <mesh position={[0, 0, 0.05]}>
        <circleGeometry args={[LED.d / 2 + 1, 32]} />
        <meshStandardMaterial color="#2b2e32" />
      </mesh>
      <mesh position={[0, 0, 0.3]}>
        <circleGeometry args={[LED.d / 2, 32]} />
        <meshStandardMaterial color={on ? '#3dff6e' : '#1f3326'} emissive={on ? '#2bff5a' : '#000000'} emissiveIntensity={on ? 1.6 : 0} />
      </mesh>
    </group>
  );
}

export default function Radio3D({ sticks, switches, dragging, onStickDown, onSwitch, onKey, onTrim, pressed, lcd, lcdVersion, labels, showLabels, power }) {
  const T = (s) => labels[s] || s;
  return (
    <group scale={0.01}>
      <Body />
      <BackEdge />
      <Screen canvas={lcd} version={lcdVersion} />
      <Print text="JUMPER" position={[9, 14.6, FACE_Z + 0.05]} width={10} color="#d9dde2" />
      <Led on={power} />
      <Speaker />
      <Stick side="left" x={sticks.lh} y={sticks.lv} onDown={onStickDown} label={T('left')} show={showLabels} active={dragging === 'left'} />
      <Stick side="right" x={sticks.rh} y={sticks.rv} onDown={onStickDown} label={T('right')} show={showLabels} active={dragging === 'right'} />
      {Object.keys(SWITCH_POS).map((sw) => (
        <Switch key={sw} name={sw} pos={switches[sw]} onCycle={onSwitch} label={T(sw)} show={showLabels} />
      ))}
      {KEYS.map((k) => <Key key={k.key} k={k} onKey={onKey} pressed={pressed} showText={showLabels} />)}
      {TRIMS.map((t) => <Trim key={t.name} t={t} onTrim={onTrim} pressed={pressed} />)}
    </group>
  );
}
