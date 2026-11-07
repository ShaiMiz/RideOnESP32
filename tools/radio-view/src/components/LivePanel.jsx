import React, { useRef } from 'react';
import { SWITCHES, CHANNELS } from '../lib/model.js';
import { toCrsf } from '../lib/mixer.js';
import { meaning } from '../lib/uses.js';
import { CH, LIMITS } from '../lib/repoData.js';

// A touch-friendly stick pad, for phones and for exact positions.
function Pad({ label, x, y, onChange, springX, springY }) {
  const ref = useRef();
  const set = (e) => {
    const r = ref.current.getBoundingClientRect();
    const nx = Math.max(-1, Math.min(1, ((e.clientX - r.left) / r.width) * 2 - 1));
    const ny = Math.max(-1, Math.min(1, 1 - ((e.clientY - r.top) / r.height) * 2));
    onChange(nx, ny);
  };
  return (
    <div className="pad-wrap">
      <div
        ref={ref}
        className="pad"
        onPointerDown={(e) => { e.currentTarget.setPointerCapture(e.pointerId); set(e); }}
        onPointerMove={(e) => { if (e.buttons) set(e); }}
        onPointerUp={() => onChange(springX ? 0 : x, springY ? 0 : y)}
      >
        <div className="pad-dot" style={{ left: `${(x + 1) * 50}%`, top: `${(1 - y) * 50}%` }} />
      </div>
      <div className="pad-label">{label}</div>
    </div>
  );
}

function Bar({ v }) {
  const pct = (v / 1024) * 50;
  return (
    <div className="bar">
      <div className="bar-fill" style={{ left: `${pct < 0 ? 50 + pct : 50}%`, width: `${Math.abs(pct)}%` }} />
      <div className="bar-mid" />
    </div>
  );
}

const MODE_CLASS = { LOCKED: 'm-locked', KID: 'm-kid', PARENT: 'm-parent', KILLED: 'm-killed' };

