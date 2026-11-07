import React, { useMemo, useRef, useState } from 'react';
import { toYaml } from '../lib/model.js';
import { diffFromRepo } from '../lib/rules.js';
import { REPO, REPO_YML, CH } from '../lib/repoData.js';
import { roleOf } from '../lib/uses.js';
import { download } from '../lib/download.js';
import { Checks } from './MappingPanel.jsx';

export default function ExportPanel({ base, config, rules, importModel, reset, say }) {
  const yaml = useMemo(() => toYaml(base, config), [base, config]);
  const diff = useMemo(() => diffFromRepo(config, REPO.config), [config]);
  const same = yaml === REPO_YML;
  const blocked = rules.errors.length > 0;
  const [paste, setPaste] = useState('');
  const [err, setErr] = useState('');
  const fileRef = useRef();
  const preRef = useRef();
  const firmwareChans = diff.filter((d) => typeof d.ch === 'number' && roleOf(d.ch, CH));
  // rideon.lua's top line reads SC, SA and SB directly
  const luaMoved = ['KILL', 'MODE', 'SPEED_CAP'].some((r) => {
    const want = { KILL: 'SC', MODE: 'SA', SPEED_CAP: 'SB' }[r];
    return !config.mixes.some((m) => m.ch === CH[r] && m.src === want);
  });

  const save = async () => {
    try {
      const r = await download(new Blob([yaml], { type: 'text/yaml' }), 'model42.yml');
      say(r === 'zip' ? 'Saved model42.yml.zip: unzip it to get model42.yml.' : 'Saved model42.yml.');
    } catch (e) {
      say(e?.code === 'declined' ? 'Save cancelled.' : `Couldn't save: ${e?.message || e}`);
    }
  };
  const copy = async () => {
    try {
      await navigator.clipboard.writeText(yaml);
      say('Copied the model file.');
    } catch {
      const sel = window.getSelection();
      const range = document.createRange();
      range.selectNodeContents(preRef.current);
      sel.removeAllRanges();
      sel.addRange(range);
      say('Clipboard blocked here: the text is selected, press Ctrl+C.');
    }
  };
  const tryLoad = (text) => {
    try {
      importModel(text);
      setErr('');
      setPaste('');
    } catch (e) {
      setErr(String(e.message || e));
    }
  };

  return (
    <div className="panel">
      <Checks rules={rules} />
      <section className="card">
        <h3>Changes from the repo model</h3>
        {same ? <div className="small">None: this is radio/edgetx/sdcard/MODELS/model42.yml exactly.</div> : (
          <table className="chans">
            <thead><tr><th /><th>Repo</th><th>Now</th></tr></thead>
            <tbody>
              {diff.map((d) => (
                <tr key={String(d.ch)}><td>{typeof d.ch === 'number' ? `CH${d.ch}` : d.ch === 'L' ? 'L' : 'Start-up'}</td><td className="small">{d.from}</td><td className="small">{d.to}</td></tr>
              ))}
              {!diff.length && <tr><td colSpan={3} className="small">Only the text layout differs.</td></tr>}
            </tbody>
          </table>
        )}
        {firmwareChans.length > 0 && (
          <p className="warn small">
            ! This moves a channel the car reads ({firmwareChans.map((d) => `CH${d.ch}`).join(', ')}). CI (tools/check_radio_files.py) will fail until
            docs/firmware/CHANNEL_MAP.md, radio/MODEL_SETUP.md and the EXPECTED table in check_radio_files.py say the same, in the same PR.
          </p>
        )}
        {luaMoved && <p className="warn small">! rideon.lua's top line reads SC, SA and SB itself; change its switchText() too, or the screen shows the wrong switch.</p>}
      </section>

      <section className="card">
        <h3>Save</h3>
        <div className="row wrap">
          <button className="primary" disabled={blocked} onClick={save}>Save model42.yml</button>
          <button disabled={blocked} onClick={copy}>Copy</button>
          <button onClick={reset}>Reset to the repo model</button>
        </div>
        {blocked && <p className="err small">Fix the safety problems above first.</p>}
        <ol className="small steps">
          <li>Put the file at <code>radio/edgetx/sdcard/MODELS/model42.yml</code> in your clone (replace the old one).</li>
          <li>Run <code>python tools/check_radio_files.py</code> (needs <code>pip install pyyaml</code>). It must say OK.</li>
          <li>Radio in USB Storage mode, then double-click <code>radio/install_radio.cmd</code>. It backs the SD card up first, then installs the model.</li>
          <li>Commit it on a branch and open a PR, so the repo and the radio stay the same.</li>
          <li>On the radio, redo the phase 2 bench checks in radio/README.md, wheels off the ground.</li>
        </ol>
      </section>

      <section className="card">
        <h3>Load a model file</h3>
        <div className="row wrap">
          <button onClick={() => fileRef.current?.click()}>Open a .yml file…</button>
          <input ref={fileRef} type="file" accept=".yml,.yaml,.txt" hidden onChange={async (e) => { const f = e.target.files?.[0]; if (f) tryLoad(await f.text()); e.target.value = ''; }} />
        </div>
        <textarea rows={4} placeholder="…or paste a model file here (for example one copied off the radio's SD card, MODELS/)" value={paste} onChange={(e) => setPaste(e.target.value)} />
        <button disabled={!paste.trim()} onClick={() => tryLoad(paste)}>Load pasted text</button>
        {err && <p className="err small">{err}</p>}
      </section>

      <section className="card">
        <h3>The file</h3>
        <pre ref={preRef} className="yaml">{yaml}</pre>
      </section>
    </div>
  );
}
