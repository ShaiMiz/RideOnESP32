import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { Canvas } from '@react-three/fiber';
import data from './data/project.json';
import { buildModel, VARIANTS } from './data/model.js';
import { explodeDirections } from './data/explode.js';
import Scene from './components/Scene.jsx';
import Sidebar from './components/Sidebar.jsx';
import InfoCard from './components/InfoCard.jsx';
import { ExportPanel, Timeline } from './components/Panels.jsx';
import { WIRE_COLOURS, COLOUR_NAMES } from './components/Wiring.jsx';

const STORE = 'rideonesp32-exploded-view/v1';
const INITIAL = { variant: 'A', hidden: [], explode: 0, offsets: {}, wireMode: 'all' };

function load() {
  try {
    const s = JSON.parse(localStorage.getItem(STORE) || 'null');
    if (s && Array.isArray(s.revisions) && s.revisions.length) return s;
  } catch { /* storage blocked: start fresh */ }
  return null;
}

export default function App() {
  const saved = useMemo(load, []);
  const last = saved ? saved.revisions[saved.index] : null;
  const [visualStyle, setVisualStyle] = useState(last?.snapshot.visualStyle || 'realistic');
  const [assembly, setAssembly] = useState(last?.snapshot.assembly || INITIAL);
  const [revisions, setRevisions] = useState(saved?.revisions || []);
  const [revIndex, setRevIndex] = useState(saved ? saved.index : -1);
  const [liveExplode, setLiveExplode] = useState(null);
  const [liveOffset, setLiveOffset] = useState(null);
  const [selected, setSelected] = useState(null);
  const [hovered, setHovered] = useState(null);
  const [focus, setFocus] = useState(null);
  const [moving, setMoving] = useState(false);
  const [showTbd, setShowTbd] = useState(true);
  const [showPaths, setShowPaths] = useState(true);
  const [panel, setPanel] = useState(null);
  const [sidebarOpen, setSidebarOpen] = useState(() => window.innerWidth > 760);
  const [toast, setToast] = useState(null);

  const registry = useMemo(() => new Map(), []);
  const partsRef = useRef();
  const wiresRef = useRef();
  const threeRef = useRef();

  const model = useMemo(() => buildModel(data, assembly.variant), [assembly.variant]);
  const hidden = useMemo(() => new Set(assembly.hidden), [assembly.hidden]);
  const directions = useMemo(() => explodeDirections(model.parts, hidden), [model, hidden]);
  const explode = liveExplode ?? assembly.explode;
  const offsets = useMemo(() => (liveOffset ? { ...assembly.offsets, [liveOffset.id]: liveOffset.o } : assembly.offsets), [assembly.offsets, liveOffset]);
  const byId = useMemo(() => Object.fromEntries(model.parts.map((p) => [p.id, p])), [model]);
  const selectedPart = selected ? byId[selected] : null;

  // Every committed change becomes a revision; jumping back and changing again drops the redo tail.
  const commit = useCallback((label, nextAssembly, nextStyle = visualStyle, kind = 'edit') => {
    setAssembly(nextAssembly);
    setVisualStyle(nextStyle);
    setRevisions((revs) => {
      const head = revs.slice(0, revIndex + 1);
      const n = (head.at(-1)?.n || 0) + 1;
      const rev = { id: `${Date.now().toString(36)}-${n}`, n, time: new Date().toISOString(), label, kind, snapshot: { visualStyle: nextStyle, assembly: nextAssembly } };
      const out = [...head, rev].slice(-200);
      setRevIndex(out.length - 1);
      return out;
    });
  }, [revIndex, visualStyle]);

  useEffect(() => {
    if (revisions.length === 0) {
      const rev = { id: 'init', n: 1, time: new Date().toISOString(), label: `Assembly loaded from BOM + wire list (${data.meta.gitRef})`, kind: 'init', snapshot: { visualStyle, assembly } };
      setRevisions([rev]);
      setRevIndex(0);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    try { localStorage.setItem(STORE, JSON.stringify({ revisions, index: revIndex })); } catch { /* not saved: fine */ }
  }, [revisions, revIndex]);

  const jump = useCallback((i) => {
    const r = revisions[i];
    if (!r) return;
    setAssembly(r.snapshot.assembly);
    setVisualStyle(r.snapshot.visualStyle);
    setRevIndex(i);
    setLiveExplode(null);
    setLiveOffset(null);
  }, [revisions]);

  const say = (t) => { setToast(t); setTimeout(() => setToast(null), 2600); };

  // ---- actions ----
  const toggleParts = useCallback((ids, show, what) => {
    const h = new Set(assembly.hidden);
    ids.forEach((id) => (show ? h.delete(id) : h.add(id)));
    commit(`${show ? 'Showed' : 'Hid'} ${what}${ids.length > 1 ? ` (${ids.length} parts)` : ''}`, { ...assembly, hidden: [...h] }, visualStyle, 'visibility');
  }, [assembly, commit, visualStyle]);

  const onSelect = useCallback((p, andFocus) => {
    setSelected(p ? p.id : null);
    if (p && andFocus) setFocus({ id: p.id, radius: p.radius, t: Date.now() });
  }, []);
  const onHover = useCallback((p) => setHovered(p ? p.id : null), []);

  const pick = (ref) => {
    const p = byId[ref];
    if (!p) return say(`${ref} is not in this variant`);
    if (hidden.has(ref)) toggleParts([ref], true, ref);
    onSelect(p, true);
  };

  const setStyle = (s) => { if (s !== visualStyle) commit(`Style: ${s}`, assembly, s, 'style'); };
  const setVariant = (v) => {
    if (v === assembly.variant) return;
    const next = buildModel(data, v);
    const before = new Set(model.parts.map((p) => p.id));
    const after = new Set(next.parts.map((p) => p.id));
    const added = [...after].filter((x) => !before.has(x)).length;
    const removed = [...before].filter((x) => !after.has(x)).length;
    commit(`Drive swapped to ${VARIANTS[v].name} (+${added} / −${removed} parts, wiring re-routed)`, { ...assembly, variant: v, hidden: assembly.hidden.filter((id) => after.has(id)) }, visualStyle, 'variant');
    if (selected && !after.has(selected)) setSelected(null);
  };
  const commitExplode = () => {
    if (liveExplode === null) return;
    if (Math.abs(liveExplode - assembly.explode) > 0.001) commit(`Explode factor ${assembly.explode.toFixed(2)} → ${liveExplode.toFixed(2)}`, { ...assembly, explode: liveExplode }, visualStyle, 'explode');
    setLiveExplode(null);
  };
  const setWireMode = (m) => commit(`Wiring: ${m === 'all' ? 'all wires' : m === 'selected' ? 'selected part only' : 'off'}`, { ...assembly, wireMode: m }, visualStyle, 'wiring');
  const onLiveOffset = (id, o) => setLiveOffset({ id, o });
  const onCommitOffset = (id) => {
    if (!liveOffset || liveOffset.id !== id) return;
    const o = liveOffset.o.map((v) => Math.round(v * 1000) / 1000);
    setLiveOffset(null);
    commit(`Moved ${id} to offset (${o.map((v) => (v * 100).toFixed(1)).join(', ')}) cm`, { ...assembly, offsets: { ...assembly.offsets, [id]: o } }, visualStyle, 'move');
  };
  const resetOffset = (id) => {
    const { [id]: _, ...rest } = assembly.offsets;
    commit(`Reset ${id} position`, { ...assembly, offsets: rest }, visualStyle, 'move');
  };
  const showAll = () => commit('Showed all parts', { ...assembly, hidden: [] }, visualStyle, 'visibility');
  const hideAll = () => commit('Hid all parts', { ...assembly, hidden: model.parts.map((p) => p.id) }, visualStyle, 'visibility');
  const isolate = (id = selected) => id && commit(`Isolated ${id}`, { ...assembly, hidden: model.parts.filter((p) => p.id !== id).map((p) => p.id) }, visualStyle, 'visibility');

  useEffect(() => {
    const k = (e) => {
      if (e.target.tagName === 'INPUT') return;
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') { e.preventDefault(); jump(e.shiftKey ? revIndex + 1 : revIndex - 1); }
      if (e.key === 'Escape') { setSelected(null); setMoving(false); }
    };
    window.addEventListener('keydown', k);
    return () => window.removeEventListener('keydown', k);
  }, [jump, revIndex]);

  const state = { visualStyle, explode, hidden, variant: assembly.variant, offsets: assembly.offsets, wireMode: assembly.wireMode };
  const visibleCount = model.parts.length - model.parts.filter((p) => hidden.has(p.id)).length;
  const rev = revisions[revIndex];

  return (
    <div className={`app ${visualStyle}`}>
      <header className="topbar">
        <button className="icon-btn" onClick={() => setSidebarOpen((v) => !v)} aria-label="Parts list">☰</button>
        <div className="title">
          <b>RideOnESP32</b> <span className="muted">{data.car.Model || 'Can-Am Maverick X3 24V Ride-On Buggy'} · exploded view</span>
        </div>
        <div className="seg" role="group" aria-label="Visual style">
          <button className={visualStyle === 'realistic' ? 'on' : ''} onClick={() => setStyle('realistic')}>Realistic</button>
          <button className={visualStyle === 'blueprint' ? 'on' : ''} onClick={() => setStyle('blueprint')}>Blueprint</button>
        </div>
        <label className="slider">
          <span>Explode</span>
          <input type="range" min="0" max="1.5" step="0.01" value={explode}
            onChange={(e) => setLiveExplode(Number(e.target.value))}
            onPointerUp={commitExplode} onKeyUp={commitExplode} onBlur={commitExplode} />
          <span className="num">{explode.toFixed(2)}</span>
        </label>
        <select value={assembly.variant} onChange={(e) => setVariant(e.target.value)} aria-label="Drive variant" title={VARIANTS[assembly.variant].blurb}>
          {Object.values(VARIANTS).map((v) => <option key={v.id} value={v.id}>{v.name}</option>)}
        </select>
        <select value={assembly.wireMode} onChange={(e) => setWireMode(e.target.value)} aria-label="Wiring">
          <option value="all">Wires: all</option>
          <option value="selected">Wires: selected part</option>
          <option value="none">Wires: off</option>
        </select>
        <div className="spacer" />
        <button className={panel === 'history' ? 'on' : ''} onClick={() => setPanel(panel === 'history' ? null : 'history')}>History <span className="count">{revisions.length}</span></button>
        <button className={panel === 'export' ? 'on primary' : 'primary'} onClick={() => setPanel(panel === 'export' ? null : 'export')}>Export</button>
      </header>

      <div className="main">
        {sidebarOpen && (
          <Sidebar model={model} hidden={hidden} selected={selected} onToggleParts={toggleParts} onSelect={(p, f) => { onSelect(p, f); if (window.innerWidth <= 760) setSidebarOpen(false); }}
            onShowAll={showAll} onHideAll={hideAll} onIsolate={() => isolate()} />
        )}
        <div className="viewport">
          <Canvas shadows dpr={[1, 2]} gl={{ preserveDrawingBuffer: true, antialias: true }} camera={{ position: [1.9, 1.35, 2.3], fov: 40, near: 0.01, far: 60 }}
            onPointerMissed={() => { if (!moving) setSelected(null); }}>
            <Scene model={model} directions={directions} hidden={hidden} explodeFactor={explode} offsets={offsets} visualStyle={visualStyle}
              selected={selected} onSelect={onSelect} onHover={onHover} wireMode={assembly.wireMode} showTbd={showTbd} showPaths={showPaths}
              registry={registry} partsRef={partsRef} wiresRef={wiresRef} threeRef={threeRef} focus={focus} moving={moving}
              onLiveOffset={onLiveOffset} onCommitOffset={onCommitOffset} />
          </Canvas>

          {visualStyle === 'blueprint' && (
            <div className="title-block" aria-hidden="true">
              <div><span>PROJECT</span>RideOnESP32 v3</div>
              <div><span>DRAWING</span>Exploded assembly</div>
              <div><span>DRIVE</span>{model.variant.name.replace('Variant ', '')}</div>
              <div><span>REV</span>{rev ? rev.n : 1}</div>
              <div><span>SCALE</span>NTS · metres</div>
              <div><span>DATA</span>{data.meta.gitRef.replace('origin/', '')}</div>
            </div>
          )}

          <div className="hud">
            <label><input type="checkbox" checked={showTbd} onChange={(e) => setShowTbd(e.target.checked)} /> TBD markers</label>
            <label><input type="checkbox" checked={showPaths} onChange={(e) => setShowPaths(e.target.checked)} /> Disassembly paths</label>
            <span className="muted">{visibleCount}/{model.parts.length} parts · {model.connections.length} wires</span>
            {hovered && byId[hovered] && <span className="hover-name">{byId[hovered].ref || ''} {byId[hovered].name}</span>}
          </div>

          {assembly.wireMode !== 'none' && (
            <details className="legend">
              <summary>Wire colours</summary>
              <ul>{Object.entries(COLOUR_NAMES).map(([k, v]) => <li key={k}><span className="swatch" style={{ background: WIRE_COLOURS[k] }} />{v}</li>)}</ul>
            </details>
          )}

          <InfoCard part={selectedPart} model={model} data={data} onClose={() => { setSelected(null); setMoving(false); }} onPick={pick}
            onFocus={() => selectedPart && setFocus({ id: selectedPart.id, radius: selectedPart.radius, t: Date.now() })}
            onHide={() => { toggleParts([selected], false, selected); setSelected(null); }}
            onIsolate={() => isolate()} moving={moving} onMove={() => setMoving((m) => !m)}
            hasOffset={!!assembly.offsets[selected]} onResetOffset={() => resetOffset(selected)} />

          {panel === 'export' && (
            <ExportPanel onClose={() => setPanel(null)} three={threeRef} partsRef={partsRef} wiresRef={wiresRef} model={model} data={data}
              state={state} directions={directions} revisions={revisions} revision={rev} say={say} />
          )}
          {panel === 'history' && (
            <Timeline revisions={revisions} index={revIndex} onJump={jump} onClose={() => setPanel(null)}
              onClear={() => { const r = { ...revisions[revIndex], n: 1, id: 'init', label: 'History cleared: current assembly kept', kind: 'init', time: new Date().toISOString() }; setRevisions([r]); setRevIndex(0); }} />
          )}
          {toast && <div className="toast" role="status">{toast}</div>}
        </div>
      </div>
    </div>
  );
}
