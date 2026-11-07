import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { Canvas, useThree } from '@react-three/fiber';
import { OrbitControls, ContactShadows, Environment, Lightformer } from '@react-three/drei';
import Radio3D from './components/Radio3D.jsx';
import LivePanel from './components/LivePanel.jsx';
import MappingPanel from './components/MappingPanel.jsx';
import ExportPanel from './components/ExportPanel.jsx';
import { REPO, CH, LIMITS } from './lib/repoData.js';
import { SWITCHES, load, toYaml } from './lib/model.js';
import { mix, toUs } from './lib/mixer.js';
import { decode, ModeMachine } from './lib/firmware.js';
import { check, warningPositions } from './lib/rules.js';
import { controlUses, channelLabel } from './lib/uses.js';
import { makeCanvas, draw, PAGES } from './lib/lcd.js';
import { beepsFor, enableSound } from './lib/sound.js';

const STORE = 'rideon-radio-view-v1';
const START_SWITCHES = { SA: 0, SB: 0, SC: 0, SD: 0 };
const REST = { lh: 0, lv: -1, rh: 0, rv: 0 }; // the left stick's up/down rests where it was left: start at the bottom

function stored() {
  try {
    const s = JSON.parse(localStorage.getItem(STORE) || 'null');
    if (s?.yaml) return { ...load(s.yaml), yaml: s.yaml };
  } catch { /* storage blocked or bad: start from the repo model */ }
  return null;
}

// Moves the camera back on narrow (portrait) screens so the whole radio fits.
function Fit() {
  const { camera, size } = useThree();
  useEffect(() => {
    const aspect = size.width / size.height;
    const dist = aspect < 1.25 ? 2.9 * Math.min(2.2, 1.25 / aspect) : 2.9;
    camera.position.set(0, 0.9 * (dist / 2.9), dist);
    camera.updateProjectionMatrix();
  }, [camera, size]);
  return null;
}

