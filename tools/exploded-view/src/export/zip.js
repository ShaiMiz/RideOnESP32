// A minimal single-file .zip writer (stored, no compression), no dependencies.
const CRC = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; }
  return t;
})();

// One file, stored (no compression): enough to carry a .glb through the viewer's save.
export async function zipOne(name, blob) {
  const data = new Uint8Array(await blob.arrayBuffer());
  let crc = 0xffffffff;
  for (const b of data) crc = CRC[(crc ^ b) & 0xff] ^ (crc >>> 8);
  crc = (crc ^ 0xffffffff) >>> 0;
  const fn = new TextEncoder().encode(name);
  const local = new DataView(new ArrayBuffer(30));
  [[0, 0x04034b50, 4], [4, 20, 2], [8, 0, 2], [14, crc, 4], [18, data.length, 4], [22, data.length, 4], [26, fn.length, 2]]
    .forEach(([o, v, n]) => (n === 4 ? local.setUint32(o, v, true) : local.setUint16(o, v, true)));
  const central = new DataView(new ArrayBuffer(46));
  [[0, 0x02014b50, 4], [4, 20, 2], [6, 20, 2], [16, crc, 4], [20, data.length, 4], [24, data.length, 4], [28, fn.length, 2]]
    .forEach(([o, v, n]) => (n === 4 ? central.setUint32(o, v, true) : central.setUint16(o, v, true)));
  const end = new DataView(new ArrayBuffer(22));
  const cdOffset = 30 + fn.length + data.length;
  [[0, 0x06054b50, 4], [8, 1, 2], [10, 1, 2], [12, 46 + fn.length, 4], [16, cdOffset, 4]]
    .forEach(([o, v, n]) => (n === 4 ? end.setUint32(o, v, true) : end.setUint16(o, v, true)));
  return new Blob([local, fn, data, central, fn, end], { type: 'application/zip' });
}
