// The alarm beeps of rideon.lua's background(), played in the browser when Sound is on.
let ctx = null;

export function enableSound() {
  try {
    ctx = ctx || new (window.AudioContext || window.webkitAudioContext)();
    if (ctx.state === 'suspended') ctx.resume();
  } catch { ctx = null; }
}

function tone(freq, ms, at) {
  if (!ctx) return;
  const o = ctx.createOscillator();
  const g = ctx.createGain();
  o.frequency.value = freq;
  o.type = 'square';
  g.gain.setValueAtTime(0.05, ctx.currentTime + at);
  g.gain.setValueAtTime(0, ctx.currentTime + at + ms / 1000);
  o.connect(g).connect(ctx.destination);
  o.start(ctx.currentTime + at);
  o.stop(ctx.currentTime + at + ms / 1000 + 0.02);
}

/** 1 beep Locked, 2 Kid, 3 Parent, a long low tone Killed, a low buzz for a lost link. */
export function beepsFor(mode) {
  const n = { LOCKED: [1, 600], KID: [2, 1000], PARENT: [3, 1400] }[mode];
  if (n) for (let i = 0; i < n[0]; i++) tone(n[1], 120, i * 0.2);
  else if (mode === 'KILLED') tone(400, 600, 0);
  else if (mode === 'LOST') tone(180, 500, 0);
}
