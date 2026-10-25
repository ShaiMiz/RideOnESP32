// Materials for the two visual styles. Textures are drawn on canvases at start-up, so the app
// loads no image files.
import * as THREE from 'three';

function canvasTexture(size, draw, repeat = 1) {
  const c = document.createElement('canvas');
  c.width = c.height = size;
  draw(c.getContext('2d'), size);
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.repeat.set(repeat, repeat);
  t.anisotropy = 4;
  return t;
}

let tex = null;
function textures() {
  if (tex) return tex;
  // seeded noise so every load looks the same
  let seed = 7;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  tex = {
    // brushed metal: fine horizontal streaks, used as a roughness map
    brushed: canvasTexture(256, (g, s) => {
      g.fillStyle = '#808080';
      g.fillRect(0, 0, s, s);
      for (let i = 0; i < 1400; i++) {
        const v = 100 + rnd() * 80;
        g.strokeStyle = `rgba(${v},${v},${v},0.35)`;
        g.beginPath();
        const y = rnd() * s;
        g.moveTo(0, y);
        g.lineTo(s, y + (rnd() - 0.5) * 2);
        g.stroke();
      }
    }, 2),
    // molded plastic: soft speckle, used as a bump map
    molded: canvasTexture(256, (g, s) => {
      const img = g.createImageData(s, s);
      for (let i = 0; i < img.data.length; i += 4) {
        const v = 118 + rnd() * 20;
        img.data[i] = img.data[i + 1] = img.data[i + 2] = v;
        img.data[i + 3] = 255;
      }
      g.putImageData(img, 0, 0);
    }, 6),
    // hazard stripes for anything still TBD
    tbd: canvasTexture(64, (g, s) => {
      g.fillStyle = '#ff9f1c';
      g.fillRect(0, 0, s, s);
      g.strokeStyle = '#1b1b1b';
      g.lineWidth = s / 6;
      for (let i = -s; i < 2 * s; i += s / 2) { g.beginPath(); g.moveTo(i, 0); g.lineTo(i + s, s); g.stroke(); }
    }, 1),
    // perfboard holes
    proto: canvasTexture(128, (g, s) => {
      g.fillStyle = '#c9a15b';
      g.fillRect(0, 0, s, s);
      g.fillStyle = '#7a5a26';
      for (let x = 4; x < s; x += 8) for (let y = 4; y < s; y += 8) { g.beginPath(); g.arc(x, y, 1.6, 0, 7); g.fill(); }
    }, 3),
    // red hex mesh for the front and rear grilles
    grille: canvasTexture(128, (g, s) => {
      g.fillStyle = '#2a0606';
      g.fillRect(0, 0, s, s);
      g.strokeStyle = '#d0201c';
      g.lineWidth = 3;
      const r = s / 8;
      const h = r * Math.sqrt(3) / 2;
      for (let row = -1; row < s / h + 1; row++) for (let col = -1; col < s / (r * 1.5) + 1; col++) {
        const cx = col * r * 1.5;
        const cy = row * h * 2 + (col % 2 ? h : 0);
        g.beginPath();
        for (let k = 0; k < 6; k++) g.lineTo(cx + r * 0.85 * Math.cos(k * Math.PI / 3), cy + r * 0.85 * Math.sin(k * Math.PI / 3));
        g.closePath();
        g.stroke();
      }
    }, 2),
    // sand door panel with the black lower stripe and the "RS" decal
    door: (() => {
      const t = canvasTexture(256, (g, s) => {
        g.fillStyle = '#cdb68c';
        g.fillRect(0, 0, s, s);
        g.fillStyle = '#16171a';
        g.beginPath();
        g.moveTo(0, s * 0.68); g.lineTo(s, s * 0.6); g.lineTo(s, s); g.lineTo(0, s);
        g.fill();
        g.fillStyle = '#b81c1c';
        g.fillRect(0, s * 0.62, s, s * 0.03);
        g.fillStyle = '#16171a';
        g.font = 'italic bold 92px sans-serif';
        g.fillText('RS', s * 0.3, s * 0.48);
        g.font = 'bold 22px sans-serif';
        g.fillText('MAVERICK X3', s * 0.26, s * 0.57);
      }, 1);
      t.wrapS = t.wrapT = THREE.ClampToEdgeWrapping;
      t.colorSpace = THREE.SRGBColorSpace;
      return t;
    })(),
    label: canvasTexture(128, (g, s) => {
      g.fillStyle = '#f2f2ee';
      g.fillRect(0, 0, s, s);
      g.fillStyle = '#222';
      g.font = 'bold 22px sans-serif';
      g.fillText('24 V', 10, 40);
      g.font = '14px sans-serif';
      g.fillText('chemistry TBD', 10, 70);
      g.fillText('#13', 10, 95);
    }, 1),
  };
  return tex;
}

