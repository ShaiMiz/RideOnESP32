// Save and export: .glb / .gltf (GLTFExporter), .png (canvas buffer), .pdf (jsPDF data sheet),
// and .json (the procedural parts data plus the current assembly state).
import { GLTFExporter } from 'three/examples/jsm/exporters/GLTFExporter.js';
import { jsPDF } from 'jspdf';
import autoTable from 'jspdf-autotable';
import { COLOUR_NAMES } from '../components/Wiring.jsx';
import { zipOne } from './zip.js';

// Inside a claude.ai Artifact the page cannot start a download itself; the viewer's
// `downloads` capability saves it after the viewer confirms. That accepts a fixed list of
// extensions, so .glb/.gltf go inside a .zip there. Anywhere else, a plain link download.
const SAVE_OK = /\.(png|jpe?g|json|pdf|zip|txt|csv|html|svg)$/i;

async function viewerSaver() {
  try {
    return window.claude?.use ? await window.claude.use('downloads') : null;
  } catch {
    return null;
  }
}

export async function download(blob, filename) {
  const saver = await viewerSaver();
  if (saver) {
    let name = filename;
    let body = blob;
    if (!SAVE_OK.test(name)) { body = await zipOne(name, blob); name = `${name}.zip`; }
    await saver.save({ filename: name, data: body });
    return;
  }
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 4000);
}

export function stamp() {
  const d = new Date();
  const p = (n) => String(n).padStart(2, '0');
  return `${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}`;
}

// glTF extras must be plain JSON: drop anything else from userData.
function cleanUserData(root) {
  root.traverse((o) => { o.userData = JSON.parse(JSON.stringify(o.userData || {})); });
}

export function exportGLTF(objects, { binary, filename, extras }) {
  return new Promise((resolve, reject) => {
    const list = objects.filter(Boolean);
    list.forEach(cleanUserData);
    if (list[0] && extras) list[0].userData.rideonesp32 = extras;
    new GLTFExporter().parse(
      list,
      (result) => {
        const blob = binary
          ? new Blob([result], { type: 'model/gltf-binary' })
          : new Blob([JSON.stringify(result, null, 1)], { type: 'model/gltf+json' });
        if (list[0]) delete list[0].userData.rideonesp32;
        download(blob, filename).then(() => resolve(blob.size), reject);
      },
      reject,
      { binary, onlyVisible: true, trs: false, maxTextureSize: 1024 },
    );
  });
}

// Renders one frame at `scale` x the screen resolution and returns it as a data URL.
export function snapshot(three, scale = 2, type = 'image/png', quality) {
  const { gl, scene, camera } = three;
  const old = gl.getPixelRatio();
  gl.setPixelRatio(old * scale);
  gl.render(scene, camera);
  const url = gl.domElement.toDataURL(type, quality);
  gl.setPixelRatio(old);
  gl.render(scene, camera);
  return { url, width: gl.domElement.width * scale, height: gl.domElement.height * scale };
}

export async function exportPNG(three, scale, filename) {
  const { url } = snapshot(three, scale);
  const blob = await (await fetch(url)).blob();
  await download(blob, filename);
  return blob.size;
}

// jsPDF's built-in fonts are Latin-1: spell out the few symbols the BOM uses.
const ASCII = { 'µ': 'u', '≤': '<=', '≥': '>=', '−': '-', '–': '-', '—': '-', 'Ω': 'ohm', '×': 'x', '→': '->', '°': ' deg', '’': "'", '“': '"', '”': '"', '…': '...', '·': '-' };
const pdfText = (s) => String(s ?? '').replace(/[µ≤≥−–—Ω×→°’“”…·]/g, (c) => ASCII[c]).replace(/[^\x20-\x7e\n]/g, '?');

