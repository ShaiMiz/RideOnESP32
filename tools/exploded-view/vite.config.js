import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';

// `npm run build` makes a normal static site in dist/; `npm run build:single` inlines everything
// into one dist-single/index.html that opens straight from disk or can be shared as one file.
export default defineConfig(({ mode }) => ({
  base: './',
  plugins: [react(), ...(mode === 'single' ? [viteSingleFile()] : [])],
  build: { outDir: mode === 'single' ? 'dist-single' : 'dist', chunkSizeWarningLimit: 4000 },
}));
