import React, { memo, useLayoutEffect, useMemo, useRef, useState } from 'react';
import { useFrame } from '@react-three/fiber';
import { Html } from '@react-three/drei';
import * as THREE from 'three';
import { geometryFor, edgesFor } from '../three/primitives.js';
import { realisticMaterials, blueprintMaterials, highlighted } from '../three/materials.js';

const tmp = new THREE.Vector3();

function Primitive({ p, visualStyle, mode, tbd }) {
  const g = geometryFor(p);
  const pos = p.p || [0, 0, 0];
  const rot = p.r || [0, 0, 0];
  if (visualStyle === 'blueprint') {
    const bp = blueprintMaterials();
    const line = mode === 'selected' ? bp.selected : mode === 'hover' ? bp.hover : tbd || p.m === 'tbd' ? bp.tbd : bp.line;
    return (
      <group position={pos} rotation={rot}>
        <mesh geometry={g.geo} material={bp.fill} userData={{ material: p.m }} />
        <lineSegments geometry={edgesFor(p)} material={line} raycast={() => null} />
      </group>
    );
  }
  const mat = mode ? highlighted(p.m, mode) : realisticMaterials()[p.m] || realisticMaterials().greyPlastic;
  return <mesh geometry={g.geo} material={mat} position={pos} rotation={rot} castShadow receiveShadow userData={{ material: p.m }} />;
}

// One part of the car. `origin` is where it sits assembled; `direction` is its explosion vector
// (assembly + own + tier lift, see data/explode.js); the rendered position is
// origin + direction * explodeFactor + offset, eased towards each frame.
function PowerWheelPart({ part, origin, direction, explodeFactor, offset, visible, visualStyle, selected, showTbd, onSelect, onHover, registry }) {
  const ref = useRef();
  const [hover, setHover] = useState(false);
  const target = useMemo(() => {
    const o = offset || [0, 0, 0];
    return [origin[0] + direction[0] * explodeFactor + o[0], origin[1] + direction[1] * explodeFactor + o[1], origin[2] + direction[2] * explodeFactor + o[2]];
  }, [origin, direction, explodeFactor, offset]);

  useLayoutEffect(() => {
    if (!ref.current) return undefined;
    ref.current.position.set(...target);
    registry.set(part.id, { pos: ref.current.position, origin: new THREE.Vector3(...origin).add(new THREE.Vector3(...(offset || [0, 0, 0]))), radius: part.radius });
    return () => registry.delete(part.id);
    // re-register when shown again; target changes are eased in useFrame
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [visible, part.id]);

  useLayoutEffect(() => {
    const r = registry.get(part.id);
    if (r) r.origin.set(...origin).add(tmp.set(...(offset || [0, 0, 0])));
  }, [origin, offset, part.id, registry]);

  useFrame((_, dt) => {
    const g = ref.current;
    if (!g) return;
    tmp.set(...target);
    if (g.position.distanceToSquared(tmp) > 1e-10) g.position.lerp(tmp, 1 - Math.exp(-dt * 7));
  });

  if (!visible) return null;
  const mode = selected ? 'selected' : hover ? 'hover' : null;
  return (
    <group
      ref={ref}
      name={part.ref ? `${part.ref} ${part.name}` : part.name}
      rotation={part.rotation}
      userData={{ id: part.id, ref: part.ref, type: part.type, status: part.status, bomLine: part.bomLine, tbd: part.tbd }}
      onClick={(e) => { e.stopPropagation(); onSelect(part); }}
      onDoubleClick={(e) => { e.stopPropagation(); onSelect(part, true); }}
      onPointerOver={(e) => { e.stopPropagation(); setHover(true); onHover(part); document.body.style.cursor = 'pointer'; }}
      onPointerOut={() => { setHover(false); onHover(null); document.body.style.cursor = 'auto'; }}
    >
      {part.primitives.map((p, i) => <Primitive key={i} p={p} visualStyle={visualStyle} mode={mode} tbd={part.tbd.length > 0 && part.kind === 'deutsch'} />)}
      {showTbd && part.tbd.length > 0 && part.badge !== false && (
        <Html position={[0, part.radius * 1.1 + 0.01, 0]} center zIndexRange={[20, 0]} style={{ pointerEvents: 'none' }}>
          <div className={`tbd-badge ${visualStyle}`}>{typeof part.badge === 'string' ? part.badge : `${part.ref || 'TBD'}${part.issues.length ? ` · #${part.issues[0]}` : ''}`}</div>
        </Html>
      )}
    </group>
  );
}

export default memo(PowerWheelPart);
