import * as THREE from 'three';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import { toonMaterial } from '../render/shaders/Toon.js';
import { MeshoptDecoder } from 'three/examples/jsm/libs/meshopt_decoder.module.js';
// Charge les modèles Tripo (GLB) listés dans public/models/manifest.json.
// Si un modèle manque → `get()` renvoie null et l'appelant utilise son fallback procédural. Le jeu reste jouable sans aucun GLB.
const loader = new GLTFLoader(); loader.setMeshoptDecoder(MeshoptDecoder);
const cache = new Map();
let manifest = {};
export const Assets = {
  async init() {
    try { manifest = await (await fetch('./models/manifest.json')).json(); } catch { manifest = {}; }
    await Promise.all(Object.entries(manifest).filter(([, e]) => e.file && !e.disabled).map(async ([key, e]) => {
      try { const gltf = await loader.loadAsync('./models/' + e.file);
        // Harmonise avec le rendu du jeu: matériau toon (texture ou couleurs de sommets conservées), pas de PBR.
        gltf.scene.traverse(o => { if (!o.isMesh) return; const old = o.material, vc = !!o.geometry.attributes.color;
          o.material = toonMaterial({ color: vc || old.map ? '#ffffff' : old.color, map: old.map ?? null, normalMap: old.normalMap ?? null, roughness: vc ? .75 : (old.roughness ?? .8), metalness: old.metalnessMap ? .5 : (old.metalness ?? 0), vertexColors: vc, rim: e.rim ?? '#9fd8ff', rimStrength: e.rimStrength ?? .7 }); old.dispose?.(); });
        cache.set(key, { gltf, meta: e }); }
      catch (err) { console.warn('[Assets] modèle introuvable:', key, e.file); }
    }));
  },
  has: (key) => cache.has(key),
  // Renvoie un clone prêt à l'emploi, normalisé: hauteur = meta.height (m), pieds à y=0, centré en XZ.
  get(key) {
    const e = cache.get(key); if (!e) return null;
    const o = e.gltf.scene.clone(true);
    const box = new THREE.Box3().setFromObject(o), size = box.getSize(new THREE.Vector3()), ctr = box.getCenter(new THREE.Vector3());
    const s = (e.meta.height ?? 1) / (size.y || 1); o.scale.setScalar(s);
    o.position.set(-ctr.x * s, -box.min.y * s, -ctr.z * s);
    const wrap = new THREE.Group(); wrap.add(o); if (e.meta.rotY) { const piv = new THREE.Group(); wrap.remove(o); piv.add(o); piv.rotation.y = e.meta.rotY; wrap.add(piv); } wrap.userData.animations = e.gltf.animations; return wrap;
  },
};