const std = (o) => new THREE.MeshStandardMaterial(o);
const phys = (o) => new THREE.MeshPhysicalMaterial(o);

let realistic = null;
export function realisticMaterials() {
  if (realistic) return realistic;
  const t = textures();
  realistic = {
    // molded matte plastics
    // body colours from the stock car: gloss black shell, sand RS panels, red details
    bodyPlastic: phys({ color: '#0e0f11', roughness: 0.22, metalness: 0, clearcoat: 1, clearcoatRoughness: 0.08 }),
    bodyAccent: std({ color: '#1a1b1e', roughness: 0.85, bumpMap: t.molded, bumpScale: 0.6 }),
    sand: phys({ color: '#cdb68c', roughness: 0.4, clearcoat: 0.6, clearcoatRoughness: 0.2 }),
    doorSand: phys({ map: t.door, roughness: 0.4, clearcoat: 0.6, clearcoatRoughness: 0.2 }),
    grilleRed: std({ map: t.grille, roughness: 0.55 }),
    cageBlack: std({ color: '#141518', roughness: 0.45, metalness: 0.3 }),
    shockRed: std({ color: '#d11d1d', roughness: 0.3, metalness: 0.4 }),
    seatRed: std({ color: '#b01818', roughness: 0.75, bumpMap: t.molded, bumpScale: 0.8 }),
    rimRed: std({ color: '#cc1f1f', roughness: 0.35, metalness: 0.3 }),
    rimBlack: std({ color: '#1b1c1f', roughness: 0.4, metalness: 0.5 }),
    blackPlastic: std({ color: '#1d1e20', roughness: 0.65 }),
    greyPlastic: std({ color: '#8b8f94', roughness: 0.6 }),
    boxAbs: std({ color: '#3c4148', roughness: 0.7, bumpMap: t.molded, bumpScale: 0.4 }),
    seatFabric: std({ color: '#151619', roughness: 0.95, bumpMap: t.molded, bumpScale: 1.2 }),
    rubber: std({ color: '#141414', roughness: 0.92 }),
    whiteNylon: std({ color: '#e8e6df', roughness: 0.6 }),
    white: std({ color: '#f1f1ee', roughness: 0.5 }),
    yellow: std({ color: '#f2c200', roughness: 0.5 }),
    red: std({ color: '#c81e1e', roughness: 0.45 }),
    orangeSeal: std({ color: '#e46d1a', roughness: 0.7 }),
    label: std({ map: t.label, roughness: 0.8 }),
    // metals
    chrome: std({ color: '#f4f4f4', metalness: 1, roughness: 0.08 }),
    brushed: std({ color: '#c9ccd1', metalness: 1, roughness: 0.38, roughnessMap: t.brushed }),
    steel: std({ color: '#d9dde2', metalness: 1, roughness: 0.18 }),
    copper: std({ color: '#c46f3d', metalness: 1, roughness: 0.25 }),
    brass: std({ color: '#d1a54a', metalness: 1, roughness: 0.28 }),
    gold: std({ color: '#e4b84b', metalness: 1, roughness: 0.3 }),
    aluminiumDark: std({ color: '#2a2c30', metalness: 0.8, roughness: 0.45, roughnessMap: t.brushed }),
    // boards
    pcbGreen: std({ color: '#1f6b3a', roughness: 0.45 }),
    pcbBlue: std({ color: '#1f4fa8', roughness: 0.45 }),
    pcbRed: std({ color: '#b3242a', roughness: 0.45 }),
    pcbBlack: std({ color: '#141518', roughness: 0.4 }),
    pcbPurple: std({ color: '#5b2c8f', roughness: 0.45 }),
    pcbProto: std({ map: t.proto, roughness: 0.6 }),
    blueTerminal: std({ color: '#2f6fd6', roughness: 0.5 }),
    greenTerminal: std({ color: '#2e9a4c', roughness: 0.5 }),
    // passives
    beige: std({ color: '#d9c49b', roughness: 0.6 }),
    bandBrown: std({ color: '#6b3b1c', roughness: 0.6 }),
    bandBlack: std({ color: '#111', roughness: 0.6 }),
    bandRed: std({ color: '#c0262b', roughness: 0.6 }),
    yellowCap: std({ color: '#e0a526', roughness: 0.5 }),
    blueCap: std({ color: '#203e8c', roughness: 0.4 }),
    fuseBlack: std({ color: '#222', roughness: 0.3, transparent: true, opacity: 0.9 }),
    fuseGrey: std({ color: '#9aa0a6', roughness: 0.3, transparent: true, opacity: 0.9 }),
    fuseViolet: std({ color: '#7b3fb8', roughness: 0.3, transparent: true, opacity: 0.9 }),
    fuseRed: std({ color: '#d42a2a', roughness: 0.3, transparent: true, opacity: 0.9 }),
    fuseBlue: std({ color: '#2a62d4', roughness: 0.3, transparent: true, opacity: 0.9 }),
    // glass and light
    glass: phys({ color: '#ffffff', roughness: 0.05, transmission: 0.9, transparent: true, opacity: 0.35, thickness: 0.002 }),
    lens: std({ color: '#fffbe6', emissive: '#fff3c4', emissiveIntensity: 0.6, roughness: 0.1 }),
    screen: std({ color: '#05070a', emissive: '#4dc3ff', emissiveIntensity: 0.25, roughness: 0.15 }),
    blueLed: std({ color: '#3d7bff', emissive: '#3d7bff', emissiveIntensity: 1.2 }),
    speakerCone: std({ color: '#2b2b2b', roughness: 0.9, side: THREE.DoubleSide }),
    tbd: std({ map: t.tbd, roughness: 0.6, transparent: true, opacity: 0.92 }),
  };
  return realistic;
}

