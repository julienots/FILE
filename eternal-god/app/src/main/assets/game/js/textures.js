// Chargement des textures photo (Poly Haven, CC0) en tableaux de textures WebGL2.
import * as THREE from 'three';

export const TERRAIN_LAYERS = ['rocky_terrain_02', 'forrest_ground_01', 'mossy_rock', 'snow_02', 'coast_sand_01', 'grass_path_2',
  'burned_ground_01', 'dark_rock', 'raked_dirt', 'cobblestone_floor_01', 'aerial_grass_rock'];
export const BUILDING_LAYERS = ['plastered_wall_02', 'weathered_brown_planks', 'clay_roof_tiles_02', 'roof_slates_02',
  'thatch_roof_angled', 'castle_wall_slates', 'marble_01'];

function loadImage(url) {
  return new Promise((res, rej) => {
    const img = new Image();
    img.onload = () => res(img);
    img.onerror = () => rej(new Error('Image introuvable : ' + url));
    img.src = url;
  });
}

// Construit un DataArrayTexture à partir d'une liste d'images
async function arrayTexture(urls, size, srgb, onEach) {
  const c = document.createElement('canvas');
  c.width = c.height = size;
  const ctx = c.getContext('2d', { willReadFrequently: true });
  const data = new Uint8Array(size * size * 4 * urls.length);
  for (let i = 0; i < urls.length; i++) {
    const img = await loadImage(urls[i]);
    ctx.drawImage(img, 0, 0, size, size);
    data.set(ctx.getImageData(0, 0, size, size).data, i * size * size * 4);
    if (onEach) onEach();
  }
  const t = new THREE.DataArrayTexture(data, size, size, urls.length);
  t.format = THREE.RGBAFormat;
  t.type = THREE.UnsignedByteType;
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.minFilter = THREE.LinearMipmapLinearFilter;
  t.magFilter = THREE.LinearFilter;
  t.generateMipmaps = true;
  t.anisotropy = 4;
  t.colorSpace = srgb ? THREE.SRGBColorSpace : THREE.NoColorSpace;
  t.needsUpdate = true;
  return t;
}

export async function loadTextureSet(names, base, size, onEach) {
  const diff = await arrayTexture(names.map((n) => `${base}/${n}_diffuse.jpg`), size, true, onEach);
  const nor = await arrayTexture(names.map((n) => `${base}/${n}_nor_gl.jpg`), Math.min(size, 512), false, onEach);
  return { diff, nor };
}
