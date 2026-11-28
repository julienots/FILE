// LK Studio — ZIP files without a library: write (stored, CRC-32), read (stored or deflated).
const CRC = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
export function crc32(b) { let c = 0xffffffff; for (let i = 0; i < b.length; i++) c = CRC[(c ^ b[i]) & 0xff] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; }
const enc = new TextEncoder();

// files: [{name, data: Uint8Array | string}]
export function makeZip(files) {
  const parts = [], central = [];
  let offset = 0;
  for (const f of files) {
    const data = typeof f.data === 'string' ? enc.encode(f.data) : f.data;
    const name = enc.encode(f.name), crc = crc32(data);
    const h = new DataView(new ArrayBuffer(30));
    h.setUint32(0, 0x04034b50, true); h.setUint16(4, 20, true); h.setUint16(6, 0x0800, true); h.setUint16(8, 0, true);
    h.setUint16(10, 0, true); h.setUint16(12, 0x21, true); h.setUint32(14, crc, true); h.setUint32(18, data.length, true); h.setUint32(22, data.length, true);
    h.setUint16(26, name.length, true); h.setUint16(28, 0, true);
    parts.push(new Uint8Array(h.buffer), name, data);
    const c = new DataView(new ArrayBuffer(46));
    c.setUint32(0, 0x02014b50, true); c.setUint16(4, 20, true); c.setUint16(6, 20, true); c.setUint16(8, 0x0800, true); c.setUint16(10, 0, true);
    c.setUint16(12, 0, true); c.setUint16(14, 0x21, true); c.setUint32(16, crc, true); c.setUint32(20, data.length, true); c.setUint32(24, data.length, true);
    c.setUint16(28, name.length, true); c.setUint32(42, offset, true);
    central.push(new Uint8Array(c.buffer), name);
    offset += 30 + name.length + data.length;
  }
  const csize = central.reduce((s, p) => s + p.length, 0);
  const end = new DataView(new ArrayBuffer(22));
  end.setUint32(0, 0x06054b50, true); end.setUint16(8, files.length, true); end.setUint16(10, files.length, true); end.setUint32(12, csize, true); end.setUint32(16, offset, true);
  const all = [...parts, ...central, new Uint8Array(end.buffer)];
  const out = new Uint8Array(all.reduce((s, p) => s + p.length, 0));
  let o = 0; for (const p of all) { out.set(p, o); o += p.length; }
  return out;
}

async function inflateRaw(data) {
  if (typeof DecompressionStream === 'undefined') throw new Error('Ce téléphone ne sait pas décompresser ce zip.');
  const ds = new DecompressionStream('deflate-raw');
  const buf = await new Response(new Blob([data]).stream().pipeThrough(ds)).arrayBuffer();
  return new Uint8Array(buf);
}

// -> [{name, data: Uint8Array}]
export async function readZip(bytes) {
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let e = bytes.length - 22;
  while (e >= 0 && dv.getUint32(e, true) !== 0x06054b50) e--;
  if (e < 0) throw new Error('Ce fichier n\'est pas un zip.');
  const n = dv.getUint16(e + 10, true); let p = dv.getUint32(e + 16, true);
  const dec = new TextDecoder(), out = [];
  for (let i = 0; i < n; i++) {
    const method = dv.getUint16(p + 10, true), csize = dv.getUint32(p + 20, true), nlen = dv.getUint16(p + 28, true), xlen = dv.getUint16(p + 30, true), clen = dv.getUint16(p + 32, true), lo = dv.getUint32(p + 42, true);
    const name = dec.decode(bytes.subarray(p + 46, p + 46 + nlen));
    p += 46 + nlen + xlen + clen;
    if (name.endsWith('/')) continue;
    const lnlen = dv.getUint16(lo + 26, true), lxlen = dv.getUint16(lo + 28, true);
    const raw = bytes.subarray(lo + 30 + lnlen + lxlen, lo + 30 + lnlen + lxlen + csize);
    out.push({ name, data: method === 0 ? raw.slice() : await inflateRaw(raw) });
  }
  return out;
}

export function toBase64(u8) { let s = ''; const CH = 0x8000; for (let i = 0; i < u8.length; i += CH) s += String.fromCharCode.apply(null, u8.subarray(i, i + CH)); return btoa(s); }
