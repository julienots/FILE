import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

/*
 * MODÈLES 3D EXTERNES (Tripo, Blender…)
 * Déposez des fichiers .glb dans `models/` et décrivez-les dans `models/manifest.json` :
 * [
 *   { "file": "heros.glb",   "role": "player",   "height": 1.8 },
 *   { "file": "fermiere.glb","role": "villager", "height": 1.7, "name": "Brune" },
 *   { "file": "rocher.glb",  "role": "scatter",  "height": 1.2, "count": 30 },
 *   { "file": "statue.glb",  "role": "prop",     "height": 4, "x": -30, "z": 10, "rotY": 0 }
 * ]
 * Le modèle est mis à l'échelle (hauteur en mètres), posé au sol, ombré et intégré à la même scène
 * que les deux modes. Sans manifeste, le jeu utilise ses modèles procéduraux.
 */
export async function loadExternalModels(ctx) {
  const { scene, terrain, world, player, villagers, onProgress = () => {} } = ctx;
  let manifest;
  try {
    const r = await fetch('models/manifest.json', { cache: 'no-cache' });
    if (!r.ok) return [];
    manifest = await r.json();
  } catch (e) { return []; }
  const loader = new GLTFLoader();
  const loaded = [];
  for (const [i, m] of manifest.entries()) {
    onProgress(`Modèle 3D ${i + 1}/${manifest.length} : ${m.file}`);
    let gltf;
    try { gltf = await loader.loadAsync('models/' + m.file); } catch (e) { console.warn('Modèle introuvable', m.file, e); continue; }
    const model = normalize(gltf.scene, m.height || 1.8);
    const place = (obj, x, z, rotY = 0) => { obj.position.set(x, world.groundAt(x, z), z); obj.rotation.y = rotY; scene.add(obj); };
    if (m.role === 'player') {
      swapVisual(player.root, model);
    } else if (m.role === 'villager') {
      const targets = m.name ? villagers.list.filter(v => v.name === m.name) : villagers.list;
      targets.forEach((v, k) => swapVisual(v.root, k === 0 ? model : model.clone(true)));
    } else if (m.role === 'prop') {
      place(model, m.x || 0, m.z || 0, m.rotY || 0);
    } else if (m.role === 'scatter') {
      for (let k = 0; k < (m.count || 10); k++) {
        for (let t = 0; t < 40; t++) {
          const a = Math.random() * 6.28, r = Math.sqrt(Math.random()) * 105, x = Math.cos(a) * r, z = Math.sin(a) * r;
          if (!terrain.vegetationOK(x, z, 0.5)) continue;
          const c = model.clone(true); c.scale.multiplyScalar(0.75 + Math.random() * 0.5);
          place(c, x, z, Math.random() * 6.28); break;
        }
      }
    }
    loaded.push(m.file);
  }
  return loaded;
}

// échelle à la hauteur voulue, pieds au sol, centré, ombres activées
function normalize(root, height) {
  const box = new THREE.Box3().setFromObject(root);
  const size = box.getSize(new THREE.Vector3());
  const s = height / Math.max(size.y, 1e-3);
  root.scale.multiplyScalar(s);
  const box2 = new THREE.Box3().setFromObject(root);
  const c = box2.getCenter(new THREE.Vector3());
  root.position.x -= c.x; root.position.z -= c.z; root.position.y -= box2.min.y;
  root.traverse(o => {
    if (o.isMesh) {
      o.castShadow = true; o.receiveShadow = true;
      const mats = Array.isArray(o.material) ? o.material : [o.material];
      mats.forEach(mt => { if (mt.map) mt.map.anisotropy = 8; mt.envMapIntensity = 0.6; });
    }
  });
  const wrap = new THREE.Group(); wrap.add(root);
  return wrap;
}

// remplace l'apparence procédurale d'un personnage par le modèle (le personnage garde son IA et ses collisions)
function swapVisual(root, model) {
  root.children.forEach(c => { c.visible = false; });
  root.userData.setLOD = null;
  root.add(model);
  root.userData.external = model;
  // animation simple pour un modèle non riggé : léger balancement à la marche
  const base = model.position.y;
  root.userData.extAnimate = (speed, t) => {
    const k = Math.min(1, speed / 3);
    model.position.y = base + Math.abs(Math.sin(t * 9)) * 0.06 * k;
    model.rotation.z = Math.sin(t * 9) * 0.05 * k;
    model.rotation.x = k * 0.08;
  };
}