export default function LivePanel(p) {
  const { us, out, config, car, sending, power, warningPending, carOn, setCarOn, lq, setLq, batt, setBatt, stickMode, setStickMode, sticks, setSticks, switches, setSwitch, onKey, channelName, onTrim, heldTrim, uses } = p;
  const used = new Set(config.mixes.map((m) => m.ch));
  const shown = [...Array(CHANNELS).keys()].map((i) => i + 1).filter((c) => c <= 8 || used.has(c));
  return (
    <div className="panel">
      <section className="card car">
        <div className="row between">
          <h2>Car</h2>
          <span className="muted small">{!power ? 'radio off' : warningPending ? 'radio holds: switch warning' : sending ? 'radio sending' : ''}</span>
        </div>
        {car && (
          <>
            <div className={`mode ${MODE_CLASS[car.mode] || ''}`}>{car.linkUsable ? car.fm : 'NO LINK'}</div>
            <div className="muted small">{car.note}</div>
            <div className="grid2">
              <div>Command<Bar v={car.command * 1024} /><span className="small">{Math.round(car.command * 100)} %</span></div>
              <div>Steering<Bar v={car.steering * 1024} /><span className="small">{Math.round(car.steering * 100)} %</span></div>
            </div>
            <div className="small">Horn: <b>{car.horn ? 'ON' : 'off'}</b> · Kid cap: {Math.round(car.kidCap * 100)} %{car.linkCap < 1 ? ` · weak link: caps halved` : ''}</div>
          </>
        )}
        <p className="muted small">A picture of the firmware's rules (ChannelMap.h, ModeMachine, ArmingCheck), with no faults and the pedal released. Not the firmware itself.</p>
      </section>

      <section className="card">
        <h2>Radio and link</h2>
        <div className="row wrap">
          <button onClick={() => onKey('POWER')}>{power ? 'Radio off' : 'Radio on'}</button>
          <label className="chk"><input type="checkbox" checked={carOn} onChange={(e) => setCarOn(e.target.checked)} /> Car on, in range</label>
          <label className="chk">Stick mode
            <select value={stickMode} onChange={(e) => setStickMode(Number(e.target.value))}>
              <option value={2}>2 (required)</option>
              <option value={1}>1</option>
            </select>
          </label>
        </div>
        <label className="slider">Link quality {lq} %
          <input type="range" min={0} max={100} value={lq} onChange={(e) => setLq(Number(e.target.value))} />
        </label>
        <label className="slider">Battery {batt.toFixed(1)} V
          <input type="range" min={18} max={29.4} step={0.1} value={batt} onChange={(e) => setBatt(Number(e.target.value))} />
        </label>
      </section>

      <section className="card">
        <h2>Sticks and switches</h2>
        <div className="row pads">
          <Pad label="Left stick" x={sticks.lh} y={sticks.lv} springX onChange={(x, y) => setSticks((s) => ({ ...s, lh: x, lv: y }))} />
          <Pad label="Right stick" x={sticks.rh} y={sticks.rv} springX springY onChange={(x, y) => setSticks((s) => ({ ...s, rh: x, rv: y }))} />
        </div>
        <div className="switches">
          {Object.entries(SWITCHES).map(([sw, n]) => (
            <div key={sw} className="sw">
              <b>{sw}</b>
              {[...Array(n).keys()].map((pos) => (
                <button key={pos} className={switches[sw] === pos ? 'seg on' : 'seg'} onClick={() => setSwitch(sw, pos)}>
                  {n === 2 ? ['up', 'down'][pos] : ['up', 'mid', 'down'][pos]}
                </button>
              ))}
            </div>
          ))}
        </div>
        <div className="sw trims">
          <b title="Trim buttons: push-buttons, on only while held">Trim</b>
          {['T1-', 'T1+', 'T2-', 'T2+', 'T3-', 'T3+', 'T4-', 'T4+'].map((t) => {
            const u = (uses?.[t] || []).map((x) => `CH${x.ch} ${x.label}`).join(', ');
            return (
              <button key={t} className={heldTrim === t ? 'seg on' : 'seg'} title={u ? `${u}, while held` : 'spare'} style={{ touchAction: 'none' }}
                onPointerDown={(e) => { e.preventDefault(); onTrim(t, true); }} onPointerUp={() => onTrim(t, false)} onPointerLeave={() => heldTrim === t && onTrim(t, false)}>
                {t}{u ? ` ${u.replace(/^CH\d+ /, '')}` : ''}
              </button>
            );
          })}
        </div>
        <p className="muted small">Hold a trim button to press it (or hold H for T1+). The trims are off in this model, so the buttons are free; T1, the pair under the left stick, is the horn.</p>
      </section>

      <section className="card">
        <h2>CRSF channels out</h2>
        <table className="chans">
          <thead><tr><th>CH</th><th>Does</th><th /><th>µs</th><th>CRSF</th><th>The car reads</th></tr></thead>
          <tbody>
            {shown.map((c) => (
              <tr key={c} className={sending ? '' : 'stale'}>
                <td>{c}</td>
                <td className="small">{channelName(c)}</td>
                <td className="barcell"><Bar v={out.channels[c - 1]} /></td>
                <td className="num">{us[c - 1]}</td>
                <td className="num muted">{toCrsf(out.channels[c - 1])}</td>
                <td className="small">{meaning(c, us[c - 1], CH, LIMITS)}</td>
              </tr>
            ))}
          </tbody>
        </table>
        <p className="muted small">
          µs as AlfredoCRSF hands them to the ESP32 ({LIMITS.low} to {LIMITS.high} nominal; EdgeTX's CRSF rounding gives 989 to 2011).
          ELRS Hybrid mode sends CH5 as on/off and CH6 to CH12 in a few steps, so the car sees the nearest step of a switch channel.
          {!sending && ' The radio is not sending, so the car ignores these values.'}
        </p>
      </section>
    </div>
  );
}
