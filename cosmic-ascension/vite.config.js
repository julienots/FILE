import { defineConfig } from 'vite';
export default defineConfig({
  base: './',               // chemins relatifs: requis pour Capacitor / APK
  build: { target: 'es2020', chunkSizeWarningLimit: 1500, sourcemap: false },
  server: { port: 5173 },
});