export const BLUEPRINT = {
  background: '#0c2340',
  line: '#cfe6ff',
  tbd: '#ffb454',
  hover: '#ffffff',
  selected: '#ffd166',
};

let bp = null;
export function blueprintMaterials() {
  if (bp) return bp;
  const line = (c, o = 0.9) => new THREE.LineBasicMaterial({ color: c, transparent: true, opacity: o });
  const fill = new THREE.MeshBasicMaterial({ color: BLUEPRINT.line, transparent: true, opacity: 0.035, depthWrite: false, side: THREE.DoubleSide });
  bp = { line: line(BLUEPRINT.line, 0.75), tbd: line(BLUEPRINT.tbd, 0.95), hover: line(BLUEPRINT.hover, 1), selected: line(BLUEPRINT.selected, 1), fill };
  return bp;
}

// Hover/selection tint for the realistic style: a cloned material per key, cached.
const tinted = new Map();
export function highlighted(key, mode) {
  const k = `${key}:${mode}`;
  if (!tinted.has(k)) {
    const m = realisticMaterials()[key]?.clone() || std({ color: '#888' });
    m.emissive = new THREE.Color(mode === 'selected' ? '#ffb000' : '#5aa9ff');
    m.emissiveIntensity = mode === 'selected' ? 0.8 : 0.35;
    tinted.set(k, m);
  }
  return tinted.get(k);
}
