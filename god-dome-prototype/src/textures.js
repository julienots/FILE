import * as THREE from 'three';
import { mulberry32, makeSimplex } from './noise.js';

// Textures 100 % procédurales (générées au lancement sur <canvas>) : aucun asset externe.
const sn = makeSimplex(99);

function canvas(w, h = w) {
  const c = document.createElement('canvas'); c.width = w; c.height = h;
  return [c, c.getContext('2d')];
}
function tex(c, { repeat = true, srgb = true } = {}) {
  const t = new THREE.CanvasTexture(c);
  if (srgb) t.colorSpace = THREE.SRGBColorSpace;
  if (repeat) t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.anisotropy = 4;
  t.generateMipmaps = true;
  t.minFilter = THREE.LinearMipmapLinearFilter;
  return t;
}
function noiseFill(ctx, w, h, base, amp, scale, rnd = Math.random) {
  const img = ctx.getImageData(0, 0, w, h), d = img.data;
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    // bruit périodique (tuilable) par mélange de 4 échantillons
    const fx = x / w, fy = y / h;
    const n = (a, b) => sn(a * scale, b * scale) + 0.5 * sn(a * scale * 2.1 + 9, b * scale * 2.1);
    const v = n(fx * w, fy * h) * (1 - fx) * (1 - fy) + n((fx - 1) * w, fy * h) * fx * (1 - fy) +
      n(fx * w, (fy - 1) * h) * (1 - fx) * fy + n((fx - 1) * w, (fy - 1) * h) * fx * fy;
    const g = (rnd() - 0.5) * 10;
    const i = (y * w + x) * 4;
    d[i] = base[0] + v * amp + g; d[i + 1] = base[1] + v * amp + g; d[i + 2] = base[2] + v * amp * 0.9 + g; d[i + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

export function makeTextures() {
  const rnd = mulberry32(7);
  const T = {};

  // Enduit crème des maisons, légèrement taché
  {
    const [c, g] = canvas(256); noiseFill(g, 256, 256, [246, 236, 212], 9, 0.03, rnd);
    for (let i = 0; i < 22; i++) { g.fillStyle = `rgba(170,150,120,${0.02 + rnd() * 0.04})`; g.beginPath(); g.ellipse(rnd() * 256, rnd() * 256, 10 + rnd() * 30, 6 + rnd() * 20, rnd() * 3, 0, 7); g.fill(); }
    g.fillStyle = 'rgba(140,120,95,0.12)'; g.fillRect(0, 240, 256, 16);
    T.plaster = tex(c);
  }
  // Bois (planches)
  {
    const [c, g] = canvas(256); noiseFill(g, 256, 256, [128, 86, 52], 26, 0.02, rnd);
    for (let x = 0; x < 256; x += 32) {
      g.fillStyle = 'rgba(40,22,10,0.55)'; g.fillRect(x, 0, 2, 256);
      for (let k = 0; k < 14; k++) { g.strokeStyle = `rgba(60,35,18,${0.15 + rnd() * 0.2})`; g.lineWidth = 1; const xx = x + 4 + rnd() * 24; g.beginPath(); g.moveTo(xx, 0); g.bezierCurveTo(xx + 3, 80, xx - 3, 170, xx + 2, 256); g.stroke(); }
      if (rnd() > 0.5) { g.fillStyle = 'rgba(50,28,14,0.5)'; g.beginPath(); g.ellipse(x + 16, rnd() * 256, 3, 5, 0, 0, 7); g.fill(); }
    }
    T.wood = tex(c);
  }
  // Tuiles de toit (rangées décalées, teintes terre cuite / brun)
  {
    const [c, g] = canvas(256); g.fillStyle = '#5a3426'; g.fillRect(0, 0, 256, 256);
    const rows = 10, cols = 8, rh = 256 / rows, cw = 256 / cols;
    for (let r = 0; r < rows; r++) for (let k = -1; k < cols + 1; k++) {
      const x = k * cw + (r % 2) * cw / 2, y = r * rh;
      const hue = 12 + rnd() * 14, l = 34 + rnd() * 14;
      const grd = g.createLinearGradient(0, y, 0, y + rh);
      grd.addColorStop(0, `hsl(${hue},48%,${l + 8}%)`); grd.addColorStop(1, `hsl(${hue},45%,${l - 10}%)`);
      g.fillStyle = grd; g.beginPath(); g.roundRect(x + 1, y + 1, cw - 2, rh + 3, [0, 0, 6, 6]); g.fill();
      g.fillStyle = 'rgba(255,240,220,0.08)'; g.fillRect(x + 2, y + 2, cw - 4, 2);
    }
    T.roof = tex(c);
  }
  // Ardoises bleu-gris (variante de toit)
  {
    const [c, g] = canvas(256); g.fillStyle = '#2c3440'; g.fillRect(0, 0, 256, 256);
    const rows = 12, cols = 8, rh = 256 / rows, cw = 256 / cols;
    for (let r = 0; r < rows; r++) for (let k = -1; k < cols + 1; k++) {
      const x = k * cw + (r % 2) * cw / 2, y = r * rh, l = 30 + rnd() * 12;
      g.fillStyle = `hsl(212,18%,${l}%)`; g.beginPath(); g.roundRect(x + 1, y + 1, cw - 2, rh + 2, [0, 0, 3, 3]); g.fill();
    }
    T.slate = tex(c);
  }
  // Pierre (moellons)
  {
    const [c, g] = canvas(256); g.fillStyle = '#5d574f'; g.fillRect(0, 0, 256, 256);
    let y = 0;
    while (y < 256) {
      const h = 20 + rnd() * 16; let x = -rnd() * 30;
      while (x < 256) {
        const w = 28 + rnd() * 34, l = 52 + rnd() * 18;
        g.fillStyle = `hsl(${30 + rnd() * 15},${8 + rnd() * 8}%,${l}%)`;
        g.beginPath(); g.roundRect(x + 2, y + 2, w - 4, h - 4, 7); g.fill();
        g.fillStyle = 'rgba(255,255,255,0.08)'; g.fillRect(x + 5, y + 4, w - 12, 3);
        x += w;
      }
      y += h;
    }
    T.stone = tex(c);
  }
  // Cartes de feuillage (bord "duveteux" des arbres)
  {
    const [c, g] = canvas(256); g.clearRect(0, 0, 256, 256);
    for (let i = 0; i < 70; i++) {
      const a = rnd() * Math.PI * 2, r = rnd() ** 0.6 * 100, x = 128 + Math.cos(a) * r, y = 128 + Math.sin(a) * r;
      const l = 32 + rnd() * 22;
      g.fillStyle = `hsl(${88 + rnd() * 18},${48 + rnd() * 15}%,${l}%)`;
      g.save(); g.translate(x, y); g.rotate(rnd() * 6.28); g.beginPath(); g.ellipse(0, 0, 9 + rnd() * 6, 4 + rnd() * 3, 0, 0, 7); g.fill(); g.restore();
    }
    T.leaves = tex(c, { repeat: false });
  }
  // Bouffée de nuage (douce)
  {
    const [c, g] = canvas(128);
    for (let i = 0; i < 18; i++) {
      const x = 64 + (rnd() - 0.5) * 50, y = 64 + (rnd() - 0.5) * 40, r = 18 + rnd() * 26;
      const grd = g.createRadialGradient(x, y, 0, x, y, r);
      grd.addColorStop(0, 'rgba(255,255,255,0.55)'); grd.addColorStop(1, 'rgba(255,255,255,0)');
      g.fillStyle = grd; g.fillRect(0, 0, 128, 128);
    }
    T.cloud = tex(c, { repeat: false, srgb: false });
  }
  // Halo lumineux
  {
    const [c, g] = canvas(128);
    const grd = g.createRadialGradient(64, 64, 0, 64, 64, 64);
    grd.addColorStop(0, 'rgba(255,255,255,1)'); grd.addColorStop(0.2, 'rgba(255,255,255,0.6)');
    grd.addColorStop(0.5, 'rgba(255,255,255,0.15)'); grd.addColorStop(1, 'rgba(255,255,255,0)');
    g.fillStyle = grd; g.fillRect(0, 0, 128, 128);
    T.glow = tex(c, { repeat: false, srgb: false });
  }
  // Frise runique lumineuse pour le piédestal
  {
    const [c, g] = canvas(1024, 64); g.fillStyle = '#000'; g.fillRect(0, 0, 1024, 64);
    g.strokeStyle = '#fff'; g.lineWidth = 3; g.lineCap = 'round';
    for (let x = 12; x < 1024; x += 40) {
      g.beginPath();
      const n = 2 + Math.floor(rnd() * 3);
      for (let k = 0; k < n; k++) {
        const x0 = x + rnd() * 22, y0 = 14 + rnd() * 36, x1 = x + rnd() * 22, y1 = 14 + rnd() * 36;
        g.moveTo(x0, y0); g.lineTo(x1, y1);
      }
      g.stroke();
      g.beginPath(); g.arc(x + 31, 32, 2, 0, 7); g.fillStyle = '#fff'; g.fill();
    }
    g.fillRect(0, 4, 1024, 2); g.fillRect(0, 58, 1024, 2);
    T.runes = tex(c, { srgb: false });
  }
  // Feuillage réaliste : feuilles pointues avec nervure, nuances variées (texture alpha)
  {
    const [c, g] = canvas(512); g.clearRect(0, 0, 512, 512);
    const leaf = (x, y, a, L, Wd, hue, light) => {
      g.save(); g.translate(x, y); g.rotate(a);
      const grd = g.createLinearGradient(0, -Wd, 0, Wd);
      grd.addColorStop(0, `hsl(${hue},${45 + rnd() * 20}%,${light + 8}%)`); grd.addColorStop(1, `hsl(${hue + 4},${40 + rnd() * 20}%,${light - 6}%)`);
      g.fillStyle = grd;
      g.beginPath(); g.moveTo(0, 0); g.quadraticCurveTo(L * 0.45, -Wd, L, 0); g.quadraticCurveTo(L * 0.45, Wd, 0, 0); g.fill();
      g.strokeStyle = `hsla(${hue},30%,${light + 18}%,0.55)`; g.lineWidth = 0.8; g.beginPath(); g.moveTo(0, 0); g.lineTo(L * 0.95, 0); g.stroke();
      g.strokeStyle = 'rgba(30,50,10,0.5)'; g.beginPath(); g.moveTo(-L * 0.15, 0); g.lineTo(0, 0); g.stroke();
      g.restore();
    };
    // brindilles puis feuilles disposées en grappe
    for (let i = 0; i < 9; i++) {
      const a0 = rnd() * 6.28, x0 = 256 + Math.cos(a0) * 40, y0 = 256 + Math.sin(a0) * 40;
      g.strokeStyle = '#4a3a22'; g.lineWidth = 2; g.beginPath(); g.moveTo(256, 256); g.lineTo(x0 + Math.cos(a0) * 150, y0 + Math.sin(a0) * 150); g.stroke();
    }
    for (let i = 0; i < 230; i++) {
      const a = rnd() * Math.PI * 2, rr = Math.pow(rnd(), 0.7) * 210, x = 256 + Math.cos(a) * rr, y = 256 + Math.sin(a) * rr;
      leaf(x, y, a + (rnd() - 0.5) * 1.4, 26 + rnd() * 22, 8 + rnd() * 6, 78 + rnd() * 30, 26 + rnd() * 22);
    }
    T.leaves = tex(c, { repeat: false });
  }
  // Branche de sapin : tige + aiguilles
  {
    const [c, g] = canvas(256, 512); g.clearRect(0, 0, 256, 512);
    const stem = (x0, y0, x1, y1, w, depth) => {
      g.strokeStyle = '#3b2e1c'; g.lineWidth = w; g.beginPath(); g.moveTo(x0, y0); g.lineTo(x1, y1); g.stroke();
      const L = Math.hypot(x1 - x0, y1 - y0), n = Math.floor(L / 2.2);
      for (let i = 0; i < n; i++) {
        const t = i / n, x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t, ang = Math.atan2(y1 - y0, x1 - x0);
        for (const sd of [-1, 1]) {
          const a = ang + sd * (0.9 + rnd() * 0.4), l = (16 + rnd() * 8) * (1 - t * 0.5) * (depth ? 0.7 : 1);
          g.strokeStyle = `hsl(${120 + rnd() * 25},${35 + rnd() * 20}%,${16 + rnd() * 16 + t * 8}%)`; g.lineWidth = 1.6;
          g.beginPath(); g.moveTo(x, y); g.lineTo(x + Math.cos(a) * l, y + Math.sin(a) * l); g.stroke();
        }
      }
    };
    stem(128, 500, 128, 14, 4, 0);
    for (let i = 0; i < 7; i++) { const y = 440 - i * 62, sd = i % 2 ? 1 : -1; stem(128, y, 128 + sd * (90 - i * 8), y - 80, 2, 1); }
    T.fir = tex(c, { repeat: false });
  }
  // Écorce
  {
    const [c, g] = canvas(256); noiseFill(g, 256, 256, [96, 76, 58], 30, 0.025, rnd);
    for (let i = 0; i < 70; i++) { const x = rnd() * 256; g.strokeStyle = `rgba(30,20,12,${0.3 + rnd() * 0.4})`; g.lineWidth = 1 + rnd() * 3; g.beginPath(); g.moveTo(x, 0); let xx = x; for (let y = 0; y <= 256; y += 16) { xx += (rnd() - 0.5) * 6; g.lineTo(xx, y); } g.stroke(); }
    T.bark = tex(c);
  }
  return T;
}

/** Remet les UV d'une BoxGeometry à l'échelle monde (densité de texel constante). */
export function boxUV(geo, w, h, d, s = 0.25) {
  const uv = geo.attributes.uv;
  const dims = [[d, h], [d, h], [w, d], [w, d], [w, h], [w, h]];
  for (let f = 0; f < 6; f++) for (let k = 0; k < 4; k++) {
    const i = f * 4 + k;
    uv.setXY(i, uv.getX(i) * dims[f][0] * s, uv.getY(i) * dims[f][1] * s);
  }
  return geo;
}
