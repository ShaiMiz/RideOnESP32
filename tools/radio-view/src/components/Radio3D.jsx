// The Jumper T-Lite V2, built in code. Sizes are in millimetres (the group is scaled to metres).
// The layout follows EdgeTX's T-Lite hardware list (hw_defs/tlite.json): two gimbals, four
// switches on the top edge (SA, SC left; SD, SB right, as radio/MODEL_SETUP.md places them),
// four trims, six keys (RTN, ENT, Up, Down, Left/SYS, Right/MDL), a 128 x 64 screen and power.
// Proportions are approximate, from photos; nothing here is a measured drawing.
import React, { useMemo, useRef, useState, useEffect } from 'react';
import { useFrame } from '@react-three/fiber';
import { Html } from '@react-three/drei';
import * as THREE from 'three';

const BODY = '#2b2f36';
const FACE = '#22262c';
const METAL = '#b8bcc2';
const ACCENT = '#e2531f';

function roundedRect(w, h, r) {
  const s = new THREE.Shape();
  const x = -w / 2;
  const y = -h / 2;
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

function Body() {
  const geo = useMemo(() => {
    const opts = { depth: 22, bevelEnabled: true, bevelThickness: 5, bevelSize: 5, bevelSegments: 5, curveSegments: 24 };
    const main = new THREE.ExtrudeGeometry(roundedRect(160, 78, 22), opts);
    const grip = new THREE.ExtrudeGeometry(new THREE.Shape().absarc(0, 0, 27, 0, Math.PI * 2), opts);
    return { main, grip };
  }, []);
  const mat = <meshStandardMaterial color={BODY} roughness={0.62} metalness={0.05} />;
  return (
    <group>
      <mesh geometry={geo.main} position={[0, 12, -11]} castShadow receiveShadow>{mat}</mesh>
      <mesh geometry={geo.grip} position={[-60, -30, -11]} castShadow receiveShadow>{mat}</mesh>
      <mesh geometry={geo.grip} position={[60, -30, -11]} castShadow receiveShadow>{mat}</mesh>
      {/* face plate */}
      <mesh position={[0, 12, 16.2]}>
        <shapeGeometry args={[roundedRect(150, 70, 18)]} />
        <meshStandardMaterial color={FACE} roughness={0.8} />
      </mesh>
      {/* battery door on the back (one 18650) and the USB-C port on the bottom edge */}
      <mesh position={[0, 6, -16.4]}>
        <boxGeometry args={[84, 30, 1.2]} />
        <meshStandardMaterial color="#353a42" roughness={0.7} />
      </mesh>
      <mesh position={[0, -32, 0]}>
        <boxGeometry args={[10, 2, 3.6]} />
        <meshStandardMaterial color="#0b0c0e" />
      </mesh>
    </group>
  );
}

function Label({ position, children, show, side = 'center' }) {
  if (!show) return null;
  return (
    <Html position={position} center={side === 'center'} zIndexRange={[20, 0]} style={{ pointerEvents: 'none' }}>
      <div className={`tag tag-${side}`}>{children}</div>
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

function Stick({ position, x, y, onDown, label, show, id, active }) {
  const g = useRef();
  const [hover, hv] = useHover();
  useFrame(() => {
    if (!g.current) return;
    g.current.rotation.x += (-y * 0.42 - g.current.rotation.x) * 0.45;
    g.current.rotation.y += (x * 0.42 - g.current.rotation.y) * 0.45;
  });
  return (
    <group position={position}>
      <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, 0, 16.6]}>
        <cylinderGeometry args={[19, 19, 1.2, 48]} />
        <meshStandardMaterial color="#17191d" roughness={0.9} />
      </mesh>
      <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, 0, 17.4]}>
        <cylinderGeometry args={[9, 11, 1.4, 32]} />
        <meshStandardMaterial color="#30343b" roughness={0.5} />
      </mesh>
      <group ref={g} position={[0, 0, 17]}>
        <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, 0, 8]}>
          <cylinderGeometry args={[2.2, 2.6, 16, 16]} />
          <meshStandardMaterial color={METAL} metalness={0.8} roughness={0.3} />
        </mesh>
        <mesh
          rotation={[Math.PI / 2, 0, 0]}
          position={[0, 0, 19]}
          onPointerDown={(e) => { e.stopPropagation(); onDown(id, e); }}
          {...hv}
          castShadow
        >
          <cylinderGeometry args={[6.5, 6.5, 7, 28]} />
          <meshStandardMaterial color={active ? ACCENT : hover ? '#d4d8de' : METAL} metalness={0.85} roughness={0.35} />
        </mesh>
      </group>
      <Label position={[0, -27, 20]} show={show}>{label}</Label>
    </group>
  );
}