export default function App() {
  const saved = useMemo(stored, []);
  const [base, setBase] = useState(saved?.tree || REPO.tree);
  const [config, setConfig] = useState(saved?.config || REPO.config);
  const [sticks, setSticks] = useState(REST);
  const [switches, setSwitches] = useState(START_SWITCHES);
  const [stickMode, setStickMode] = useState(2);
  const [power, setPower] = useState(true);
  const [warningPending, setWarningPending] = useState(true);
  const [carOn, setCarOn] = useState(true);
  const [lq, setLq] = useState(100);
  const [batt, setBatt] = useState(25.2);
  const [sound, setSound] = useState(false);
  const [showLabels, setShowLabels] = useState(true);
  const [page, setPage] = useState(0);
  const [chPage, setChPage] = useState(0);
  const [mixScroll, setMixScroll] = useState(0);
  const [tab, setTab] = useState('live');
  const [pressed, setPressed] = useState(null);
  const [toast, setToast] = useState('');
  const [dragging, setDragging] = useState(null);
  const [ready, setReady] = useState(false); // drei's Html needs the canvas in the page first
  const [car, setCar] = useState(null);
  const [blink, setBlink] = useState(true);
  const [lcdVersion, setLcdVersion] = useState(0);
  const lcd = useMemo(makeCanvas, []);
  const machine = useRef(new ModeMachine());
  const capa = useRef(0);

  useEffect(() => {
    try { localStorage.setItem(STORE, JSON.stringify({ yaml: toYaml(base, config) })); } catch { /* fine */ }
  }, [base, config]);

  const say = useCallback((t) => {
    setToast(t);
    clearTimeout(say.timer);
    say.timer = setTimeout(() => setToast(''), 3500);
  }, []);

  // ---- radio ------------------------------------------------------------------------------
  const input = useMemo(() => ({ sticks, switches, stickMode }), [sticks, switches, stickMode]);
  const out = useMemo(() => mix(config, input), [config, input]);
  const us = useMemo(() => out.channels.map(toUs), [out]);
  const wp = useMemo(() => warningPositions(config), [config]);
  const warnList = Object.entries(wp).filter(([, p]) => p !== null).map(([sw, p]) => ({
    sw, ok: switches[sw] === p, text: `${sw}${SWITCHES[sw] === 2 ? (p ? '↓' : '↑') : ['↑', '-', '↓'][p]}`,
  }));
  const warningOk = warnList.every((w) => w.ok);
  useEffect(() => { if (power && warningPending && warningOk) setWarningPending(false); }, [power, warningPending, warningOk]);
  const sending = power && !warningPending;
  const linkUp = sending && carOn;
  const rules = useMemo(() => check(config, CH, LIMITS, stickMode), [config, stickMode]);
  const uses = useMemo(() => controlUses(config, input, CH, LIMITS), [config, input]);

  // ---- car (firmware picture), 20 times a second -------------------------------------------
  const live = useRef({});
  live.current = { us, linkUp, lq, batt, sound };
  useEffect(() => {
    let last = null;
    let lastUp = false;
    const t = setInterval(() => {
      const L = live.current;
      const d = decode([0, ...L.us], CH, LIMITS, { up: L.linkUp, lq: L.linkUp ? L.lq : 0 }, machine.current);
      const curr = L.linkUp ? Math.abs(d.command) * 38 + 0.4 : 0;
      capa.current += (curr * 0.05) / 3.6;
      const fm = d.mode + (d.inputFault ? ' INPUT' : '');
      if (L.sound && L.linkUp && last !== null && fm !== last) beepsFor(d.mode);
      if (L.sound && lastUp && !L.linkUp) beepsFor('LOST');
      last = L.linkUp ? fm : null;
      lastUp = L.linkUp;
      setCar({ ...d, fm, curr, capa: capa.current });
    }, 50);
    return () => clearInterval(t);
  }, []);
  useEffect(() => { if (!carOn) { machine.current = new ModeMachine(); capa.current = 0; } }, [carOn]);

  useEffect(() => {
    const t = setInterval(() => setBlink((b) => !b), 500);
    return () => clearInterval(t);
  }, []);

  // ---- the screen ---------------------------------------------------------------------------
  const names = useMemo(() => {
    const n = new Array(16).fill('');
    for (const m of config.mixes) if (m.name && !n[m.ch - 1]) n[m.ch - 1] = m.name;
    return n;
  }, [config]);
  useEffect(() => {
    // rideon.lua's top line reads SC, SA and SB itself, not the channels.
    const sa = switches.SA - 1;
    const sb = switches.SB - 1;
    const tele = linkUp && car ? {
      fm: car.fm, rxbt: batt, curr: car.curr, capa: car.capa, rqly: lq,
      rss: Math.round(-38 - (100 - lq) * 0.62), tpwr: lq > 90 ? 10 : lq > 70 ? 25 : lq > 50 ? 50 : 100,
    } : null;
    const controls = [
      ...Object.keys(SWITCHES).map((s) => ({ name: s, value: SWITCHES[s] === 2 ? (switches[s] ? 'down' : 'up') : ['up', 'mid', 'down'][switches[s]], use: (uses[s] || []).map((u) => `CH${u.ch}`).join(' ') || '-' })),
      ...['lh', 'lv', 'rh', 'rv'].map((a) => ({ name: a.toUpperCase(), value: `${Math.round(sticks[a] * 100)}`, use: (uses[a] || []).map((u) => `CH${u.ch}`).join(' ') || '-' })),
    ];
    draw(lcd, {
      power, warning: power && warningPending, warnList, page, chPage, mixScroll,
      radio: { kill: switches.SC > 0 ? 'RUN' : 'KILL', mode: sa < 0 ? 'Lock' : sa > 0 ? 'Parent' : 'Kid', cap: sb < 0 ? '30%' : sb > 0 ? '100%' : '60%' },
      tele, battWarn: LIMITS.batteryLowWarnV, lqWarn: LIMITS.lqDegradedBelow,
      channels: out.channels, names,
      mixes: config.mixes.map((m, i) => ({ ...m, active: out.active[i] })),
      controls,
    }, blink);
    setLcdVersion((v) => v + 1);
  }, [lcd, power, warningPending, page, chPage, mixScroll, switches, sticks, car, linkUp, batt, lq, out, names, config, uses, blink]); // eslint-disable-line react-hooks/exhaustive-deps

  // ---- controls -----------------------------------------------------------------------------
  const cycleSwitch = useCallback((sw, dir = 1) => {
    setSwitches((s) => ({ ...s, [sw]: (s[sw] + dir + SWITCHES[sw]) % SWITCHES[sw] }));
  }, []);
  const setSwitch = useCallback((sw, p) => setSwitches((s) => ({ ...s, [sw]: p })), []);

  const onKey = useCallback((k) => {
    setPressed(k);
    setTimeout(() => setPressed(null), 140);
    if (k === 'POWER') {
      setPower((p) => {
        if (!p) setWarningPending(true);
        return !p;
      });
      return;
    }
    if (!power) return;
    if (warningPending) { setWarningPending(false); say('Switch warning skipped: EdgeTX lets any key skip it, so never rely on it alone.'); return; }
    if (k === 'RIGHT') setPage((p) => (p + 1) % PAGES.length);
    else if (k === 'LEFT') setPage((p) => (p + PAGES.length - 1) % PAGES.length);
    else if (k === 'UP' || k === 'DOWN') {
      const d = k === 'DOWN' ? 1 : -1;
      if (PAGES[page] === 'Channels') setChPage((c) => (c + 1) % 2);
      else if (PAGES[page] === 'Mixes') setMixScroll((s) => Math.max(0, Math.min(Math.max(0, config.mixes.length - 9), s + d)));
      else setPage((p) => (p + d + PAGES.length) % PAGES.length);
    }
  }, [power, warningPending, page, config.mixes.length, say]);

  const onTrim = useCallback((t) => {
    setPressed(t);
    setTimeout(() => setPressed(null), 140);
    say(config.trimsOff ? 'Trims are off in this model, so the trim buttons change nothing.' : `Trim ${t}`);
  }, [config.trimsOff, say]);

  // stick dragging: pointer moves anywhere on the page while held
  const drag = useRef(null);
  const onStickDown = useCallback((id, e) => {
    const ne = e.nativeEvent || e;
    drag.current = { id, x: ne.clientX, y: ne.clientY, from: { ...live.sticks } };
    setDragging(id);
  }, []);
  live.sticks = sticks;
  useEffect(() => {
    if (!dragging) return undefined;
    const move = (e) => {
      const d = drag.current;
      if (!d) return;
      const k = 1 / 70;
      const h = Math.max(-1, Math.min(1, (d.id === 'left' ? d.from.lh : d.from.rh) + (e.clientX - d.x) * k));
      const v = Math.max(-1, Math.min(1, (d.id === 'left' ? d.from.lv : d.from.rv) - (e.clientY - d.y) * k));
      setSticks((s) => (d.id === 'left' ? { ...s, lh: h, lv: v } : { ...s, rh: h, rv: v }));
    };
    const up = () => {
      const d = drag.current;
      drag.current = null;
      setDragging(null);
      // springs: the right stick both ways, the left stick sideways; left up/down stays put
      setSticks((s) => (d?.id === 'left' ? { ...s, lh: 0 } : { ...s, rh: 0, rv: 0 }));
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
    return () => { window.removeEventListener('pointermove', move); window.removeEventListener('pointerup', up); };
  }, [dragging]);

  // keyboard: arrows = right stick, A/D = left stick sideways, W/S = left stick up/down,
  // 1-4 = SA SB SC SD, K = the kill switch to KILL
  useEffect(() => {
    const held = new Set();
    const apply = () => setSticks((s) => ({
      ...s,
      rh: (held.has('ArrowRight') ? 1 : 0) - (held.has('ArrowLeft') ? 1 : 0),
      rv: (held.has('ArrowUp') ? 1 : 0) - (held.has('ArrowDown') ? 1 : 0),
      lh: (held.has('d') ? 1 : 0) - (held.has('a') ? 1 : 0),
    }));
    const typing = (e) => /INPUT|SELECT|TEXTAREA/.test(e.target.tagName);
    const down = (e) => {
      if (typing(e)) return;
      const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
      if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'a', 'd'].includes(k)) { held.add(k); apply(); e.preventDefault(); }
      else if (k === 'w' || k === 's') setSticks((s) => ({ ...s, lv: Math.max(-1, Math.min(1, s.lv + (k === 'w' ? 0.1 : -0.1))) }));
      else if (['1', '2', '3', '4'].includes(k)) cycleSwitch(['SA', 'SB', 'SC', 'SD'][Number(k) - 1], e.shiftKey ? -1 : 1);
      else if (k === 'k' && live.killer) setSwitch(live.killer.sw, live.killer.p);
    };
    const upKey = (e) => {
      const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
      if (held.delete(k)) apply();
    };
    window.addEventListener('keydown', down);
    window.addEventListener('keyup', upKey);
    return () => { window.removeEventListener('keydown', down); window.removeEventListener('keyup', upKey); };
  }, [cycleSwitch, setSwitch]);
  live.killer = rules.killers.find((k) => k.n === 2) || rules.killers[0];

  // ---- labels in the 3D view ----------------------------------------------------------------
  const labels = useMemo(() => {
    const L = {};
    for (const sw of Object.keys(SWITCHES)) {
      const u = uses[sw] || [];
      L[sw] = (
        <>
          <b>{sw}</b>
          {u.length ? u.map((x) => (
            <div key={x.ch}>CH{x.ch} {x.label}: {x.positions.map((p, i) => <span key={i} className={i === switches[sw] ? 'on' : ''}>{i ? ' / ' : ''}{p}</span>)}</div>
          )) : <div className="dim">spare</div>}
        </>
      );
    }
    const stick = (h, v) => (
      <>
        {[h, v].map((a) => (
          <div key={a}>{a.endsWith('h') ? '↔' : '↕'} {(uses[a] || []).map((x) => `CH${x.ch} ${x.label}`).join(', ') || <span className="dim">{a === 'lv' ? 'unused (does not spring back)' : 'unused'}</span>}</div>
        ))}
      </>
    );
    L.left = stick('lh', 'lv');
    L.right = stick('rh', 'rv');
    return L;
  }, [uses, switches]);

  const resetModel = () => { setBase(REPO.tree); setConfig(REPO.config); say('Back to the repo model (radio/edgetx/sdcard/MODELS/model42.yml).'); };
  const importModel = (text) => {
    const m = load(text);
    setBase(m.tree);
    setConfig(m.config);
    say(`Loaded model "${m.config.name}".`);
  };

  return (
    <div className="app">
      <header className="top">
        <div>
          <h1>Jumper T-Lite V2 · RideOn</h1>
          <p className="sub">Drag the sticks, click the switches and keys. The channels, the screen and what the car would do follow live.</p>
        </div>
        <div className="top-actions">
          <label className="chk"><input type="checkbox" checked={showLabels} onChange={(e) => setShowLabels(e.target.checked)} /> Labels</label>
          <label className="chk"><input type="checkbox" checked={sound} onChange={(e) => { setSound(e.target.checked); if (e.target.checked) enableSound(); }} /> Sound</label>
        </div>
      </header>
      <main className="layout">
        <section className="stage">
          <Canvas shadows dpr={[1, 2]} camera={{ position: [0, 0.9, 2.9], fov: 36 }} gl={{ preserveDrawingBuffer: true }} onCreated={() => setTimeout(() => setReady(true), 0)}>
            <color attach="background" args={['#e9edf1']} />
            <hemisphereLight args={['#ffffff', '#7d8794', 0.9]} />
            <directionalLight position={[2, 3, 4]} intensity={1.6} castShadow shadow-mapSize={[1024, 1024]} />
            <directionalLight position={[-3, 1, -2]} intensity={0.5} />
            <Environment resolution={128}>
              <Lightformer intensity={2} position={[0, 3, 2]} scale={[6, 2, 1]} />
              <Lightformer intensity={0.8} position={[-4, 1, 1]} scale={[2, 4, 1]} />
              <Lightformer intensity={0.6} position={[4, 0, 3]} scale={[2, 4, 1]} />
            </Environment>
            <group rotation={[-0.35, 0, 0]}>
              <Radio3D
                sticks={sticks} switches={switches} dragging={dragging}
                onStickDown={onStickDown} onSwitch={cycleSwitch} onKey={onKey} onTrim={onTrim}
                pressed={pressed} lcd={lcd} lcdVersion={lcdVersion} labels={labels} showLabels={showLabels && ready}
              />
            </group>
            <Fit />
            <ContactShadows position={[0, -0.75, 0]} opacity={0.35} scale={4} blur={2.4} far={1.5} />
            <OrbitControls enabled={!dragging} enablePan={false} minDistance={1.2} maxDistance={7} target={[0, 0.05, 0]} />
          </Canvas>
          <div className="hint">
            Keys: arrows = right stick · A/D = left stick sideways · W/S = left stick up/down · 1-4 = SA SB SC SD (Shift reverses) · K = kill.
            Screen: SYS / MDL change page, ▲ ▼ scroll, ⏻ power.
          </div>
          {toast && <div className="toast">{toast}</div>}
        </section>
        <aside className="side">
          <nav className="tabs">
            {[['live', 'Live'], ['map', `Mapping${rules.errors.length ? ` (${rules.errors.length})` : ''}`], ['export', 'Export']].map(([k, t]) => (
              <button key={k} className={tab === k ? 'tab on' : 'tab'} onClick={() => setTab(k)}>{t}</button>
            ))}
          </nav>
          {tab === 'live' && (
            <LivePanel
              {...{ us, out, config, car, sending, power, warningPending, carOn, setCarOn, lq, setLq, batt, setBatt, stickMode, setStickMode, sticks, setSticks, switches, setSwitch, onKey, uses }}
              channelName={(c) => channelLabel(c, CH, config)}
            />
          )}
          {tab === 'map' && <MappingPanel config={config} setConfig={setConfig} rules={rules} stickMode={stickMode} reset={resetModel} uses={uses} />}
          {tab === 'export' && <ExportPanel base={base} config={config} rules={rules} importModel={importModel} reset={resetModel} say={say} />}
        </aside>
      </main>
    </div>
  );
}


