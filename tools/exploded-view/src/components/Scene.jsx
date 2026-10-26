import React, { useEffect, useMemo, useRef } from 'react';
import { useFrame, useThree } from '@react-three/fiber';
import { OrbitControls, Environment, Lightformer, Grid, TransformControls } from '@react-three/drei';
import * as THREE from 'three';
import PowerWheelPart from './PowerWheelPart.jsx';
import { Wiring, ProjectionLines } from './Wiring.jsx';
import { BLUEPRINT } from '../three/materials.js';

// Hands gl, scene and camera to the export code outside the canvas.
function Bridge({ threeRef }) {
  const three = useThree();
  useEffect(() => { threeRef.current = three; }, [three, threeRef]);
  return null;
}

// Eases the orbit target (and camera, keeping its angle) to a part on focus.
function Focus({ focus, registry }) {
  const { controls, camera } = useThree();
  const anim = useRef(null);
  useEffect(() => {
    if (!focus || !controls) return;
    const r = registry.get(focus.id);
    if (!r) return;
    const to = r.pos.clone();
    const dist = Math.max(0.4, focus.radius * 8);
    const dir = camera.position.clone().sub(controls.target).normalize();
    anim.current = { t: 0, fromT: controls.target.clone(), toT: to, fromC: camera.position.clone(), toC: to.clone().add(dir.multiplyScalar(dist)) };
  }, [focus, controls, camera, registry]);
  useFrame((_, dt) => {
    const a = anim.current;
    if (!a || !controls) return;
    a.t = Math.min(1, a.t + dt * 2.2);
    const k = 1 - Math.pow(1 - a.t, 3);
    controls.target.lerpVectors(a.fromT, a.toT, k);
    camera.position.lerpVectors(a.fromC, a.toC, k);
    controls.update();
    if (a.t >= 1) anim.current = null;
  });
  return null;
}

// Proxy the move gizmo drags; the part follows through its offset.
function MoveGizmo({ part, base, offset, onLive, onCommit }) {
  const ref = useRef();
  const start = useRef(null);
  const at = [base[0] + offset[0], base[1] + offset[1], base[2] + offset[2]];
  return (
    <TransformControls
      position={at}
      size={0.6}
      onMouseDown={() => { start.current = offset; }}
      onObjectChange={() => {
        const p = ref.current?.parent?.position || ref.current?.position;
        if (!p) return;
        onLive([p.x - base[0], p.y - base[1], p.z - base[2]]);
      }}
      onMouseUp={() => onCommit(start.current)}
    >
      <group ref={ref}>
        <mesh visible={false}><boxGeometry args={[0.01, 0.01, 0.01]} /></mesh>
      </group>
    </TransformControls>
  );
}

export default function Scene({
  model, directions, hidden, explodeFactor, offsets, visualStyle, selected, onSelect, onHover, wireMode,
  showTbd, showPaths, registry, partsRef, wiresRef, threeRef, focus, moving, onLiveOffset, onCommitOffset,
}) {
  const visibleIds = useMemo(() => model.parts.filter((p) => !hidden.has(p.id)).map((p) => p.id), [model, hidden]);
  const visibleSet = useMemo(() => new Set(visibleIds), [visibleIds]);
  const connections = useMemo(() => model.connections.filter((w) => visibleSet.has(w.from.ref) && visibleSet.has(w.to.ref)), [model, visibleSet]);
  const realistic = visualStyle === 'realistic';
  const selectedPart = selected ? model.parts.find((p) => p.id === selected) : null;

  return (
    <>
      <Bridge threeRef={threeRef} />
      <color attach="background" args={[realistic ? '#e7ebef' : BLUEPRINT.background]} />
      {realistic ? (
        <>
          <ambientLight intensity={0.35} />
          <directionalLight
            position={[2.5, 4, 2]}
            intensity={2.2}
            castShadow
            shadow-mapSize={[2048, 2048]}
            shadow-camera-left={-2.5}
            shadow-camera-right={2.5}
            shadow-camera-top={2.5}
            shadow-camera-bottom={-2.5}
            shadow-bias={-0.0004}
          />
          <directionalLight position={[-3, 2, -2]} intensity={0.5} />
          {/* soft studio environment built from light panels: no HDR file to load */}
          <Environment resolution={256} frames={1}>
            <Lightformer form="rect" intensity={3} position={[0, 5, 0]} rotation-x={Math.PI / 2} scale={[8, 8, 1]} />
            <Lightformer form="rect" intensity={1.5} position={[-5, 1.5, 0]} rotation-y={Math.PI / 2} scale={[6, 2, 1]} />
            <Lightformer form="rect" intensity={1.5} position={[5, 1.5, 0]} rotation-y={-Math.PI / 2} scale={[6, 2, 1]} />
            <Lightformer form="ring" intensity={2} color="#ffe2c4" position={[2, 2, 5]} scale={2} />
          </Environment>
          <mesh rotation-x={-Math.PI / 2} position={[0, 0, 0]} receiveShadow raycast={() => null} name="floor">
            <planeGeometry args={[12, 12]} />
            <shadowMaterial opacity={0.22} />
          </mesh>
          <Grid args={[12, 12]} cellSize={0.1} sectionSize={0.5} cellColor="#c9d0d8" sectionColor="#aab4bf" fadeDistance={9} infiniteGrid position={[0, 0.0005, 0]} />
        </>
      ) : (
        <>
          <ambientLight intensity={1} />
          <Grid args={[12, 12]} cellSize={0.025} sectionSize={0.25} cellThickness={0.6} sectionThickness={1.1} cellColor="#1d4a7a" sectionColor="#3c78b8" fadeDistance={12} infiniteGrid position={[0, 0, 0]} />
        </>
      )}

      <group ref={partsRef} name="RideOnESP32 parts">
        {model.parts.map((p) => (
          <PowerWheelPart
            key={p.id}
            part={p}
            origin={p.position}
            direction={directions[p.id]}
            explodeFactor={explodeFactor}
            offset={offsets[p.id]}
            visible={visibleSet.has(p.id)}
            visualStyle={visualStyle}
            selected={selected === p.id}
            showTbd={showTbd}
            onSelect={onSelect}
            onHover={onHover}
            registry={registry}
          />
        ))}
      </group>
      <Wiring connections={connections} registry={registry} selectedId={selected} mode={wireMode} visualStyle={visualStyle} groupRef={wiresRef} />
      {showPaths && explodeFactor > 0 && <ProjectionLines ids={visibleIds} registry={registry} visualStyle={visualStyle} />}
      {moving && selectedPart && visibleSet.has(selectedPart.id) && (
        <MoveGizmo
          key={selectedPart.id}
          part={selectedPart}
          base={(() => { const d = directions[selectedPart.id]; const o = selectedPart.position; return [o[0] + d[0] * explodeFactor, o[1] + d[1] * explodeFactor, o[2] + d[2] * explodeFactor]; })()}
          offset={offsets[selectedPart.id] || [0, 0, 0]}
          onLive={(o) => onLiveOffset(selectedPart.id, o)}
          onCommit={(prev) => onCommitOffset(selectedPart.id, prev)}
        />
      )}
      <OrbitControls makeDefault target={[0, 0.35, 0.35]} maxDistance={8} minDistance={0.05} enableDamping />
      <Focus focus={focus} registry={registry} />
    </>
  );
}

export { THREE };