export async function exportPDF({ three, model, data, state, revision, filename }) {
  const doc = new jsPDF({ orientation: 'landscape', unit: 'mm', format: 'a4' });
  const W = doc.internal.pageSize.getWidth();
  const Hh = doc.internal.pageSize.getHeight();
  const { url, width, height } = snapshot(three, 2, 'image/jpeg', 0.9);
  const visible = model.parts.filter((p) => !state.hidden.has(p.id));
  const ids = new Set(visible.map((p) => p.id));
  const wires = model.connections.filter((w) => ids.has(w.from.ref) && ids.has(w.to.ref));

  // page 1: title block, snapshot, assembly state
  doc.setFont('helvetica', 'bold');
  doc.setFontSize(16);
  doc.text('RideOnESP32 - exploded view data sheet', 12, 14);
  doc.setFont('helvetica', 'normal');
  doc.setFontSize(9);
  const meta = [
    `Car: ${pdfText(data.car.Model || 'Can-Am Maverick X3 24V Ride-On Buggy')}  |  ${pdfText(data.car['Weight / size'] || '')}`,
    `Drive: ${pdfText(model.variant.name)}  |  Style: ${state.visualStyle}  |  Explode factor: ${state.explode.toFixed(2)}`,
    `Revision: ${pdfText(revision ? `${revision.n}. ${revision.label} (${new Date(revision.time).toLocaleString()})` : '-')}`,
    `Visible: ${visible.length} of ${model.parts.length} parts, ${wires.length} of ${model.connections.length} connections  |  Data: ${pdfText(data.meta.sources.bom)}, ${pdfText(data.meta.sources.wires)} (${pdfText(data.meta.gitRef)})`,
  ];
  meta.forEach((m, i) => doc.text(m, 12, 21 + i * 4.5));
  const maxW = W - 24;
  const maxH = Hh - 50;
  const k = Math.min(maxW / width, maxH / height);
  doc.addImage(url, 'JPEG', 12 + (maxW - width * k) / 2, 41, width * k, height * k);
  doc.setDrawColor(150);
  doc.rect(12, 41, maxW, maxH);
  doc.setFontSize(7);
  doc.text('Positions are illustrative. Box size (#18) and the car-facing plugs J1-J8 (#17) are TBD.', 12, Hh - 5);

  // page 2+: visible parts
  doc.addPage();
  doc.setFontSize(12);
  doc.text('Visible parts', 12, 12);
  autoTable(doc, {
    startY: 16,
    head: [['Ref', 'Model / name', 'Type', 'Status', 'Where', 'BOM line', 'Usage / note', 'TBD']],
    body: visible.map((p) => [p.ref || p.id, p.name, p.type, p.status, p.where, p.bomLine || '', p.usage, p.tbd.join('; ')].map(pdfText)),
    styles: { fontSize: 6.5, cellPadding: 1, overflow: 'linebreak' },
    headStyles: { fillColor: [12, 35, 64] },
    columnStyles: { 0: { cellWidth: 14 }, 1: { cellWidth: 58 }, 2: { cellWidth: 30 }, 3: { cellWidth: 20 }, 4: { cellWidth: 16 }, 5: { cellWidth: 12 }, 7: { cellWidth: 38 } },
    margin: { left: 12, right: 12 },
  });

  // connection routing
  doc.addPage();
  doc.setFontSize(12);
  doc.text('Connection routing (Point A upstream -> Point B downstream)', 12, 12);
  autoTable(doc, {
    startY: 16,
    head: [['Wire', 'From (A)', 'To (B)', 'Net', 'AWG', 'Colour', 'Protected by', 'Route', 'Note']],
    body: wires.map((w) => [w.id, `${w.from.ref}.${w.from.pin}`, `${w.to.ref}.${w.to.pin}`, w.net, w.awg, COLOUR_NAMES[w.colour] || w.colour, w.protectedBy, w.route, w.note].map(pdfText)),
    styles: { fontSize: 6.5, cellPadding: 0.8 },
    headStyles: { fillColor: [12, 35, 64] },
    margin: { left: 12, right: 12 },
  });

  const pages = doc.getNumberOfPages();
  for (let i = 1; i <= pages; i++) {
    doc.setPage(i);
    doc.setFontSize(7);
    doc.text(`${i} / ${pages}`, W - 20, Hh - 5);
  }
  const blob = doc.output('blob');
  await download(blob, filename);
  return blob.size;
}

// The parts as plain data: every primitive, the explosion vectors, the wiring and the state.
export async function exportJSON({ model, directions, state, revisions, data, filename }) {
  const out = {
    format: 'rideonesp32-exploded-view/1',
    exported: new Date().toISOString(),
    sources: data.meta,
    variant: model.variant,
    state: { ...state, hidden: [...state.hidden] },
    assemblies: model.assemblies,
    parts: model.parts.map((p) => ({ ...p, explodeDirection: directions[p.id] })),
    connections: model.connections,
    revisions,
  };
  const blob = new Blob([JSON.stringify(out, null, 1)], { type: 'application/json' });
  await download(blob, filename);
  return blob.size;
}
