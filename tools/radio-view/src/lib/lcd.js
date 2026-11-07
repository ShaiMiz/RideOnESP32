// The T-Lite's 128 x 64 monochrome screen, drawn on a canvas that the 3D model uses as a texture.
// The RideOn page is a line-for-line port of run() in radio/edgetx/sdcard/SCRIPTS/TELEMETRY/rideon.lua;
// the other pages are simplified EdgeTX ones (channel monitor, mixes, switch warning).

export const W = 128;
export const H = 64;
export const SCALE = 4;
export const PAGES = ['RideOn', 'Channels', 'Mixes', 'Controls'];

const INK = '#1d2330';
const BACK = '#b9c9d6';
const FONTS = {
  SML: { px: 6, w: 4 },
  STD: { px: 8, w: 6 },
  MID: { px: 12, w: 8 },
  DBL: { px: 16, w: 11 },
};

export function makeCanvas() {
  const c = document.createElement('canvas');
  c.width = W * SCALE;
  c.height = H * SCALE;
  return c;
}

function painter(ctx, blinkOn) {
  ctx.setTransform(SCALE, 0, 0, SCALE, 0, 0);
  ctx.fillStyle = BACK;
  ctx.fillRect(0, 0, W, H);
  ctx.textBaseline = 'top';
  const text = (x, y, s, flags = {}) => {
    const f = FONTS[flags.size || 'STD'];
    if (flags.blink && !blinkOn) return;
    ctx.font = `bold ${f.px}px "DejaVu Sans Mono", Menlo, Consolas, monospace`;
    const w = Math.ceil(ctx.measureText(s).width) + 1;
    if (flags.invers) {
      ctx.fillStyle = INK;
      ctx.fillRect(x - 1, y - 1, w + 1, f.px + 1);
      ctx.fillStyle = BACK;
    } else ctx.fillStyle = INK;
    ctx.fillText(s, x, y);
    ctx.fillStyle = INK;
    return w;
  };
  const rect = (x, y, w, h, fill) => {
    ctx.fillStyle = INK;
    if (fill) ctx.fillRect(x, y, w, h);
    else ctx.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
  };
  ctx.lineWidth = 0.6;
  ctx.strokeStyle = INK;
  return { text, rect, ctx };
}

const fmt1 = (v) => v.toFixed(1);

/** The RideOn telemetry page: rideon.lua's run(). */
function rideonPage(p, s) {
  p.text(0, 0, 'RideOn', { invers: true });
  p.text(40, 0, `${s.radio.kill} ${s.radio.mode} ${s.radio.cap}`, { size: 'SML' });
  if (!s.tele) {
    p.text(16, 18, 'NO LINK', { size: 'DBL', blink: true });
    p.text(0, 48, 'Car off, or out of range.', { size: 'SML' });
    p.text(0, 56, 'Car brakes by itself.', { size: 'SML' });
    return;
  }
  const t = s.tele;
  const [carMode, fault] = t.fm.split(/\s+(.+)/);
  if (fault) {
    p.text(0, 10, carMode, { size: 'MID' });
    p.text(0, 24, fault, { size: 'MID', invers: true, blink: true });
  } else p.text(0, 12, carMode || '...', { size: 'DBL' });
  const low = t.rxbt > 5 && t.rxbt < s.battWarn;
  p.text(0, 40, `${fmt1(t.rxbt)}V`, { invers: low, blink: low });
  p.text(42, 40, `${fmt1(t.curr)}A`);
  p.text(84, 40, `${Math.round(t.capa)}mAh`, { size: 'SML' });
  const weak = t.rqly < s.lqWarn;
  p.text(0, 54, `LQ ${t.rqly}%`, { invers: weak, blink: weak });
  p.text(46, 54, `${t.rss}dBm`, { size: 'SML' });
  p.text(90, 54, `${t.tpwr}mW`, { size: 'SML' });
}

function channelsPage(p, s) {
  p.text(0, 0, 'CHANNEL MONITOR', { invers: true, size: 'SML' });
  const first = s.chPage * 8;
  p.text(100, 0, `${first + 1}-${first + 8}`, { size: 'SML' });
  for (let i = 0; i < 8; i++) {
    const c = first + i;
    const v = s.channels[c];
    const y = 8 + i * 7;
    p.text(0, y, `${String(c + 1).padStart(2, '0')}`, { size: 'SML' });
    p.text(10, y, (s.names[c] || '').slice(0, 6), { size: 'SML' });
    p.text(36, y, `${Math.round((v * 100) / 1024)}`.padStart(4), { size: 'SML' });
    const x0 = 56;
    const bw = 70;
    p.rect(x0, y + 1, bw, 5);
    const mid = x0 + bw / 2;
    const len = (v / 1024) * (bw / 2 - 1);
    p.ctx.fillRect(Math.min(mid, mid + len), y + 2, Math.abs(len), 3);
    p.ctx.fillRect(mid, y, 0.6, 7);
  }
}

function mixesPage(p, s) {
  p.text(0, 0, 'MIXES', { invers: true, size: 'SML' });
  const rows = s.mixes.slice(s.mixScroll, s.mixScroll + 9);
  rows.forEach((m, i) => {
    const y = 8 + i * 6.2;
    p.text(0, y, `CH${m.ch}`, { size: 'SML', invers: m.active && m.highlight });
    p.text(18, y, m.mltpx === 'REPL' ? ':=' : m.mltpx === 'MUL' ? '*=' : '+=', { size: 'SML' });
    p.text(28, y, `${m.src}`, { size: 'SML' });
    p.text(48, y, `${m.weight}%`.padStart(5), { size: 'SML' });
    p.text(72, y, m.swtch !== 'NONE' ? m.swtch : '', { size: 'SML' });
    p.text(92, y, m.name, { size: 'SML', invers: m.active });
  });
  if (!s.mixes.length) p.text(0, 24, 'No mixes', { size: 'MID' });
}

function controlsPage(p, s) {
  p.text(0, 0, 'CONTROLS', { invers: true, size: 'SML' });
  s.controls.slice(0, 9).forEach((c, i) => {
    const y = 8 + i * 6.2;
    p.text(0, y, c.name, { size: 'SML' });
    p.text(22, y, c.value, { size: 'SML' });
    p.text(50, y, c.use.slice(0, 19), { size: 'SML' });
  });
}

function warningPage(p, s) {
  p.text(0, 0, 'SWITCH WARNING', { invers: true });
  p.text(0, 14, 'Put the switches', { size: 'SML' });
  p.text(0, 21, 'where it asks:', { size: 'SML' });
  s.warnList.forEach((w, i) => p.text(4 + (i % 2) * 62, 32 + Math.floor(i / 2) * 9, w.text, { invers: !w.ok }));
  p.text(0, 56, 'Any key skips (EdgeTX)', { size: 'SML' });
}

/** Draw the current page. s is built by App from the radio and car state. */
export function draw(canvas, s, blinkOn) {
  const ctx = canvas.getContext('2d');
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  if (!s.power) {
    ctx.fillStyle = '#20262b';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    return;
  }
  const p = painter(ctx, blinkOn);
  if (s.warning) return warningPage(p, s);
  const page = PAGES[s.page];
  if (page === 'RideOn') rideonPage(p, s);
  else if (page === 'Channels') channelsPage(p, s);
  else if (page === 'Mixes') mixesPage(p, s);
  else controlsPage(p, s);
}
