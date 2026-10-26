import React, { useState } from 'react';
import { exportGLTF, exportPNG, exportPDF, exportJSON, stamp } from '../export/exporters.js';

const kb = (n) => (n > 1e6 ? `${(n / 1e6).toFixed(1)} MB` : `${Math.round(n / 1024)} kB`);

export function ExportPanel({ onClose, three, partsRef, wiresRef, model, data, state, directions, revisions, revision, say }) {
  const [busy, setBusy] = useState(null);
  const [wires, setWires] = useState(true);
  const [scale, setScale] = useState(2);
  const base = `rideonesp32-exploded-${model.variant.id}-${stamp()}`;
  const extras = { variant: model.variant.name, explode: state.explode, style: state.visualStyle, revision: revision?.label, sources: data.meta };

  const run = async (what, fn) => {
    setBusy(what);
    try {
      const size = await fn();
      say(`${what} saved (${kb(size)})`);
    } catch (e) {
      console.error(e);
      if (e?.code === 'declined') say(`${what}: save cancelled`);
      else say(`${what} failed: ${e?.message || e}`);
    } finally {
      setBusy(null);
    }
  };

  const items = [
    { id: 'glb', title: '3D model (.glb)', text: 'Binary glTF: every visible mesh, its hierarchy and the current explode positions, with part metadata in extras.', go: () => exportGLTF([partsRef.current, wires && wiresRef.current], { binary: true, filename: `${base}.glb`, extras }) },
    { id: 'gltf', title: '3D model (.gltf)', text: 'The same as text JSON, with buffers and textures embedded.', go: () => exportGLTF([partsRef.current, wires && wiresRef.current], { binary: false, filename: `${base}.gltf`, extras }) },
    { id: 'png', title: 'Snapshot (.png)', text: `The current view at ${scale}x screen resolution.`, go: () => exportPNG(three.current, scale, `${base}.png`) },
    { id: 'pdf', title: 'Data sheet (.pdf)', text: 'Snapshot, assembly state, every visible part with its BOM status, and the wire routing between them.', go: () => exportPDF({ three: three.current, model, data, state, revision, filename: `${base}.pdf` }) },
    { id: 'json', title: 'Project data (.json)', text: 'The procedural parts (primitives, explode vectors), the wiring, the state and the revision history.', go: () => exportJSON({ model, directions, state, revisions, data, filename: `${base}.json` }) },
  ];

  return (
    <div className="panel export-panel" role="dialog" aria-label="Export">
      <div className="card-head"><b>Save and export</b><button className="close" onClick={onClose} aria-label="Close">×</button></div>
      <div className="opts">
        <label><input type="checkbox" checked={wires} onChange={(e) => setWires(e.target.checked)} /> Include wiring in 3D files</label>
        <label>PNG scale <select value={scale} onChange={(e) => setScale(Number(e.target.value))}>{[1, 2, 3, 4].map((s) => <option key={s} value={s}>{s}x</option>)}</select></label>
      </div>
      <ul className="exports">
        {items.map((it) => (
          <li key={it.id}>
            <div><b>{it.title}</b><div className="muted small">{it.text}</div></div>
            <button className="primary" disabled={!!busy} onClick={() => run(it.title, it.go)}>{busy === it.title ? 'Saving…' : 'Download'}</button>
          </li>
        ))}
      </ul>
      <p className="muted small">Hidden parts are left out of every export. Positions are illustrative; the box size (#18) and the stock plugs (#17) are still TBD.</p>
    </div>
  );
}

const KIND_ICON = { init: '●', visibility: '◐', explode: '↕', style: '◧', variant: '⇄', wiring: '∿', move: '✥', edit: '•' };

export function Timeline({ revisions, index, onJump, onClose, onClear }) {
  return (
    <div className="panel timeline" role="dialog" aria-label="Revision history">
      <div className="card-head">
        <b>Revision history</b>
        <div className="btn-row">
          <button onClick={() => onJump(index - 1)} disabled={index <= 0} title="Undo (Ctrl+Z)">◀ Back</button>
          <button onClick={() => onJump(index + 1)} disabled={index >= revisions.length - 1} title="Redo (Ctrl+Shift+Z)">Forward ▶</button>
          <button className="close" onClick={onClose} aria-label="Close">×</button>
        </div>
      </div>
      <ol className="revs">
        {revisions.map((r, i) => (
          <li key={r.id} className={`${i === index ? 'current' : ''} ${i > index ? 'future' : ''}`}>
            <button onClick={() => onJump(i)}>
              <span className="kind" title={r.kind}>{KIND_ICON[r.kind] || '•'}</span>
              <span className="lbl">{r.n}. {r.label}</span>
              <time dateTime={r.time}>{new Date(r.time).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' })}</time>
            </button>
          </li>
        )).reverse()}
      </ol>
      <div className="foot">
        <span className="muted small">Kept in this browser only. Click a revision to jump to it; a new change after jumping back replaces the later ones.</span>
        <button onClick={onClear} disabled={revisions.length < 2}>Clear</button>
      </div>
    </div>
  );
}
