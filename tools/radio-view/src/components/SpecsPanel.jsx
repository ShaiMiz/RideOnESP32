import React from 'react';
import { SPECS, BACK_PORTS } from '../lib/tlite.js';

export default function SpecsPanel() {
  return (
    <div className="panel">
      <section className="card">
        <h3>Jumper T-Lite V2 (ELRS)</h3>
        <table className="chans specs">
          <tbody>
            {SPECS.map(([k, v]) => <tr key={k}><td><b>{k}</b></td><td className="small">{v}</td></tr>)}
          </tbody>
        </table>
      </section>
      <section className="card">
        <h3>Back edge, left to right</h3>
        <ul className="small steps">
          <li>SC (2-position) below SA (3-position)</li>
          {BACK_PORTS.filter((p) => p.x < 0).map((p) => <li key={p.name}>{p.name}</li>)}
          <li>Reserved screw hole (FPV monitor holder)</li>
          <li>Micro-SD slot, above the SMA antenna connector</li>
          <li>Reserved screw hole</li>
          {BACK_PORTS.filter((p) => p.x > 0).map((p) => <li key={p.name}>{p.name}</li>)}
          <li>SB (3-position) above SD (2-position)</li>
        </ul>
      </section>
      <p className="muted small">
        Layout from Jumper's components drawing on jumper-rc.com, scaled to the 166 mm body width; sizes and ports from
        Oscar Liang's and Brushless Whoop's reviews; inputs, switches, keys and trims from EdgeTX's own T-Lite hardware file.
        Positions are within about 1.5 mm. Key and trim legends are added by this view; the real keys are blank.
      </p>
    </div>
  );
}
