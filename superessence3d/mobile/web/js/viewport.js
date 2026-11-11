// Viewport mobile : navigation tactile, sélection par toucher, gizmo, sculpture, lecture d'animation.
import * as THREE from "three";
import { OrbitControls } from "../vendor/three/addons/controls/OrbitControls.js";
import { TransformControls } from "../vendor/three/addons/controls/TransformControls.js";
import { RoomEnvironment } from "../vendor/three/addons/utils/RoomEnvironment.js";

export class Viewport {
  constructor(el, hooks) {
    this.el = el;
    this.hooks = hooks;
    this.tool = "select";
    this.brush = { kind: "draw", radius: 0.15, strength: 0.5, invert: false, symmetry: true, color: "#ff3030" };
    const r = this.renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true });
    r.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    r.outputColorSpace = THREE.SRGBColorSpace;
    r.toneMapping = THREE.ACESFilmicToneMapping;
    r.shadowMap.enabled = true;
    r.shadowMap.type = THREE.PCFSoftShadowMap;
    el.appendChild(r.domElement);
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x1b1c22);
    this.scene.environment = new THREE.PMREMGenerator(r).fromScene(new RoomEnvironment(), 0.04).texture;
    this.camera = new THREE.PerspectiveCamera(45, 1, 0.01, 2000);
    this.camera.position.set(4, 3, 5);
    this.controls = new OrbitControls(this.camera, r.domElement);
    this.controls.target.set(0, 0.8, 0);
    this.controls.enableDamping = true;
    this.controls.screenSpacePanning = true;
    this.lights = new THREE.Group();
    this.scene.add(this.lights);
    this.setLighting("studio");
    this.grid = new THREE.GridHelper(40, 40, 0x555a6a, 0x2c2f3a);
    this.scene.add(this.grid);
    this.content = new THREE.Group();
    this.content.name = "Scène";
    this.scene.add(this.content);
    this.selection = null;
    this.box = new THREE.BoxHelper(new THREE.Object3D(), 0xff9a3c);
    this.box.visible = false;
    this.scene.add(this.box);
    this.gizmo = new TransformControls(this.camera, r.domElement);
    this.gizmo.setSize(1.15);
    this.scene.add(this.gizmo.getHelper ? this.gizmo.getHelper() : this.gizmo);
    this.gizmo.addEventListener("dragging-changed", (e) => {
      this.controls.enabled = !e.value;
      if (e.value) this.hooks.beforeChange?.("Transformer");
      else this.hooks.changed?.();
    });
    this.gizmo.addEventListener("objectChange", () => this.box.update());
    this.cursor = new THREE.Mesh(new THREE.RingGeometry(0.95, 1, 40), new THREE.MeshBasicMaterial({ color: 0xffffff, side: THREE.DoubleSide, depthTest: false, transparent: true }));
    this.cursor.visible = false;
    this.cursor.renderOrder = 999;
    this.scene.add(this.cursor);
    this.mixers = new Map();
    this.playing = false;
    this.clock = new THREE.Clock();
    this.ray = new THREE.Raycaster();
    this._events();
    new ResizeObserver(() => this.resize()).observe(el);
    this.resize();
    const loop = () => {
      requestAnimationFrame(loop);
      const dt = this.clock.getDelta();
      if (this.playing) for (const m of this.mixers.values()) m.update(dt);
      this.controls.update();
      if (this.selection) this.box.update();
      r.render(this.scene, this.camera);
    };
    loop();
  }

  resize() {
    const w = this.el.clientWidth, h = this.el.clientHeight;
    if (!w || !h) return;
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
  }

  setLighting(preset) {
    this.lights.clear();
    const add = (l, p) => { if (p) l.position.set(...p); this.lights.add(l); return l; };
    const sun = (c, i, p) => { const d = add(new THREE.DirectionalLight(c, i), p); d.castShadow = true; d.shadow.mapSize.set(1024, 1024); d.shadow.camera.left = d.shadow.camera.bottom = -15; d.shadow.camera.right = d.shadow.camera.top = 15; return d; };
    this.lightPreset = preset;
    if (preset === "outdoor") { add(new THREE.HemisphereLight(0xbfd9ff, 0x6b5a40, 1.2)); sun(0xfff1d8, 3, [8, 12, 6]); this.scene.background = new THREE.Color(0x8fb4e0); this.scene.environmentIntensity = 1; }
    else if (preset === "night") { add(new THREE.HemisphereLight(0x3040a0, 0x101018, 0.5)); sun(0x9fb4ff, 0.8, [-6, 10, -4]); add(new THREE.PointLight(0xff9040, 30, 20), [2, 2, 3]); this.scene.background = new THREE.Color(0x07080f); this.scene.environmentIntensity = 0.25; }
    else if (preset === "dramatic") { add(new THREE.HemisphereLight(0x202030, 0x000000, 0.3)); sun(0xffd2a0, 4, [6, 6, 2]); add(new THREE.DirectionalLight(0x6080ff, 2), [-5, 3, -6]); this.scene.background = new THREE.Color(0x0a0a0d); this.scene.environmentIntensity = 0.3; }
    else { add(new THREE.HemisphereLight(0xdfe6ff, 0x302820, 1.1)); sun(0xffffff, 2.4, [5, 9, 6]); add(new THREE.DirectionalLight(0xa9b8ff, 0.8), [-6, 4, -7]); this.scene.background = new THREE.Color(0x1b1c22); this.scene.environmentIntensity = 0.8; }
    if (this.customBg) this.scene.background = new THREE.Color(this.customBg);
  }

  add(obj, select = true) {
    this.content.add(obj);
    if (select) this.select(obj);
  }

  select(obj) {
    this.selection = obj;
    this.box.visible = !!obj;
    if (obj) this.box.setFromObject(obj);
    this._attach();
    this.hooks.selected?.(obj);
  }

  selectPart(p) {
    // une pièce précise (maillage ou groupe) d'un modèle
    if (p && p.isMesh && p.parent && p.parent.userData.part && !p.userData.part) p = p.parent;
    this.selection = p;
    this.box.visible = !!p;
    if (p) this.box.setFromObject(p);
    this._attach();
    this.hooks.selected?.(p);
  }

  setTool(t) {
    this.tool = t;
    this.cursor.visible = false;
    const paint = t === "sculpt" || t === "paint";
    this.controls.enableRotate = !paint;
    this.controls.touches = paint ? { ONE: null, TWO: THREE.TOUCH.DOLLY_ROTATE } : { ONE: THREE.TOUCH.ROTATE, TWO: THREE.TOUCH.DOLLY_PAN };
    this._attach();
  }

  _attach() {
    const mode = { move: "translate", rotate: "rotate", scale: "scale" }[this.tool];
    if (mode && this.selection) { this.gizmo.setMode(mode); this.gizmo.attach(this.selection); } else this.gizmo.detach();
  }

  _ndc(ev) {
    const r = this.renderer.domElement.getBoundingClientRect();
    this.ray.setFromCamera(new THREE.Vector2(((ev.clientX - r.left) / r.width) * 2 - 1, -((ev.clientY - r.top) / r.height) * 2 + 1), this.camera);
  }

  topLevel(o) { while (o && o.parent && o.parent !== this.content) o = o.parent; return o && o.parent === this.content ? o : null; }

  pick(ev, within = null) {
    this._ndc(ev);
    this.content.updateMatrixWorld(true);
    const hits = this.ray.intersectObject(within || this.content, true).filter((h) => h.object.isMesh && h.object.visible);
    return hits[0] || null;
  }

  _events() {
    const el = this.renderer.domElement;
    let down = null, stroke = null;
    el.addEventListener("pointerdown", (ev) => {
      down = { x: ev.clientX, y: ev.clientY };
      if ((this.tool === "sculpt" || this.tool === "paint") && this.selection && ev.isPrimary) {
        const h = this.pick(ev, this.selection);
        if (h) {
          this.hooks.beforeChange?.(this.tool === "sculpt" ? "Sculpter" : "Peindre");
          stroke = { last: null };
          this._stroke(h, stroke);
          el.setPointerCapture(ev.pointerId);
        }
      }
    });
    el.addEventListener("pointermove", (ev) => {
      if (this.tool !== "sculpt" && this.tool !== "paint") return;
      const h = this.selection ? this.pick(ev, this.selection) : null;
      if (h) {
        const n = h.face.normal.clone().transformDirection(h.object.matrixWorld);
        this.cursor.visible = true;
        this.cursor.position.copy(h.point).addScaledVector(n, 0.003);
        this.cursor.lookAt(h.point.clone().add(n));
        this.cursor.scale.setScalar(this.brush.radius);
        if (stroke) this._stroke(h, stroke);
      } else this.cursor.visible = false;
    });
    el.addEventListener("pointerup", (ev) => {
      if (stroke) { stroke = null; this.hooks.changed?.(); return; }
      if (!down) return;
      const moved = Math.hypot(ev.clientX - down.x, ev.clientY - down.y);
      down = null;
      if (moved > 8 || this.gizmo.dragging || this.gizmo.axis) return;
      if (this.placeMode) { const hp = this.pick(ev); if (hp) this.placeMode(hp); return; }
      if (this.tool === "sculpt" || this.tool === "paint") return;
      const h = this.pick(ev);
      if (this.tool === "part") { this.selectPart(h ? h.object : null); return; }
      this.select(h ? this.topLevel(h.object) : null);
    });
  }

  _stroke(hit, stroke) {
    if (stroke.last && stroke.last.distanceTo(hit.point) < this.brush.radius * 0.25) return;
    stroke.last = hit.point.clone();
    const n = hit.face.normal.clone().transformDirection(hit.object.matrixWorld);
    const pts = [[hit.point, n]];
    if (this.brush.symmetry) {
      const inv = this.selection.matrixWorld.clone().invert();
      const lp = hit.point.clone().applyMatrix4(inv); lp.x = -lp.x;
      const ln = n.clone().transformDirection(inv); ln.x = -ln.x;
      pts.push([lp.applyMatrix4(this.selection.matrixWorld), ln.transformDirection(this.selection.matrixWorld)]);
    }
    this.selection.traverse((m) => { if (m.isMesh && m.geometry.attributes.position) for (const [p, nn] of pts) this.tool === "sculpt" ? sculptMesh(m, p, nn, this.brush) : paintMesh(m, p, this.brush); });
  }

  frame(obj = null) {
    const target = obj || (this.content.children.length ? this.content : null);
    const b = target ? new THREE.Box3().setFromObject(target) : new THREE.Box3(new THREE.Vector3(-1, 0, -1), new THREE.Vector3(1, 2, 1));
    if (b.isEmpty()) b.set(new THREE.Vector3(-1, 0, -1), new THREE.Vector3(1, 2, 1));
    const c = b.getCenter(new THREE.Vector3()), r = Math.max(b.getSize(new THREE.Vector3()).length() / 2, 0.1);
    const dir = this.camera.position.clone().sub(this.controls.target).normalize();
    const fov = THREE.MathUtils.degToRad(this.camera.fov) / 2;
    const d = r / Math.sin(Math.min(fov, Math.atan(Math.tan(fov) * this.camera.aspect))) * 1.05;
    this.controls.target.copy(c);
    this.camera.position.copy(c).addScaledVector(dir, d);
    this.camera.near = d / 200; this.camera.far = d * 50 + 50;
    this.camera.updateProjectionMatrix();
    // le panneau du bas recouvre une partie de la vue : on remonte l'objet dans la zone visible
    const sheet = document.getElementById("sheet");
    const cover = sheet && !sheet.classList.contains("closed") && window.innerWidth < 900 ? Math.min(0.6, sheet.offsetHeight / this.el.clientHeight) : 0;
    if (cover > 0) {
      this.camera.updateMatrixWorld();
      const up = new THREE.Vector3().setFromMatrixColumn(this.camera.matrixWorld, 1);
      const shift = up.multiplyScalar(-d * Math.tan(fov) * cover);
      this.camera.position.add(shift); this.controls.target.add(shift);
      this.camera.position.addScaledVector(dir, d * cover * 0.6);
    }
  }

  view(name) {
    const t = this.controls.target, d = this.camera.position.distanceTo(t);
    const v = { front: [0, 0, 1], right: [1, 0, 0], top: [0, 1, 0.0001], persp: [0.7, 0.45, 0.8] }[name];
    this.camera.position.copy(t).addScaledVector(new THREE.Vector3(...v).normalize(), d);
  }

  play(obj, clip) {
    let m = this.mixers.get(obj);
    if (!m) { m = new THREE.AnimationMixer(obj); this.mixers.set(obj, m); }
    m.stopAllAction();
    m.clipAction(clip).reset().play();
    this.playing = true;
  }

  stop() {
    this.playing = false;
    for (const [obj, m] of this.mixers) { m.stopAllAction(); obj.traverse((b) => { if (b.isBone && b.userData.rest) { b.position.copy(b.userData.rest.p); b.quaternion.copy(b.userData.rest.q); b.scale.copy(b.userData.rest.s); } }); }
  }

  snapshot(size = 256) {
    const prev = this.box.visible, g = this.gizmo.object;
    this.box.visible = false; this.gizmo.detach(); this.cursor.visible = false;
    this.renderer.render(this.scene, this.camera);
    const src = this.renderer.domElement;
    const cv = document.createElement("canvas"); cv.width = cv.height = size;
    const s = Math.min(src.width, src.height);
    cv.getContext("2d").drawImage(src, (src.width - s) / 2, (src.height - s) / 2, s, s, 0, 0, size, size);
    this.box.visible = prev; if (g) this._attach();
    return cv.toDataURL("image/jpeg", 0.85);
  }
}

