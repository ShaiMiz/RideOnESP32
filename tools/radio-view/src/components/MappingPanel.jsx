import React from 'react';
import { SOURCES, SOURCE_LABEL, MLTPX, LS_FUNCS, LS_COUNT, CHANNELS, NAME_LEN, SWITCHES, conditions } from '../lib/model.js';
import { CH } from '../lib/repoData.js';
import { channelLabel, roleOf } from '../lib/uses.js';

const NEW_MIX = { src: 'MAX', weight: 100, offset: 0, swtch: 'NONE', noTrim: false, mltpx: 'ADD', name: '' };

export function Checks({ rules }) {
  return (
    <div className={`checks ${rules.errors.length ? 'bad' : 'good'}`}>
      {rules.errors.length === 0 ? <div><b>Safety checks pass.</b> Kill, start-up check, sticks let go = stop, Locked reachable.</div> : <div><b>{rules.errors.length} safety problem{rules.errors.length > 1 ? 's' : ''}: export is blocked.</b></div>}
      {rules.errors.map((e, i) => <div key={`e${i}`} className="err">✕ {e}</div>)}
      {rules.warnings.map((w, i) => <div key={`w${i}`} className="warn">! {w}</div>)}
    </div>
  );
}

export default function MappingPanel({ config, setConfig, rules, reset }) {
  const update = (fn) => setConfig((c) => {
    const n = structuredClone(c);
    fn(n);
    return n;
  });
  const used = new Set(config.mixes.map((m) => m.ch));
  const [extra, setExtra] = React.useState(9);
  const chans = [...Array(CHANNELS).keys()].map((i) => i + 1).filter((c) => c <= 8 || used.has(c));
  const free = [...Array(CHANNELS).keys()].map((i) => i + 1).filter((c) => !chans.includes(c));
  const conds = conditions();

  return (
    <div className="panel">
      <Checks rules={rules} />
      <div className="row wrap">
        <button onClick={reset}>Reset to the repo model</button>
        <button onClick={() => update((c) => { c.mixes = []; })}>Clear all mixes</button>
      </div>
      <p className="muted small">
        These are the radio's settings: which stick or switch drives each channel. The car always reads the same channel numbers
        (lib/rideon_core/src/ChannelMap.h), so moving a function to a different <i>channel</i> needs a firmware change; moving it to a different <i>control</i> doesn't.
      </p>

      {chans.map((c) => {
        const lines = config.mixes.map((m, i) => [m, i]).filter(([m]) => m.ch === c);
        const role = roleOf(c, CH);
        return (
          <section key={c} className={`card mixcard ${role ? '' : 'spare'}`}>
            <div className="row between">
              <h3>CH{c} · {channelLabel(c, CH, config)}</h3>
              <div className="row">
                <button className="small-btn" onClick={() => update((n) => { n.mixes.push({ ...NEW_MIX, ch: c }); })}>+ line</button>
                {lines.length > 0 && <button className="small-btn" onClick={() => update((n) => { n.mixes = n.mixes.filter((m) => m.ch !== c); })}>Clear</button>}
              </div>
            </div>
            {!lines.length && <div className="muted small">No mix: the channel sits at 1500 µs.</div>}
            {lines.map(([m, i]) => (
              <div key={i} className="mixline">
                <select value={m.mltpx} title="How this line combines with the ones above" onChange={(e) => update((n) => { n.mixes[i].mltpx = e.target.value; })}>
                  {MLTPX.map((x) => <option key={x} value={x}>{x === 'ADD' ? '+=' : x === 'MUL' ? '*=' : ':='} {x}</option>)}
                </select>
                <select value={m.src} title={SOURCE_LABEL[m.src]} onChange={(e) => update((n) => { n.mixes[i].src = e.target.value; n.mixes[i].noTrim = ['Rud', 'Ele', 'Thr', 'Ail'].includes(e.target.value) || n.mixes[i].noTrim; })}>
                  {SOURCES.map((s) => <option key={s} value={s}>{s}</option>)}
                </select>
                <label title="Weight, %">w<input type="number" min={-100} max={100} value={m.weight} onChange={(e) => update((n) => { n.mixes[i].weight = Math.max(-100, Math.min(100, Math.round(Number(e.target.value) || 0))); })} /></label>
                <label title="Offset, %">off<input type="number" min={-100} max={100} value={m.offset} onChange={(e) => update((n) => { n.mixes[i].offset = Math.max(-100, Math.min(100, Math.round(Number(e.target.value) || 0))); })} /></label>
                <select value={m.swtch} title="Only while this logical switch is on" onChange={(e) => update((n) => { n.mixes[i].swtch = e.target.value; })}>
                  {conds.map((s) => <option key={s} value={s}>{s === 'NONE' ? 'always' : `if ${s}`}</option>)}
                </select>
                <input className="name" maxLength={NAME_LEN} value={m.name} placeholder="name" onChange={(e) => update((n) => { n.mixes[i].name = e.target.value.replace(/[^A-Za-z0-9 %+-]/g, ''); })} />
                <button className="x" title="Delete line" onClick={() => update((n) => { n.mixes.splice(i, 1); })}>✕</button>
                <button className="x" title="Invert" onClick={() => update((n) => { n.mixes[i].weight = -n.mixes[i].weight; })}>±</button>
              </div>
            ))}
          </section>
        );
      })}
      {free.length > 0 && (
        <div className="row">
          <select value={extra} onChange={(e) => setExtra(Number(e.target.value))}>
            {free.map((c) => <option key={c} value={c}>CH{c}</option>)}
          </select>
          <button onClick={() => update((n) => { n.mixes.push({ ...NEW_MIX, ch: free.includes(extra) ? extra : free[0], src: 'SD' }); })}>Add a spare channel</button>
          <span className="muted small">The firmware reads CH1 to CH8 only; spares are for later accessories.</span>
        </div>
      )}

      <section className="card">
        <h3>Logical switches</h3>
        <p className="muted small">Conditions for mix lines. The repo uses L1 = left stick sideways past half way, for the horn.</p>
        {[...Array(LS_COUNT).keys()].map((idx) => {
          const ls = config.logical.find((l) => l.idx === idx);
          const set = (patch) => update((n) => {
            const cur = n.logical.find((l) => l.idx === idx);
            if (cur) Object.assign(cur, patch);
            else n.logical.push({ idx, func: 'FUNC_APOS', a: 'Rud', x: 50, andsw: 'NONE', delay: 0, duration: 0, ...patch });
          });
          return (
            <div key={idx} className="mixline">
              <b className="lsn">L{idx + 1}</b>
              <select value={ls?.func || ''} onChange={(e) => (e.target.value ? set({ func: e.target.value }) : update((n) => { n.logical = n.logical.filter((l) => l.idx !== idx); }))}>
                <option value="">off</option>
                {Object.entries(LS_FUNCS).map(([k, t]) => <option key={k} value={k}>{t}</option>)}
              </select>
              {ls && (
                <>
                  <select value={ls.a} onChange={(e) => set({ a: e.target.value })}>
                    {SOURCES.filter((s) => s !== 'MAX').map((s) => <option key={s} value={s}>{s}</option>)}
                  </select>
                  <label>x<input type="number" min={-100} max={100} value={ls.x} onChange={(e) => set({ x: Math.max(-100, Math.min(100, Math.round(Number(e.target.value) || 0))) })} />%</label>
                </>
              )}
            </div>
          );
        })}
      </section>

      <section className="card">
        <h3>Start-up switch check</h3>
        <p className="muted small">At power-on the radio won't send until these switches are where it asks. EdgeTX lets any key skip it, so the firmware's own boot rule (Locked first) stays the real guard.</p>
        {Object.entries(SWITCHES).map(([sw, n]) => (
          <div key={sw} className="sw">
            <b>{sw}</b>
            {(n === 2 ? ['', 'u', 'd'] : ['', 'u', 'm', 'd']).map((p) => (
              <button key={p || 'off'} className={(config.switchWarning[sw] || '') === p ? 'seg on' : 'seg'} onClick={() => update((c) => { c.switchWarning[sw] = p; })}>
                {{ '': 'not checked', u: 'up', m: 'mid', d: 'down' }[p]}
              </button>
            ))}
          </div>
        ))}
      </section>
      <p className="muted small">Not editable here, carried through from the file as is: the model name and receiver number 42, trims (off), the ELRS module, telemetry sensors, the telemetry screen and logging. The keys (RTN, ENT, SYS, MDL, ▲ ▼) are fixed by EdgeTX and can't be remapped in a model.</p>
    </div>
  );
}
