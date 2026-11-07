import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';
import { fileURLToPath } from 'node:url';

// The model, channel map and limits are read straight from the repo (radio/, lib/, include/)
// with `?raw` imports, so there is no generated copy to go stale.
const repo = fileURLToPath(new URL('../..', import.meta.url));

// `npm run build` makes a static site in dist/; `npm run build:single` inlines everything
// into one dist-single/index.html that opens from disk or can be shared as one file.
export default defineConfig(({ mode }) => ({
  base: './',
  plugins: [react(), ...(mode === 'single' ? [viteSingleFile()] : [])],
  server: { fs: { allow: [repo] } },
  build: { outDir: mode === 'single' ? 'dist-single' : 'dist', chunkSizeWarningLimit: 4000 },
}));
