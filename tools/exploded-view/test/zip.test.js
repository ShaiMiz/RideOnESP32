// The stored .zip that carries a .glb through the claude.ai viewer's save must open anywhere.
import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { writeFileSync, mkdtempSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { zipOne } from '../src/export/zip.js';

test('zipOne makes a valid zip holding the file unchanged', async () => {
  const body = new Uint8Array(5000).map((_, i) => (i * 7) % 256);
  const zip = await zipOne('model.glb', new Blob([body]));
  const dir = mkdtempSync(join(tmpdir(), 'zip-'));
  const f = join(dir, 'm.zip');
  writeFileSync(f, Buffer.from(await zip.arrayBuffer()));
  const out = execFileSync('python3', ['-c', 'import sys,zipfile;z=zipfile.ZipFile(sys.argv[1]);assert z.testzip() is None;d=z.read("model.glb");print(len(d),sum(d))', f]).toString().trim();
  assert.equal(out, `5000 ${body.reduce((a, b) => a + b, 0)}`);
});
