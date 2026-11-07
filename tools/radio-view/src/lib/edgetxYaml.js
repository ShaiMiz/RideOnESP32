// Reads and writes the YAML subset EdgeTX uses for model files (radio/edgetx/sdcard/MODELS/*.yml).
//
// EdgeTX writes two-space indents, maps, lists of maps ("- key: value") and scalars, with no
// comments, anchors or flow style. Its own reader is just as narrow, so this file stays in that
// subset on purpose: parse(text) then stringify() gives back the same bytes, and a scalar keeps
// its exact text (quoted or not) unless it is set again.
//
// Nodes: { t: 'map', entries: [[key, node], ...] }, { t: 'list', items: [mapNode, ...] },
//        { t: 'scalar', raw: '"Steer"' }

export function parse(text) {
  const lines = [];
  for (const [n, line] of text.split(/\r?\n/).entries()) {
    if (!line.trim()) continue;
    if (line.includes('\t')) throw new Error(`line ${n + 1}: tab; EdgeTX files use spaces`);
    if (line.trimStart().startsWith('#')) throw new Error(`line ${n + 1}: comment; EdgeTX's reader has none`);
    const indent = line.length - line.trimStart().length;
    lines.push({ n: n + 1, indent, text: line.slice(indent) });
  }
  const pos = { i: 0 };
  const root = parseBlock(lines, pos, 0);
  if (pos.i < lines.length) throw new Error(`line ${lines[pos.i].n}: unexpected indent`);
  return root;
}

function splitKey(text, n) {
  const m = /^([^:\s][^:]*?):(?:\s+(.*))?$/.exec(text);
  if (!m) throw new Error(`line ${n}: expected "key: value", got ${JSON.stringify(text)}`);
  return [m[1], m[2] ?? ''];
}

function parseBlock(lines, pos, indent) {
  const first = lines[pos.i];
  if (first && first.indent === indent && (first.text === '-' || first.text.startsWith('- '))) {
    const items = [];
    while (pos.i < lines.length && lines[pos.i].indent === indent && lines[pos.i].text.startsWith('- ')) {
      const l = lines[pos.i];
      // "- key: value" opens a map whose keys sit two columns further in.
      lines[pos.i] = { n: l.n, indent: indent + 2, text: l.text.slice(2) };
      items.push(parseMap(lines, pos, indent + 2));
    }
    return { t: 'list', items };
  }
  return parseMap(lines, pos, indent);
}

function parseMap(lines, pos, indent) {
  const entries = [];
  while (pos.i < lines.length && lines[pos.i].indent === indent && !lines[pos.i].text.startsWith('- ')) {
    const l = lines[pos.i];
    const [key, rest] = splitKey(l.text, l.n);
    pos.i++;
    if (rest !== '') {
      entries.push([key, { t: 'scalar', raw: rest }]);
      continue;
    }
    const next = lines[pos.i];
    if (next && (next.indent > indent || (next.indent === indent && next.text.startsWith('- ')))) {
      entries.push([key, parseBlock(lines, pos, next.indent)]);
    } else {
      entries.push([key, { t: 'scalar', raw: '' }]);
    }
  }
  return { t: 'map', entries };
}

export function stringify(node) {
  const out = [];
  writeNode(node, 0, out);
  return out.join('\n') + '\n';
}

function writeNode(node, indent, out) {
  const pad = ' '.repeat(indent);
  if (node.t === 'map') {
    for (const [k, v] of node.entries) {
      if (v.t === 'scalar') out.push(v.raw === '' ? `${pad}${k}:` : `${pad}${k}: ${v.raw}`);
      else {
        out.push(`${pad}${k}:`);
        writeNode(v, indent + 2, out);
      }
    }
  } else if (node.t === 'list') {
    for (const item of node.items) {
      const start = out.length;
      writeNode(item, indent + 2, out);
      if (out.length > start) out[start] = `${pad}- ${out[start].slice(indent + 2)}`;
    }
  }
}

// ---- helpers -------------------------------------------------------------------------------

export const scalar = (raw) => ({ t: 'scalar', raw: String(raw) });
export const quoted = (s) => scalar(`"${String(s).replace(/"/g, '')}"`);
export const map = (entries) => ({ t: 'map', entries });

/** The scalar's value: quotes removed, numbers as numbers. */
export function value(node) {
  if (!node || node.t !== 'scalar') return undefined;
  const raw = node.raw;
  if (/^".*"$/.test(raw)) return raw.slice(1, -1);
  if (/^-?\d+$/.test(raw)) return Number(raw);
  return raw;
}

export function get(node, ...path) {
  let cur = node;
  for (const key of path) {
    if (!cur) return undefined;
    if (cur.t === 'map') cur = cur.entries.find(([k]) => k === String(key))?.[1];
    else if (cur.t === 'list') cur = cur.items[key];
    else return undefined;
  }
  return cur;
}

/** Replace (or append) a key of a map node in place. */
export function setKey(mapNode, key, child) {
  const e = mapNode.entries.find(([k]) => k === key);
  if (e) e[1] = child;
  else mapNode.entries.push([key, child]);
}

export function removeKey(mapNode, key) {
  mapNode.entries = mapNode.entries.filter(([k]) => k !== key);
}

export function clone(node) {
  return JSON.parse(JSON.stringify(node));
}
