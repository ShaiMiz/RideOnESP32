import React from 'react';
import { partConnections } from '../data/model.js';
import { COLOUR_NAMES, WIRE_COLOURS } from './Wiring.jsx';
import { StatusChip } from './Sidebar.jsx';

function Issue({ n, base }) {
  return <a href={`${base}${n}`} target="_blank" rel="noreferrer">#{n}</a>;
}

function WireRow({ c, onPick, dir }) {
  const w = c.wire;
  const swatch = WIRE_COLOURS[w.colour] ?? WIRE_COLOURS[''];
  const end = `${c.other.ref}${c.other.pin ? `.${c.other.pin}` : ''}`;
  return (
    <li className="wire">
      <span className="swatch" style={{ background: swatch }} title={COLOUR_NAMES[w.colour] || w.colour || 'by net'} />
      <div>
        <div>
          {dir === 'up' ? (
            <><button className="link" onClick={() => onPick(c.other.ref)}>{end}</button><span className="muted"> → {c.pin.ref}.{c.pin.pin}</span></>
          ) : (
            <><span className="muted">{c.pin.ref}.{c.pin.pin} → </span><button className="link" onClick={() => onPick(c.other.ref)}>{end}</button></>
          )}
          <code>{w.net}</code>
        </div>
        <div className="muted small">
          {w.id}{w.awg ? ` · ${w.awg} AWG` : ''}{w.colour ? ` · ${COLOUR_NAMES[w.colour] || w.colour}` : ''}{w.protectedBy && w.protectedBy !== '-' ? ` · ${w.protectedBy}` : ''}{w.route ? ` · ${w.route}` : ''}
          {c.esp && /^\d+$/.test(c.esp.gpio) && <> · GPIO{c.esp.gpio} {c.esp.signal}</>}
        </div>
        {w.note && <div className="muted small">{w.note}</div>}
      </div>
    </li>
  );
}

// Overlay card for the selected part, from the BOM, the wire list and the design nets.
export default function InfoCard({ part, model, data, onClose, onPick, onFocus, onHide, onIsolate, moving, onMove, onResetOffset, hasOffset }) {
  if (!part) return null;
  const { upstream, downstream, nets } = partConnections(model, data, part);
  const base = data.meta.issuesUrl;
  const SHOWN = ['Line', 'Category', 'Refs', 'Qty', 'Description', 'Part', 'Manufacturer / part number', 'Where', 'Status', 'Notes', 'Note'];
  const bomExtra = part.bom ? Object.entries(part.bom).filter(([k]) => !SHOWN.includes(k)) : [];
  const conn = data.connectors[part.id];
  return (
    <div className="info-card" role="dialog" aria-label={`${part.id} details`}>
      <div className="card-head">
        <div>
          <div className="ref">{part.ref || part.id}{part.value ? <span className="muted"> · {part.value}</span> : null}</div>
          <div className="type">{part.type}</div>
        </div>
        <button className="close" onClick={onClose} aria-label="Close">×</button>
      </div>
      <dl>
        <dt>Model</dt><dd>{part.model || <span className="muted">No part number yet</span>}</dd>
        <dt>Name</dt><dd>{part.name}{conn ? ` (${conn.name})` : ''}</dd>
        <dt>Type</dt><dd>{part.type}{part.category ? <span className="muted"> · BOM category {part.category}</span> : null}</dd>
        <dt>Usage</dt><dd>{part.usage || <span className="muted">No note in the BOM</span>}</dd>
        <dt>Status</dt><dd><StatusChip status={part.status} /> {part.where && <span className="muted">· {part.where}</span>} {part.qty && <span className="muted">· qty {part.qty}</span>} {part.bomLine && <span className="muted">· BOM line {part.bomLine}</span>}</dd>
        {conn && <><dt>Plug</dt><dd>{conn.pins} pins · faces {conn.faces}</dd></>}
        {bomExtra.map(([k, v]) => <React.Fragment key={k}><dt>{k}</dt><dd>{/^https?:/.test(v) ? <a href={v.split(' ')[0]} target="_blank" rel="noreferrer">link</a> : v}</dd></React.Fragment>)}
        {part.source === 'procedural' && <><dt>Source</dt><dd className="muted">Stock car, drawn from the listing size. Not in the BOM.</dd></>}
      </dl>
      {part.tbd.length > 0 && (
        <div className="tbd-box">
          <b>TBD</b>
          <ul>{part.tbd.map((t) => <li key={t}>{t}</li>)}</ul>
          {part.issues.length > 0 && <div>Issues: {part.issues.map((n, i) => <React.Fragment key={n}>{i ? ', ' : ''}<Issue n={n} base={base} /></React.Fragment>)}</div>}
        </div>
      )}
      <h4>Connected from (Point A, upstream) <span className="count">{upstream.length}</span></h4>
      {upstream.length ? <ul className="wires">{upstream.map((c) => <WireRow key={c.wire.id} c={c} dir="up" onPick={onPick} />)}</ul> : <p className="muted small">{downstream.length ? 'Nothing feeds it in the wire list: it is a source or a start point.' : 'No wires in WIRE_LIST.csv.'}</p>}
      <h4>Connected to (Point B, downstream) <span className="count">{downstream.length}</span></h4>
      {downstream.length ? <ul className="wires">{downstream.map((c) => <WireRow key={c.wire.id} c={c} dir="down" onPick={onPick} />)}</ul> : <p className="muted small">{upstream.length ? 'Nothing after it in the wire list: it is a load or an end point.' : 'No wires in WIRE_LIST.csv.'}</p>}
      {!upstream.length && !downstream.length && nets.length > 0 && (
        <>
          <h4>Soldered nets (schematic)</h4>
          <ul className="nets">
            {nets.map((n) => (
              <li key={n.pin}><span className="muted">pin {n.pin}</span> <code>{n.net}</code> {n.peers.slice(0, 6).map((r) => <button key={r} className="link" onClick={() => onPick(r)}>{r}</button>)}{n.peers.length > 6 && <span className="muted"> +{n.peers.length - 6}</span>}</li>
            ))}
          </ul>
        </>
      )}
      <p className="muted small">Direction follows the wire list: From is upstream (Point A), To is downstream (Point B).</p>
      <div className="btn-row">
        <button onClick={onFocus}>Focus</button>
        <button onClick={onHide}>Hide</button>
        <button onClick={onIsolate}>Isolate</button>
        <button className={moving ? 'active' : ''} onClick={onMove}>{moving ? 'Done moving' : 'Move'}</button>
        {hasOffset && <button onClick={onResetOffset}>Reset position</button>}
      </div>
    </div>
  );
}