// ------------------------------------------------------------------ sculpture / peinture
function brushVerts(mesh, point, radius, fn) {
  const g = mesh.geometry, p = g.attributes.position;
  const inv = mesh.matrixWorld.clone().invert();
  const lp = point.clone().applyMatrix4(inv);
  const sc = new THREE.Vector3().setFromMatrixScale(mesh.matrixWorld);
  const lr = radius / ((Math.abs(sc.x) + Math.abs(sc.y) + Math.abs(sc.z)) / 3);
  if (!g.boundingSphere) g.computeBoundingSphere();
  if (g.boundingSphere.center.distanceTo(lp) > g.boundingSphere.radius + lr) return false;
  let touched = false;
  for (let i = 0; i < p.count; i++) {
    const dx = p.getX(i) - lp.x, dy = p.getY(i) - lp.y, dz = p.getZ(i) - lp.z;
    const d = Math.sqrt(dx * dx + dy * dy + dz * dz);
    if (d >= lr) continue;
    const x = 1 - d / lr;
    fn(i, x * x * (3 - 2 * x), lp, lr, inv);
    touched = true;
  }
  return touched;
}

export function sculptMesh(mesh, point, normalW, b) {
  const g = mesh.geometry, p = g.attributes.position, nAttr = g.attributes.normal;
  const inv3 = new THREE.Matrix3().getNormalMatrix(mesh.matrixWorld).invert();
  const ln = normalW.clone().applyMatrix3(inv3).normalize();
  const sgn = b.invert ? -1 : 1;
  let avg = null;
  if (b.kind === "flatten" || b.kind === "smooth") {
    avg = new THREE.Vector3(); let n = 0;
    brushVerts(mesh, point, b.radius, (i) => { avg.x += p.getX(i); avg.y += p.getY(i); avg.z += p.getZ(i); n++; });
    if (!n) return;
    avg.divideScalar(n);
  }
  const touched = brushVerts(mesh, point, b.radius, (i, w, lp, lr) => {
    const s = w * b.strength * lr * 0.12 * sgn;
    let x = p.getX(i), y = p.getY(i), z = p.getZ(i);
    if (b.kind === "draw") { x += ln.x * s; y += ln.y * s; z += ln.z * s; }
    else if (b.kind === "inflate" && nAttr) { x += nAttr.getX(i) * s; y += nAttr.getY(i) * s; z += nAttr.getZ(i) * s; }
    else if (b.kind === "pinch") { x += (lp.x - x) * w * 0.15 * b.strength; y += (lp.y - y) * w * 0.15 * b.strength; z += (lp.z - z) * w * 0.15 * b.strength; }
    else if (b.kind === "crease") { x -= ln.x * s * 0.8; y -= ln.y * s * 0.8; z -= ln.z * s * 0.8; }
    else if (b.kind === "flatten") { const d = (x - avg.x) * ln.x + (y - avg.y) * ln.y + (z - avg.z) * ln.z; const k = d * w * Math.min(1, b.strength); x -= ln.x * k; y -= ln.y * k; z -= ln.z * k; }
    else if (b.kind === "smooth") { const k = w * Math.min(1, b.strength) * 0.5; x += (avg.x - x) * k; y += (avg.y - y) * k; z += (avg.z - z) * k; }
    p.setXYZ(i, x, y, z);
  });
  if (touched) { p.needsUpdate = true; g.computeVertexNormals(); g.computeBoundingSphere(); g.computeBoundingBox(); }
}

export function paintMesh(mesh, point, b) {
  const g = mesh.geometry;
  if (!g.attributes.color) {
    const c = new Float32Array(g.attributes.position.count * 3);
    const base = mesh.material.color || new THREE.Color(1, 1, 1);
    for (let i = 0; i < c.length; i += 3) { c[i] = base.r; c[i + 1] = base.g; c[i + 2] = base.b; }
    g.setAttribute("color", new THREE.BufferAttribute(c, 3));
    mesh.material.vertexColors = true;
    mesh.material.color.set(0xffffff);
    mesh.material.needsUpdate = true;
  }
  const col = new THREE.Color(b.color);
  const ca = g.attributes.color;
  const touched = brushVerts(mesh, point, b.radius, (i, w) => {
    const k = w * b.strength;
    ca.setXYZ(i, ca.getX(i) + (col.r - ca.getX(i)) * k, ca.getY(i) + (col.g - ca.getY(i)) * k, ca.getZ(i) + (col.b - ca.getZ(i)) * k);
  });
  if (touched) ca.needsUpdate = true;
}