function Switch({ position, name, pos, positions, onCycle, label, show, labelY = 24 }) {
  const g = useRef();
  const [hover, hv] = useHover();
  const target = positions === 2 ? (pos ? 0.55 : -0.55) : (pos - 1) * 0.55;
  useFrame(() => {
    if (g.current) g.current.rotation.x += (target - g.current.rotation.x) * 0.35;
  });
  return (
    <group position={position}>
      <mesh>
        <cylinderGeometry args={[4.2, 4.6, 4, 24]} />
        <meshStandardMaterial color="#3a3f47" metalness={0.4} roughness={0.4} />
      </mesh>
      <group ref={g}>
        <mesh
          position={[0, 8, 0]}
          onClick={(e) => { e.stopPropagation(); onCycle(name, e.shiftKey ? -1 : 1); }}
          {...hv}
          castShadow
        >
          <cylinderGeometry args={[1.5, 2, 14, 14]} />
          <meshStandardMaterial color={hover ? ACCENT : METAL} metalness={0.85} roughness={0.3} />
        </mesh>
        <mesh position={[0, 15, 0]} onClick={(e) => { e.stopPropagation(); onCycle(name, e.shiftKey ? -1 : 1); }} {...hv}>
          <sphereGeometry args={[2.6, 16, 12]} />
          <meshStandardMaterial color={hover ? ACCENT : '#dfe2e6'} metalness={0.6} roughness={0.3} />
        </mesh>
      </group>
      <Label position={[0, labelY, 0]} show={show}>{label}</Label>
    </group>
  );
}

const keyTextures = new Map();
// A key's legend, drawn once on a small canvas.
function keyTexture(text) {
  if (!keyTextures.has(text)) {
    const c = document.createElement('canvas');
    c.width = 128;
    c.height = 64;
    const g = c.getContext('2d');
    g.fillStyle = '#e9ecef';
    g.font = `bold ${text.length > 1 ? 34 : 40}px Inter, system-ui, sans-serif`;
    g.textAlign = 'center';
    g.textBaseline = 'middle';
    g.fillText(text, 64, 34);
    const t = new THREE.CanvasTexture(c);
    t.colorSpace = THREE.SRGBColorSpace;
    keyTextures.set(text, t);
  }
  return keyTextures.get(text);
}

function Key({ position, size, name, onKey, pressed, label, round }) {
  const [hover, hv] = useHover();
  const down = pressed === name;
  return (
    <group position={position}>
      <mesh
        rotation={round ? [Math.PI / 2, 0, 0] : [0, 0, 0]}
        position={[0, 0, down ? 16.9 : 17.6]}
        onClick={(e) => { e.stopPropagation(); onKey(name); }}
        {...hv}
        castShadow
      >
        {round ? <cylinderGeometry args={[size[0] / 2, size[0] / 2, 2.4, 24]} /> : <boxGeometry args={[size[0], size[1], 2.4]} />}
        <meshStandardMaterial color={hover ? '#5b616b' : '#40454d'} roughness={0.55} />
      </mesh>
      {label ? (
        <mesh position={[0, 0, (down ? 16.9 : 17.6) + 1.25]}>
          <planeGeometry args={[size[0] * 0.9, size[0] * 0.9 * 0.5]} />
          <meshBasicMaterial map={keyTexture(label)} transparent toneMapped={false} />
        </mesh>
      ) : null}
    </group>
  );
}

