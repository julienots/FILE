// Bruit 3D déterministe (CPU) pour générer les planètes de façon procédurale et reproductible.
export function makeNoise(seed = 1) {
  const p = new Uint8Array(512); const perm = [...Array(256).keys()];
  let s = seed * 9301 + 49297; const rnd = () => (s = (s * 9301 + 49297) % 233280) / 233280;
  for (let i = 255; i > 0; i--) { const j = Math.floor(rnd() * (i + 1)); [perm[i], perm[j]] = [perm[j], perm[i]]; }
  for (let i = 0; i < 512; i++) p[i] = perm[i & 255];
  const f = t => t * t * t * (t * (t * 6 - 15) + 10), l = (a, b, t) => a + t * (b - a);
  const g = (h, x, y, z) => { h &= 15; const u = h < 8 ? x : y, v = h < 4 ? y : (h === 12 || h === 14 ? x : z); return ((h & 1) ? -u : u) + ((h & 2) ? -v : v); };
  const noise = (x, y, z) => {
    const X = Math.floor(x) & 255, Y = Math.floor(y) & 255, Z = Math.floor(z) & 255; x -= Math.floor(x); y -= Math.floor(y); z -= Math.floor(z);
    const u = f(x), v = f(y), w = f(z), A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z, B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;
    return l(l(l(g(p[AA], x, y, z), g(p[BA], x - 1, y, z), u), l(g(p[AB], x, y - 1, z), g(p[BB], x - 1, y - 1, z), u), v),
             l(l(g(p[AA + 1], x, y, z - 1), g(p[BA + 1], x - 1, y, z - 1), u), l(g(p[AB + 1], x, y - 1, z - 1), g(p[BB + 1], x - 1, y - 1, z - 1), u), v), w);
  };
  noise.fbm = (x, y, z, o = 4) => { let a = 1, s = 0, n = 0; for (let i = 0; i < o; i++) { s += a * noise(x, y, z); n += a; x *= 2; y *= 2; z *= 2; a *= .5; } return s / n; };
  return noise;
}
