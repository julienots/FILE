// LK Studio — 3D preview: the imported model on a stage, skinned on the game skeleton, with preview animations.
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { HUMAN_PARENT, QUAD_PARENT } from './lkm.js';

export class Viewer {
  constructor(el) {
    this.el = el;
    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false, preserveDrawingBuffer: true });
    this.renderer.setPixelRatio(Math.min(2, window.devicePixelRatio || 1));
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.shadowMap.enabled = true;
    el.appendChild(this.renderer.domElement);
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x1d1712);
    this.scene.fog = new THREE.Fog(0x1d1712, 12, 30);
    this.camera = new THREE.PerspectiveCamera(40, 1, 0.05, 100);
    this.camera.position.set(2.6, 1.8, 3.4);
    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.target.set(0, 0.9, 0); this.controls.enableDamping = true; this.controls.minDistance = 0.5; this.controls.maxDistance = 25;
    const hemi = new THREE.HemisphereLight(0xcfd8ff, 0x3a2a1c, 0.9); this.scene.add(hemi);
    const sun = new THREE.DirectionalLight(0xffeed8, 2.2); sun.position.set(3, 6, 4); sun.castShadow = true; sun.shadow.mapSize.set(1024, 1024);
    sun.shadow.camera.left = sun.shadow.camera.bottom = -4; sun.shadow.camera.right = sun.shadow.camera.top = 4; this.scene.add(sun);
    const rim = new THREE.DirectionalLight(0xffb060, 0.8); rim.position.set(-4, 3, -3); this.scene.add(rim);
    // the stage: a round stone dais with a golden ring, like the game's totems
    const dais = new THREE.Mesh(new THREE.CylinderGeometry(1.8, 2, 0.12, 48), new THREE.MeshStandardMaterial({ color: 0x3a3029, roughness: 0.9 }));
    dais.position.y = -0.06; dais.receiveShadow = true; this.scene.add(dais);
    const ring = new THREE.Mesh(new THREE.TorusGeometry(1.75, 0.025, 8, 96), new THREE.MeshStandardMaterial({ color: 0xd8b058, metalness: 0.9, roughness: 0.3, emissive: 0x3a2808 }));
    ring.rotation.x = Math.PI / 2; this.scene.add(ring);
    const grid = new THREE.GridHelper(20, 40, 0x3a3029, 0x2a221c); grid.position.y = -0.12; this.scene.add(grid);
    this.model = null; this.bones = null; this.kind = null; this.anim = 'idle'; this.t = 0; this.skelHelper = null;
    this.clock = new THREE.Clock();
    new ResizeObserver(() => this.resize()).observe(el);
    this.resize();
    const loop = () => { requestAnimationFrame(loop); this.frame(); };
    loop();
  }
  resize() { const w = this.el.clientWidth || 300, h = this.el.clientHeight || 300; this.renderer.setSize(w, h, false); this.renderer.domElement.style.width = '100%'; this.renderer.domElement.style.height = '100%'; this.camera.aspect = w / h; this.camera.updateProjectionMatrix(); }
  clear() { if (this.model) { this.scene.remove(this.model); this.model.traverse(o => { if (o.geometry) o.geometry.dispose(); }); } if (this.skelHelper) this.scene.remove(this.skelHelper); this.model = null; this.bones = null; this.skelHelper = null; }

  // a raw glTF scene (before rigging)
  showScene(scene) {
    this.clear();
    const box = new THREE.Box3().setFromObject(scene), size = box.getSize(new THREE.Vector3()), c = box.getCenter(new THREE.Vector3());
    const k = 1.8 / Math.max(1e-4, size.y);
    const g = new THREE.Group(); g.add(scene); scene.position.set(-c.x, -box.min.y, -c.z); g.scale.setScalar(k);
    scene.traverse(o => { if (o.isMesh) { o.castShadow = true; } });
    this.model = g; this.kind = 'raw'; this.scene.add(g); this.frameModel(1.8);
  }

  // the rigged result: a skinned mesh on the game skeleton
  showRig(r, canvas, showWeights = false) {
    this.clear();
    const nv = r.P.length / 3;
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(r.P, 3));
    geo.setAttribute('normal', new THREE.BufferAttribute(r.N, 3));
    geo.setAttribute('uv', new THREE.BufferAttribute(r.UV, 2));
    const si = new Uint16Array(nv * 4), sw = new Float32Array(nv * 4);
    for (let i = 0; i < nv; i++) { si[i * 4] = r.b0[i]; si[i * 4 + 1] = r.b1[i]; sw[i * 4] = r.w0[i]; sw[i * 4 + 1] = 1 - r.w0[i]; }
    geo.setAttribute('skinIndex', new THREE.BufferAttribute(si, 4));
    geo.setAttribute('skinWeight', new THREE.BufferAttribute(sw, 4));
    geo.setIndex(new THREE.BufferAttribute(r.I, 1));
    let mat;
    if (showWeights) {
      const col = new Float32Array(nv * 3), pal = [];
      for (let b = 0; b < 21; b++) { const c = new THREE.Color().setHSL((b * 0.61803) % 1, 0.7, 0.55); pal.push(c); }
      for (let i = 0; i < nv; i++) { const c = pal[r.b0[i] % 21]; col[i * 3] = c.r; col[i * 3 + 1] = c.g; col[i * 3 + 2] = c.b; }
      geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
      mat = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.8 });
    } else {
      const tex = new THREE.CanvasTexture(canvas); tex.flipY = false; tex.colorSpace = THREE.SRGBColorSpace; tex.anisotropy = 4;
      mat = new THREE.MeshStandardMaterial({ map: tex, roughness: 0.85, metalness: 0 });
    }
    const parent = r.kind === 'quadruped' ? QUAD_PARENT : HUMAN_PARENT;
    const bones = r.joints.map(() => new THREE.Bone());
    r.joints.forEach((j, i) => {
      const p = r.kind === 'prop' ? -1 : parent[i];
      if (p >= 0) { bones[i].position.set(j[0] - r.joints[p][0], j[1] - r.joints[p][1], j[2] - r.joints[p][2]); bones[p].add(bones[i]); }
      else bones[i].position.set(j[0], j[1], j[2]);
    });
    const mesh = new THREE.SkinnedMesh(geo, mat);
    mesh.castShadow = true;
    const roots = bones.filter((b, i) => (r.kind === 'prop' ? -1 : parent[i]) < 0);
    roots.forEach(b => mesh.add(b));
    mesh.bind(new THREE.Skeleton(bones));
    this.model = mesh; this.bones = bones; this.kind = r.kind; this.rest = bones.map(b => b.quaternion.clone()); this.restPos = bones.map(b => b.position.clone());
    this.scene.add(mesh);
    this.skelHelper = new THREE.SkeletonHelper(mesh); this.skelHelper.visible = !!this.showSkeleton; this.scene.add(this.skelHelper);
    this.frameModel(r.kind === 'quadruped' ? 1.0 : r.height);
  }
  setSkeletonVisible(v) { this.showSkeleton = v; if (this.skelHelper) this.skelHelper.visible = v; }
  frameModel(h) { this.controls.target.set(0, h * 0.5, 0); const d = Math.max(2.4, h * 2.1); this.camera.position.set(d * 0.65, h * 0.85, d); }
  snapshot() { return this.renderer.domElement.toDataURL('image/jpeg', 0.85); }

  frame() {
    const dt = Math.min(0.05, this.clock.getDelta()); this.t += dt;
    this.controls.update();
    if (this.model && this.kind === 'raw') this.model.rotation.y += dt * 0.3;
    if (this.bones) this.pose(this.t);
    this.renderer.render(this.scene, this.camera);
  }

  // ---- procedural preview clips (the game plays its own clips on the same bones)
  pose(t) {
    const B = this.bones, R = this.rest;
    B.forEach((b, i) => { b.quaternion.copy(R[i]); b.position.copy(this.restPos[i]); });
    const rot = (i, x, y = 0, z = 0) => { if (B[i]) B[i].quaternion.multiply(new THREE.Quaternion().setFromEuler(new THREE.Euler(x, y, z))); };
    const a = this.anim;
    if (this.kind === 'prop') { this.model.rotation.y = t * 0.4; return; }
    this.model.rotation.y = 0;
    if (this.kind === 'humanoid') {
      const sp = a === 'run' ? 9.5 : 6.2, amp = a === 'run' ? 0.85 : 0.5, s = Math.sin(t * sp), c = Math.cos(t * sp);
      if (a === 'walk' || a === 'run') {
        rot(13, -s * amp); rot(16, s * amp);                                         // thighs
        rot(14, Math.max(0, c) * amp * 1.3); rot(17, Math.max(0, -c) * amp * 1.3);   // knees
        rot(6, s * amp * 0.8, 0, 0); rot(10, -s * amp * 0.8, 0, 0);                  // arms swing
        rot(7, -0.3 - (a === 'run' ? 0.6 : 0)); rot(11, -0.3 - (a === 'run' ? 0.6 : 0));
        rot(1, a === 'run' ? 0.25 : 0.05, s * 0.12, 0);
        B[0].position.y = this.restPos[0].y + Math.abs(s) * (a === 'run' ? 0.06 : 0.03);
      } else if (a === 'attack') {
        const p = (t * 1.4) % 1, wind = Math.min(1, p / 0.35), strike = Math.max(0, Math.min(1, (p - 0.35) / 0.15)), back = Math.max(0, (p - 0.6) / 0.4);
        const k = wind * (1 - strike) + 0 * strike, e = strike * (1 - back);
        rot(10, -2.3 * k + 0.6 * e, 0, -0.4 * k); rot(11, -0.6 * k); rot(1, 0, 0.6 * k - 0.8 * e, 0); rot(2, 0.1 * e, 0.3 * k - 0.4 * e, 0);
        rot(6, -0.4, 0, 0.3); rot(13, -0.3 * e); rot(16, 0.3 * e); rot(14, 0.3 * e);
      } else if (a === 'jump') {
        const p = (t * 0.9) % 1, crouch = Math.sin(Math.min(1, p / 0.25) * Math.PI), air = p > 0.25 && p < 0.85 ? Math.sin((p - 0.25) / 0.6 * Math.PI) : 0;
        B[0].position.y = this.restPos[0].y - crouch * 0.12 + air * 0.6;
        rot(13, -0.8 * (crouch + air)); rot(16, -0.8 * (crouch + air)); rot(14, 1.2 * (crouch + air * 0.6)); rot(17, 1.2 * (crouch + air * 0.6));
        rot(6, -1.5 * air, 0, 0.3 * air); rot(10, -1.5 * air, 0, -0.3 * air);
      } else {   // idle: breathing, a look around
        rot(2, Math.sin(t * 1.6) * 0.03); rot(4, 0, Math.sin(t * 0.5) * 0.35, 0); rot(6, 0, 0, 0.05 + Math.sin(t * 1.6) * 0.02); rot(10, 0, 0, -0.05 - Math.sin(t * 1.6) * 0.02);
      }
      return;
    }
    // quadruped: diagonal pairs (walk), bound (run), bite (attack)
    const sp = a === 'run' ? 11 : 6.5, amp = a === 'run' ? 0.7 : 0.4, s = Math.sin(t * sp), c = Math.cos(t * sp);
    if (a === 'walk' || a === 'run') {
      const ph = a === 'run' ? [0, 0, Math.PI, Math.PI] : [0, Math.PI, Math.PI, 0];
      [[9, 10], [12, 13], [15, 16], [18, 19]].forEach(([u, l], k) => { const q = Math.sin(t * sp + ph[k]); rot(u, q * amp); rot(l, Math.max(0, Math.cos(t * sp + ph[k])) * amp * (k < 2 ? -1.2 : 1.2)); });
      rot(6, 0, s * 0.3, 0); rot(7, 0, s * 0.25, 0); rot(3, c * 0.06); rot(1, a === 'run' ? s * 0.12 : 0);
      B[0].position.y = this.restPos[0].y + Math.abs(s) * 0.03;
    } else if (a === 'attack') {
      const p = (t * 1.5) % 1, lunge = Math.sin(Math.min(1, p / 0.4) * Math.PI);
      rot(3, -0.4 * lunge); rot(4, 0.3 * lunge); rot(5, 0.5 * Math.max(0, Math.sin(p * Math.PI * 4)) * lunge);
      B[0].position.z = this.restPos[0].z + lunge * 0.12; rot(9, -0.5 * lunge); rot(12, -0.5 * lunge);
    } else if (a === 'jump') {
      const p = (t * 0.9) % 1, air = Math.sin(p * Math.PI); B[0].position.y = this.restPos[0].y + air * 0.5;
      rot(9, -0.8 * air); rot(12, -0.8 * air); rot(15, 0.8 * air); rot(18, 0.8 * air); rot(6, 0.5 * air);
    } else { rot(6, 0, Math.sin(t * 2) * 0.25, 0); rot(3, Math.sin(t * 1.2) * 0.05); rot(4, 0, Math.sin(t * 0.6) * 0.3, 0); rot(1, Math.sin(t * 1.6) * 0.02); }
  }
}
