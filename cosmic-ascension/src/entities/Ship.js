import * as THREE from 'three';
import { toonMaterial, addOutline } from '../render/shaders/Toon.js';
import { Assets } from '../assets/AssetLibrary.js';
// Vaisseau du joueur. FABLE: améliorer visuels par niveau (State.data.ship.level), traînées, boost warp.
export class Ship {
  constructor() {
    this.root = new THREE.Group();
    const m = Assets.get('ship_basic');
    if (m) this.root.add(m); else this._procedural();
    this.flame = new THREE.Mesh(new THREE.ConeGeometry(.35, 2, 10), new THREE.MeshBasicMaterial({ color: '#7fe0ff', transparent: true, opacity: .9 }));
    this.flame.rotation.x = -Math.PI / 2; this.flame.position.z = 2.1; this.root.add(this.flame);
  }
  _procedural() {
    const hull = toonMaterial({ color: '#f2f5ff', rim: '#7fd0ff' }), accent = toonMaterial({ color: '#ff5a8a', rim: '#ffb0cc' });
    const body = new THREE.Mesh(new THREE.SphereGeometry(1, 20, 14), hull); body.scale.set(.9, .55, 2); addOutline(body, '#1a0f33', .04); this.root.add(body);
    const cock = new THREE.Mesh(new THREE.SphereGeometry(.5, 14, 10), new THREE.MeshToonMaterial({ color: '#59d7ff' })); cock.position.set(0, .4, -.5); this.root.add(cock);
    for (const s of [-1, 1]) { const w = new THREE.Mesh(new THREE.BoxGeometry(2, .12, 1.1), accent); w.position.set(1.3 * s, -.05, .6); w.rotation.z = .12 * s; this.root.add(w); }
    const fin = new THREE.Mesh(new THREE.BoxGeometry(.1, .8, .9), accent); fin.position.set(0, .6, 1.2); this.root.add(fin);
  }
  setThrust(k, t) { this.flame.scale.set(1, .6 + k * 1.2 + Math.sin(t * 40) * .1, 1); this.flame.material.opacity = .5 + k * .5; }
}
