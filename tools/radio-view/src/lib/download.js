// Inside a claude.ai Artifact the page can't start a download itself: the viewer's `downloads`
// capability saves the file after the viewer confirms, and it accepts a fixed list of
// extensions that has no .yml, so there the model goes inside a .zip. Anywhere else, a plain
// link download.
import { zipOne } from './zip.js';

const SAVE_OK = /\.(png|jpe?g|json|pdf|zip|txt|csv|html|svg|md)$/i;

async function viewerSaver() {
  try {
    return window.claude?.use ? await window.claude.use('downloads') : null;
  } catch {
    return null;
  }
}

/** Returns 'zip' when the file was wrapped in a zip, 'file' otherwise. */
export async function download(blob, filename) {
  const saver = await viewerSaver();
  if (saver) {
    if (SAVE_OK.test(filename)) {
      await saver.save({ filename, data: blob });
      return 'file';
    }
    await saver.save({ filename: `${filename}.zip`, data: await zipOne(filename, blob) });
    return 'zip';
  }
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 4000);
  return 'file';
}