function Screen({ canvas, version }) {
  const tex = useMemo(() => {
    const t = new THREE.CanvasTexture(canvas);
    t.colorSpace = THREE.SRGBColorSpace;
    t.anisotropy = 4;
    return t;
  }, [canvas]);
  useEffect(() => { tex.needsUpdate = true; }, [tex, version]);
  return (
    <group position={[0, 27, 16.4]}>
      <mesh>
        <boxGeometry args={[64, 36, 1.2]} />
        <meshStandardMaterial color="#0e1013" roughness={0.3} />
      </mesh>
      <mesh position={[0, 0, 0.65]}>
        <planeGeometry args={[56, 28]} />
        <meshBasicMaterial map={tex} toneMapped={false} />
      </mesh>
    </group>
  );
}

export default function Radio3D({ sticks, switches, dragging, onStickDown, onSwitch, onKey, onTrim, pressed, lcd, lcdVersion, labels, showLabels }) {
  const T = (s) => labels[s] || s;
  return (
    <group scale={0.01}>
      <Body />
      <Screen canvas={lcd} version={lcdVersion} />
      <Stick id="left" position={[-58, 2, 0]} x={sticks.lh} y={sticks.lv} onDown={onStickDown} label={T('left')} show={showLabels} active={dragging === 'left'} />
      <Stick id="right" position={[58, 2, 0]} x={sticks.rh} y={sticks.rv} onDown={onStickDown} label={T('right')} show={showLabels} active={dragging === 'right'} />

      {/* switches on the top edge: "up" is away from you */}
      <Switch name="SA" position={[-62, 55.5, 0]} pos={switches.SA} positions={3} onCycle={onSwitch} label={T('SA')} show={showLabels} labelY={44} />
      <Switch name="SC" position={[-38, 55.5, 0]} pos={switches.SC} positions={2} onCycle={onSwitch} label={T('SC')} show={showLabels} />
      <Switch name="SD" position={[38, 55.5, 0]} pos={switches.SD} positions={2} onCycle={onSwitch} label={T('SD')} show={showLabels} />
      <Switch name="SB" position={[62, 55.5, 0]} pos={switches.SB} positions={3} onCycle={onSwitch} label={T('SB')} show={showLabels} labelY={44} />

      {/* trims: T1 left horizontal, T2 left vertical, T3 right vertical, T4 right horizontal */}
      {[
        ['T1', [-58, -22], true], ['T2', [-31, 2], false], ['T3', [31, 2], false], ['T4', [58, -22], true],
      ].map(([n, [x, y], horiz]) => (
        <group key={n} position={[x, y, 0]}>
          {[-1, 1].map((d) => (
            <Key key={d} name={`${n}${d > 0 ? '+' : '-'}`} position={horiz ? [d * 5, 0, 0] : [0, d * 5, 0]} size={horiz ? [8, 4] : [4, 8]} onKey={onTrim} pressed={pressed} label="" />
          ))}
        </group>
      ))}

      {/* keys under the screen */}
      <Key name="UP" label="▲" position={[0, 4, 0]} size={[9, 5]} onKey={onKey} pressed={pressed} />
      <Key name="DOWN" label="▼" position={[0, -7, 0]} size={[9, 5]} onKey={onKey} pressed={pressed} />
      <Key name="LEFT" label="SYS" position={[-14, -2, 0]} size={[11, 6]} onKey={onKey} pressed={pressed} />
      <Key name="RIGHT" label="MDL" position={[14, -2, 0]} size={[11, 6]} onKey={onKey} pressed={pressed} />
      <Key name="EXIT" label="RTN" position={[-14, -13, 0]} size={[11, 6]} onKey={onKey} pressed={pressed} />
      <Key name="ENTER" label="ENT" position={[14, -13, 0]} size={[11, 6]} onKey={onKey} pressed={pressed} />
      <Key name="POWER" label="⏻" position={[0, -19, 0]} size={[7, 7]} round onKey={onKey} pressed={pressed} />
    </group>
  );
}
