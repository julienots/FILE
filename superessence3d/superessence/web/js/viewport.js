// Viewport 3D : affiche exactement la géométrie évaluée par Blender (source de vérité).
// Les manipulations (gizmo, sélection, sculpture, peinture, édition) sont renvoyées au moteur.
import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { TransformControls } from "three/addons/controls/TransformControls.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { api, b64ToArray, fileUrl } from "./api.js";

THREE.Object3D.DEFAULT_UP.set(0, 0, 1);

const SEL_COLOR = new THREE.Color(0xff9a3c);
const ACT_COLOR = new THREE.Color(0xffc27a);

export class Viewport {
  constructor(container, hooks) {
    this.container = container;
    this.hooks = hooks; // {onSelect, onTransform, onStroke, onEditPick, onInfo}
    this.objects = new Map(); // nom -> {group, mesh, data, type, signature}
    this.shading = "solid";
    this.mode = "object";
    this.tool = "select";
    this.brush = { radius: 0.15, strength: 0.5, kind: "draw", color: "#ff3030", invert: false, symmetry: true, group: "" };
    this.editData = null;
    this.editSel = new Set();
    this.selectMode = "face";
    this.selected = new Set();
    this.active = null;

    const r = this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, preserveDrawingBuffer: false });
    r.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    r.outputColorSpace = THREE.SRGBColorSpace;
    r.toneMapping = THREE.ACESFilmicToneMapping;
    r.toneMappingExposure = 1.0;
    container.prepend(r.domElement);
    r.domElement.tabIndex = 0;

    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(45, 1, 0.01, 5000);
    this.camera.position.set(7, -8, 5);
    this.controls = new OrbitControls(this.camera, r.domElement);
    this.controls.target.set(0, 0, 0.8);
    this.controls.enableDamping = true;
    this.controls.dampingFactor = 0.12;
    this.controls.screenSpacePanning = true;

    // éclairage d'affichage (le rendu final est celui de Blender)
    this.scene.add(new THREE.HemisphereLight(0xdfe6ff, 0x302820, 1.6));
    const key = new THREE.DirectionalLight(0xffffff, 2.2);
    key.position.set(5, -6, 9);
    this.scene.add(key);
    const rim = new THREE.DirectionalLight(0xa9b8ff, 0.9);
    rim.position.set(-6, 7, 4);
    this.scene.add(rim);
    this.camLight = new THREE.DirectionalLight(0xffffff, 0.6);
    this.scene.add(this.camLight);

    // grille au sol (plan XY, Z vers le haut comme Blender)
    const grid = new THREE.GridHelper(40, 40, 0x555a6a, 0x30333e);
    grid.rotation.x = Math.PI / 2;
    grid.material.transparent = true;
    grid.material.opacity = 0.7;
    this.scene.add(grid);
    this.grid = grid;
    const axes = new THREE.Group();
    const lineMat = (c) => new THREE.LineBasicMaterial({ color: c, transparent: true, opacity: 0.85 });
    const ax = (a, b, c) => new THREE.Line(new THREE.BufferGeometry().setFromPoints([a, b]), lineMat(c));
    axes.add(ax(new THREE.Vector3(-20, 0, 0.001), new THREE.Vector3(20, 0, 0.001), 0xd04848));
    axes.add(ax(new THREE.Vector3(0, -20, 0.001), new THREE.Vector3(0, 20, 0.001), 0x6cbf3a));
    this.scene.add(axes);

    this.root = new THREE.Group();
    this.scene.add(this.root);
    this.overlay = new THREE.Group(); // édition, os, curseur
    this.scene.add(this.overlay);

    // gizmo de transformation
    this.proxy = new THREE.Object3D();
    this.scene.add(this.proxy);
    this.gizmo = new TransformControls(this.camera, r.domElement);
    this.gizmo.setSize(0.9);
    this.scene.add(this.gizmo.getHelper ? this.gizmo.getHelper() : this.gizmo);
    this.gizmo.addEventListener("dragging-changed", (e) => {
      this.controls.enabled = !e.value;
      if (e.value) this._dragStart = this.proxy.matrixWorld.clone();
      else this._finishDrag();
    });
    this.gizmo.addEventListener("objectChange", () => this._previewDrag());

    // curseur de pinceau
    const ring = new THREE.RingGeometry(0.97, 1, 48);
    this.brushCursor = new THREE.Mesh(ring, new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.85, side: THREE.DoubleSide, depthTest: false }));
    this.brushCursor.visible = false;
    this.brushCursor.renderOrder = 999;
    this.overlay.add(this.brushCursor);

    this.raycaster = new THREE.Raycaster();
    this.pointer = new THREE.Vector2();
    this._bindEvents();
    this.anim = null; // aperçu d'animation glTF
    this.clock = new THREE.Clock();
    new ResizeObserver(() => this.resize()).observe(container);
    this.resize();
    this._loop();
  }

  resize() {
    const w = this.container.clientWidth, hgt = this.container.clientHeight;
    if (!w || !hgt) return;
    this.renderer.setSize(w, hgt, false);
    this.camera.aspect = w / hgt;
    this.camera.updateProjectionMatrix();
  }

  _loop() {
    requestAnimationFrame(() => this._loop());
    this.controls.update();
    this.camLight.position.copy(this.camera.position);
    if (this.anim) this.anim.mixer.update(this.clock.getDelta());
    else this.clock.getDelta();
    this.renderer.render(this.scene, this.camera);
  }

  // ------------------------------------------------------------ synchronisation
  async sync(state) {
    const seen = new Set();
    const fetches = [];
    for (const o of state.objects) {
      seen.add(o.name);
      let entry = this.objects.get(o.name);
      if (!entry) {
        entry = { group: new THREE.Group(), type: o.type, signature: null, mesh: null };
        entry.group.matrixAutoUpdate = false;
        entry.group.userData.name = o.name;
        this.root.add(entry.group);
        this.objects.set(o.name, entry);
        if (o.type !== "MESH") this._buildHelper(entry, o);
      } else if (o.type !== "MESH") {
        this._buildHelper(entry, o);
      }
      entry.group.matrix.fromArray(o.matrix_world);
      entry.group.matrixWorldNeedsUpdate = true;
      entry.info = o;
      if (o.type === "MESH" && o.signature !== entry.signature) {
        fetches.push(this._fetchMesh(o.name, entry));
      }
    }
    for (const [name, entry] of this.objects) {
      if (!seen.has(name)) {
        this.root.remove(entry.group);
        this._dispose(entry.group);
        this.objects.delete(name);
      }
    }
    await Promise.all(fetches);
    this.selected = new Set(state.objects.filter((o) => o.selected).map((o) => o.name));
    this.active = state.active;
    this._updateSelectionLook();
    this._attachGizmo();
    this._info();
  }

  async _fetchMesh(name, entry) {
    try {
      const d = await api("get_mesh", { name, signature: entry.signature || "" }, { quiet: true });
      if (d.unchanged) return;
      this._buildMesh(entry, d);
      entry.signature = d.signature;
    } catch (e) { console.warn("maillage", name, e.message); }
  }

  _materialsFrom(d) {
    return d.materials.map((m) => {
      const mat = new THREE.MeshStandardMaterial({
        color: new THREE.Color().setRGB(...m.color, THREE.SRGBColorSpace),
        metalness: Math.min(1, m.metallic),
        roughness: Math.max(0.05, m.roughness),
        emissive: new THREE.Color().setRGB(...m.emission, THREE.SRGBColorSpace),
        emissiveIntensity: Math.min(m.emission_strength, 3),
        transparent: m.alpha < 0.999, opacity: m.alpha,
        vertexColors: !!d.colors,
        side: THREE.DoubleSide,
        flatShading: false,
      });
      mat.userData.preview = m;
      if (m.texture && d.uvs) {
        api("get_material_texture", { material: m.name }, { quiet: true }).then((r) => {
          new THREE.TextureLoader().load(fileUrl(r.path), (tex) => {
            tex.colorSpace = THREE.SRGBColorSpace;
            tex.flipY = false;
            mat.map = tex;
            mat.color.set(0xffffff);
            mat.needsUpdate = true;
          });
        }).catch(() => {});
      }
      return mat;
    });
  }

  _geometryFrom(d) {
    const pos = b64ToArray(d.positions, Float32Array);
    const nrm = b64ToArray(d.normals, Float32Array);
    const g = new THREE.BufferGeometry();
    let order = d.order ? b64ToArray(d.order, Uint32Array) : null;
    const reorder = (arr, size) => {
      if (!order) return arr;
      const out = new arr.constructor(arr.length);
      for (let t = 0; t < order.length; t++) {
        const src = order[t] * 3 * size, dst = t * 3 * size;
        for (let k = 0; k < 3 * size; k++) out[dst + k] = arr[src + k];
      }
      return out;
    };
    g.setAttribute("position", new THREE.BufferAttribute(reorder(pos, 3), 3));
    g.setAttribute("normal", new THREE.BufferAttribute(reorder(nrm, 3), 3));
    if (d.uvs) {
      const uv = reorder(b64ToArray(d.uvs, Float32Array), 2);
      for (let i = 1; i < uv.length; i += 2) uv[i] = 1 - uv[i] + 0; // convention glTF (flipY désactivé)
      g.setAttribute("uv", new THREE.BufferAttribute(uv, 2));
    }
    if (d.colors) g.setAttribute("color", new THREE.BufferAttribute(reorder(b64ToArray(d.colors, Float32Array), 3), 3));
    g.clearGroups();
    for (const gr of d.groups) g.addGroup(gr.start, gr.count, Math.min(gr.material, d.materials.length - 1));
    g.computeBoundingSphere();
    g.computeBoundingBox();
    // correspondance triangle affiché -> polygone / sommets Blender
    let triPoly = b64ToArray(d.tri_poly, Uint32Array);
    let triVerts = b64ToArray(d.tri_verts, Uint32Array);
    if (order) {
      const tp = new Uint32Array(triPoly.length), tv = new Uint32Array(triVerts.length);
      for (let t = 0; t < order.length; t++) { tp[t] = triPoly[order[t]]; tv.set(triVerts.subarray(order[t] * 3, order[t] * 3 + 3), t * 3); }
      triPoly = tp; triVerts = tv;
    }
    g.userData = { triPoly, triVerts };
    return g;
  }

  _buildMesh(entry, d) {
    if (entry.mesh) { entry.group.remove(entry.mesh); this._dispose(entry.mesh); }
    const g = this._geometryFrom(d);
    const mats = this._materialsFrom(d);
    const mesh = new THREE.Mesh(g, mats);
    mesh.userData.name = entry.group.userData.name;
    entry.mesh = mesh;
    entry.data = { faces: d.faces, vertices: d.vertices, triangles: d.triangles, evaluated: d.evaluated };
    entry.group.add(mesh);
    this._applyShading(entry);
  }

  _buildHelper(entry, o) {
    for (const c of [...entry.group.children]) { entry.group.remove(c); this._dispose(c); }
    if (o.type === "EMPTY") {
      const s = 0.25;
      const pts = [[-s, 0, 0], [s, 0, 0], [0, -s, 0], [0, s, 0], [0, 0, -s], [0, 0, s]].map((p) => new THREE.Vector3(...p));
      entry.group.add(new THREE.LineSegments(new THREE.BufferGeometry().setFromPoints(pts), new THREE.LineBasicMaterial({ color: 0x9aa0b4 })));
    } else if (o.type === "LIGHT") {
      const c = new THREE.Color().setRGB(...(o.light?.color || [1, 1, 1]));
      const m = new THREE.Mesh(new THREE.OctahedronGeometry(0.12), new THREE.MeshBasicMaterial({ color: c, wireframe: true }));
      entry.group.add(m);
      const dir = new THREE.Line(new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(), new THREE.Vector3(0, 0, -1.2)]), new THREE.LineDashedMaterial({ color: c, dashSize: 0.08, gapSize: 0.06 }));
      dir.computeLineDistances();
      entry.group.add(dir);
    } else if (o.type === "CAMERA") {
      const f = 0.6, w = 0.35, hh = 0.22;
      const P = (x, y, z) => new THREE.Vector3(x, y, z);
      const pts = [P(0, 0, 0), P(-w, -hh, -f), P(0, 0, 0), P(w, -hh, -f), P(0, 0, 0), P(w, hh, -f), P(0, 0, 0), P(-w, hh, -f),
        P(-w, -hh, -f), P(w, -hh, -f), P(w, -hh, -f), P(w, hh, -f), P(w, hh, -f), P(-w, hh, -f), P(-w, hh, -f), P(-w, -hh, -f),
        P(-w * 0.5, hh * 1.1, -f), P(0, hh * 1.6, -f), P(0, hh * 1.6, -f), P(w * 0.5, hh * 1.1, -f)];
      const col = o.camera?.active ? 0xffc27a : 0x9aa0b4;
      entry.group.add(new THREE.LineSegments(new THREE.BufferGeometry().setFromPoints(pts), new THREE.LineBasicMaterial({ color: col })));
    } else if (o.type === "ARMATURE" && o.bones) {
      // les os sont donnés en coordonnées monde : on les place hors du groupe transformé
      const pts = [];
      for (const b of o.bones) pts.push(new THREE.Vector3(...b.head), new THREE.Vector3(...b.tail));
      const lines = new THREE.LineSegments(new THREE.BufferGeometry().setFromPoints(pts), new THREE.LineBasicMaterial({ color: 0x8fd0ff, depthTest: false, transparent: true, opacity: 0.9 }));
      lines.renderOrder = 900;
      const joints = new THREE.Points(new THREE.BufferGeometry().setFromPoints(pts), new THREE.PointsMaterial({ color: 0xffffff, size: 5, sizeAttenuation: false, depthTest: false }));
      joints.renderOrder = 901;
      const holder = new THREE.Group();
      holder.add(lines, joints);
      holder.userData.worldSpace = true;
      entry.group.add(holder);
      entry.worldHelper = holder;
    }
  }

  _dispose(obj) {
    obj.traverse?.((c) => {
      c.geometry?.dispose?.();
      const m = c.material;
      if (Array.isArray(m)) m.forEach((x) => { x.map?.dispose?.(); x.dispose(); });
      else if (m) { m.map?.dispose?.(); m.dispose?.(); }
    });
  }

  setShading(mode) {
    this.shading = mode;
    for (const e of this.objects.values()) this._applyShading(e);
  }

  _applyShading(entry) {
    if (!entry.mesh) return;
    const mats = entry.mesh.material;
    for (const m of mats) {
      const pv = m.userData.preview;
      m.wireframe = this.shading === "wire";
      if (this.shading === "solid") {
        m.color.setRGB(0.78, 0.78, 0.8);
        m.metalness = 0; m.roughness = 0.6; m.emissiveIntensity = 0;
        m.map && (m.userData.savedMap = m.map, m.map = null);
        m.vertexColors = !!entry.mesh.geometry.attributes.color && this.mode === "vpaint";
      } else {
        if (m.userData.savedMap) { m.map = m.userData.savedMap; m.userData.savedMap = null; }
        m.color.setRGB(...pv.color, THREE.SRGBColorSpace);
        if (m.map) m.color.set(0xffffff);
        m.metalness = Math.min(1, pv.metallic); m.roughness = Math.max(0.05, pv.roughness);
        m.emissiveIntensity = Math.min(pv.emission_strength, 3);
        m.vertexColors = !!entry.mesh.geometry.attributes.color;
      }
      m.transparent = this.shading === "xray" || pv.alpha < 0.999;
      m.opacity = this.shading === "xray" ? 0.35 : pv.alpha;
      m.depthWrite = this.shading !== "xray";
      m.needsUpdate = true;
    }
    this._updateSelectionLook();
  }

  _updateSelectionLook() {
    for (const [name, e] of this.objects) {
      if (e.box) { e.group.remove(e.box); this._dispose(e.box); e.box = null; }
      if (e.worldHelper) {
        e.worldHelper.matrixAutoUpdate = false;
        e.worldHelper.matrix.copy(e.group.matrix).invert();
      }
      if (!this.selected.has(name) || !e.mesh) continue;
      const geo = e.mesh.geometry;
      if (!geo.boundingBox) geo.computeBoundingBox();
      const helper = new THREE.Box3Helper(geo.boundingBox, name === this.active ? ACT_COLOR : SEL_COLOR);
      helper.material.depthTest = false;
      helper.material.transparent = true;
      helper.material.opacity = 0.8;
      helper.renderOrder = 800;
      e.box = helper;
      e.group.add(helper);
    }
  }

  setSelection(names, active) {
    this.selected = new Set(names);
    this.active = active;
    this._updateSelectionLook();
    this._attachGizmo();
  }

  // ------------------------------------------------------------ gizmo
  setTool(tool) {
    this.tool = tool;
    this.brushCursor.visible = false;
    const painting = ["sculpt", "vpaint", "wpaint"].includes(tool);
    this.controls.mouseButtons = painting
      ? { LEFT: null, MIDDLE: THREE.MOUSE.ROTATE, RIGHT: THREE.MOUSE.PAN }
      : { LEFT: THREE.MOUSE.ROTATE, MIDDLE: THREE.MOUSE.DOLLY, RIGHT: THREE.MOUSE.PAN };
    this._attachGizmo();
  }

  _attachGizmo() {
    const modeMap = { move: "translate", rotate: "rotate", scale: "scale" };
    const gm = modeMap[this.tool];
    if (!gm) { this.gizmo.detach(); return; }
    this.gizmo.setMode(gm);
    if (this.mode === "edit") {
      const c = this._editCentroid();
      if (!c || gm === "rotate") { this.gizmo.detach(); return; }
      this.proxy.position.copy(c); this.proxy.quaternion.identity(); this.proxy.scale.set(1, 1, 1);
      this.proxy.updateMatrixWorld();
      this.gizmo.attach(this.proxy);
      return;
    }
    const names = [...this.selected].filter((n) => this.objects.has(n));
    if (!names.length) { this.gizmo.detach(); return; }
    const act = this.objects.get(this.active) || this.objects.get(names[0]);
    act.group.matrix.decompose(this.proxy.position, this.proxy.quaternion, this.proxy.scale);
    this.proxy.updateMatrixWorld();
    this.gizmo.setSpace(gm === "scale" ? "local" : "world");
    this.gizmo.attach(this.proxy);
  }

  _previewDrag() {
    if (!this._dragStart || this.mode === "edit") return;
    const delta = this.proxy.matrixWorld.clone().multiply(this._dragStart.clone().invert());
    for (const n of this.selected) {
      const e = this.objects.get(n);
      if (!e) continue;
      if (!e._startMatrix) e._startMatrix = e.group.matrix.clone();
      e.group.matrix.copy(delta.clone().multiply(e._startMatrix));
      e.group.matrixWorldNeedsUpdate = true;
    }
  }

  _finishDrag() {
    if (!this._dragStart) return;
    const start = this._dragStart;
    this._dragStart = null;
    if (this.mode === "edit") {
      const offset = this.proxy.position.clone().sub(new THREE.Vector3().setFromMatrixPosition(start));
      const entry = this.objects.get(this.active);
      if (!entry || offset.length() < 1e-7) return;
      // décalage monde -> local
      const inv = new THREE.Matrix3().setFromMatrix4(entry.group.matrix).invert();
      offset.applyMatrix3(inv);
      this.hooks.onEditTranslate?.(offset.toArray());
      return;
    }
    const updates = [];
    for (const n of this.selected) {
      const e = this.objects.get(n);
      if (!e || !e._startMatrix) continue;
      updates.push({ name: n, matrix_world: e.group.matrix.toArray() });
      e._startMatrix = null;
    }
    if (updates.length) this.hooks.onTransform?.(updates);
  }

  // ------------------------------------------------------------ événements
  _bindEvents() {
    const el = this.renderer.domElement;
    let down = null;
    let stroke = null;
    el.addEventListener("pointerdown", (ev) => {
      el.focus();
      if (ev.button !== 0) return;
      down = { x: ev.clientX, y: ev.clientY, t: performance.now() };
      if (["sculpt", "vpaint", "wpaint"].includes(this.tool) && !this.gizmo.dragging) {
        const hit = this._pick(ev, { activeOnly: true });
        if (hit) {
          stroke = { points: [], last: null, object: hit.name };
          this._addStrokePoint(stroke, hit);
          el.setPointerCapture(ev.pointerId);
        }
      }
    });
    el.addEventListener("pointermove", (ev) => {
      if (["sculpt", "vpaint", "wpaint"].includes(this.tool)) {
        const hit = this._pick(ev, { activeOnly: true });
        this._showCursor(hit);
        if (stroke && hit) this._addStrokePoint(stroke, hit);
        if (stroke && stroke.points.length >= 24) {
          const pts = stroke.points.splice(0, stroke.points.length);
          this.hooks.onStroke?.(stroke.object, pts, false);
        }
      }
    });
    el.addEventListener("pointerup", (ev) => {
      if (stroke) {
        const pts = stroke.points;
        if (pts.length) this.hooks.onStroke?.(stroke.object, pts, true);
        else this.hooks.onStroke?.(stroke.object, [], true);
        stroke = null;
        return;
      }
      if (!down || ev.button !== 0) return;
      const moved = Math.hypot(ev.clientX - down.x, ev.clientY - down.y);
      down = null;
      if (moved > 4 || this.gizmo.dragging || this.gizmo.axis) return;
      if (this.mode === "edit") { this._editClick(ev); return; }
      if (["sculpt", "vpaint", "wpaint"].includes(this.tool)) return;
      const hit = this._pick(ev);
      this.hooks.onSelect?.(hit ? hit.name : null, ev.shiftKey || ev.ctrlKey, ev.altKey);
    });
    el.addEventListener("dblclick", (ev) => {
      const hit = this._pick(ev);
      if (hit) this.frameObjects([hit.name]);
    });
    el.addEventListener("pointerleave", () => { this.brushCursor.visible = false; });
  }

  _ndc(ev) {
    const r = this.renderer.domElement.getBoundingClientRect();
    this.pointer.set(((ev.clientX - r.left) / r.width) * 2 - 1, -((ev.clientY - r.top) / r.height) * 2 + 1);
    this.raycaster.setFromCamera(this.pointer, this.camera);
  }

  _pick(ev, { activeOnly = false, targets = null } = {}) {
    this._ndc(ev);
    let meshes = targets || [];
    if (!targets) {
      for (const [n, e] of this.objects) {
        if (!e.mesh || !e.group.visible) continue;
        if (activeOnly && n !== this.active) continue;
        meshes.push(e.mesh);
      }
    }
    this.root.updateMatrixWorld(true);
    const hits = this.raycaster.intersectObjects(meshes, false);
    if (!hits.length) return null;
    const h = hits[0];
    const n = h.face ? h.face.normal.clone().transformDirection(h.object.matrixWorld) : new THREE.Vector3(0, 0, 1);
    return { name: h.object.userData.name, point: h.point, normal: n, faceIndex: h.faceIndex, object: h.object };
  }

  _addStrokePoint(stroke, hit) {
    const spacing = this.brush.radius * 0.3;
    if (stroke.last && stroke.last.distanceTo(hit.point) < spacing) return;
    stroke.last = hit.point.clone();
    stroke.points.push({ position: hit.point.toArray(), normal: hit.normal.toArray() });
  }

  _showCursor(hit) {
    if (!hit) { this.brushCursor.visible = false; return; }
    this.brushCursor.visible = true;
    this.brushCursor.position.copy(hit.point).addScaledVector(hit.normal, 0.002);
    this.brushCursor.lookAt(hit.point.clone().add(hit.normal));
    this.brushCursor.scale.setScalar(this.brush.radius);
    this.brushCursor.material.color.set(this.tool === "vpaint" ? this.brush.color : (this.brush.invert ? 0x66aaff : 0xffffff));
  }

  // ------------------------------------------------------------ mode édition
  setMode(mode) {
    this.mode = mode;
    if (mode !== "edit") this.clearEdit();
    for (const e of this.objects.values()) this._applyShading(e);
    this._attachGizmo();
  }

  clearEdit() {
    if (this.editGroup) { this.overlay.remove(this.editGroup); this._dispose(this.editGroup); this.editGroup = null; }
    this.editData = null;
    this.editSel.clear();
    for (const e of this.objects.values()) e.group.visible = true;
  }

  async loadEdit(name) {
    const d = await api("get_mesh", { name, evaluated: false }, { quiet: true });
    this.editData = { name, d, geo: this._geometryFrom(d), co: b64ToArray(d.edit.co, Float32Array), edges: b64ToArray(d.edit.edges, Uint32Array) };
    this.editSel = new Set([...this.editSel].filter((i) => i < (this.selectMode === "face" ? d.faces : d.vertices)));
    this._drawEdit();
  }

  _drawEdit() {
    if (this.editGroup) { this.overlay.remove(this.editGroup); this._dispose(this.editGroup); }
    const ed = this.editData;
    if (!ed) return;
    const entry = this.objects.get(ed.name);
    const g = new THREE.Group();
    g.matrixAutoUpdate = false;
    g.matrix.copy(entry ? entry.group.matrix : new THREE.Matrix4());
    const base = new THREE.Mesh(ed.geo, new THREE.MeshStandardMaterial({ color: 0x8a8f9e, roughness: 0.7, side: THREE.DoubleSide, polygonOffset: true, polygonOffsetFactor: 1, polygonOffsetUnits: 1 }));
    base.userData.name = ed.name;
    g.add(base);
    this.editMesh = base;
    // arêtes
    const lp = [];
    for (let i = 0; i < ed.edges.length; i++) { const v = ed.edges[i]; lp.push(ed.co[v * 3], ed.co[v * 3 + 1], ed.co[v * 3 + 2]); }
    const lg = new THREE.BufferGeometry();
    lg.setAttribute("position", new THREE.Float32BufferAttribute(lp, 3));
    g.add(new THREE.LineSegments(lg, new THREE.LineBasicMaterial({ color: 0x1b1c22 })));
    // sélection
    if (this.selectMode === "face" && this.editSel.size) {
      const tp = ed.geo.userData.triPoly;
      const pos = ed.geo.attributes.position.array;
      const sp = [];
      for (let t = 0; t < tp.length; t++) if (this.editSel.has(tp[t])) for (let k = 0; k < 9; k++) sp.push(pos[t * 9 + k]);
      const sg = new THREE.BufferGeometry();
      sg.setAttribute("position", new THREE.Float32BufferAttribute(sp, 3));
      const sm = new THREE.Mesh(sg, new THREE.MeshBasicMaterial({ color: 0xff9a3c, transparent: true, opacity: 0.55, side: THREE.DoubleSide, depthTest: true, polygonOffset: true, polygonOffsetFactor: -1 }));
      g.add(sm);
    }
    // sommets
    const pg = new THREE.BufferGeometry();
    pg.setAttribute("position", new THREE.BufferAttribute(ed.co, 3));
    const colors = new Float32Array(ed.co.length);
    for (let i = 0; i < ed.co.length / 3; i++) {
      const s = this.selectMode === "vertex" && this.editSel.has(i);
      colors.set(s ? [1, 0.6, 0.2] : [0.1, 0.1, 0.12], i * 3);
    }
    pg.setAttribute("color", new THREE.BufferAttribute(colors, 3));
    g.add(new THREE.Points(pg, new THREE.PointsMaterial({ size: this.selectMode === "vertex" ? 6 : 3, sizeAttenuation: false, vertexColors: true })));
    this.overlay.add(g);
    this.editGroup = g;
    if (entry) entry.group.visible = false;
    this._attachGizmo();
    this._info();
  }

  _editClick(ev) {
    if (!this.editMesh) return;
    this.editGroup.updateMatrixWorld(true);
    const hit = this._pick(ev, { targets: [this.editMesh] });
    if (!hit) {
      if (!ev.shiftKey) { this.editSel.clear(); this._drawEdit(); this.hooks.onEditSelection?.(); }
      return;
    }
    const ud = this.editData.geo.userData;
    let idx;
    if (this.selectMode === "face") idx = ud.triPoly[hit.faceIndex];
    else {
      const local = this.editGroup.worldToLocal(hit.point.clone());
      let best = Infinity;
      for (let k = 0; k < 3; k++) {
        const v = ud.triVerts[hit.faceIndex * 3 + k];
        const d = local.distanceToSquared(new THREE.Vector3(this.editData.co[v * 3], this.editData.co[v * 3 + 1], this.editData.co[v * 3 + 2]));
        if (d < best) { best = d; idx = v; }
      }
    }
    if (ev.shiftKey || ev.ctrlKey) { this.editSel.has(idx) ? this.editSel.delete(idx) : this.editSel.add(idx); }
    else { this.editSel.clear(); this.editSel.add(idx); }
    this._drawEdit();
    this.hooks.onEditSelection?.();
  }

  setEditSelection(indices) {
    this.editSel = new Set(indices);
    this._drawEdit();
    this.hooks.onEditSelection?.();
  }

  _editCentroid() {
    const ed = this.editData;
    if (!ed || !this.editSel.size || !this.editGroup) return null;
    const c = new THREE.Vector3();
    let n = 0;
    const verts = new Set();
    if (this.selectMode === "vertex") this.editSel.forEach((v) => verts.add(v));
    else {
      const tp = ed.geo.userData.triPoly, tv = ed.geo.userData.triVerts;
      for (let t = 0; t < tp.length; t++) if (this.editSel.has(tp[t])) for (let k = 0; k < 3; k++) verts.add(tv[t * 3 + k]);
    }
    verts.forEach((v) => { c.x += ed.co[v * 3]; c.y += ed.co[v * 3 + 1]; c.z += ed.co[v * 3 + 2]; n++; });
    if (!n) return null;
    c.divideScalar(n);
    this.editGroup.updateMatrixWorld(true);
    return c.applyMatrix4(this.editGroup.matrixWorld);
  }

  // ------------------------------------------------------------ vues
  frameObjects(names) {
    const box = new THREE.Box3();
    const list = names && names.length ? names : [...this.objects.keys()];
    for (const n of list) {
      const e = this.objects.get(n);
      if (!e || !e.mesh) continue;
      e.group.updateMatrixWorld(true);
      box.expandByObject(e.mesh);
    }
    if (box.isEmpty()) box.setFromCenterAndSize(new THREE.Vector3(0, 0, 1), new THREE.Vector3(2, 2, 2));
    const c = box.getCenter(new THREE.Vector3());
    const r = Math.max(box.getSize(new THREE.Vector3()).length() / 2, 0.1);
    const dir = this.camera.position.clone().sub(this.controls.target).normalize();
    const dist = r / Math.sin(THREE.MathUtils.degToRad(this.camera.fov / 2)) * 1.05;
    this.controls.target.copy(c);
    this.camera.position.copy(c).addScaledVector(dir, dist);
    this.camera.near = Math.max(0.001, dist / 500);
    this.camera.far = dist * 50 + 100;
    this.camera.updateProjectionMatrix();
  }

  view(name) {
    const t = this.controls.target;
    const d = this.camera.position.distanceTo(t);
    const dirs = { front: [0, -1, 0], back: [0, 1, 0], right: [1, 0, 0], left: [-1, 0, 0], top: [0, -0.0001, 1], persp: [0.65, -0.75, 0.45] };
    const v = new THREE.Vector3(...dirs[name]).normalize();
    this.camera.position.copy(t).addScaledVector(v, d);
    this.camera.lookAt(t);
  }

  cameraView() {
    for (const e of this.objects.values()) {
      if (e.type === "CAMERA" && e.info?.camera?.active) {
        const m = e.group.matrix;
        const pos = new THREE.Vector3(), q = new THREE.Quaternion(), s = new THREE.Vector3();
        m.decompose(pos, q, s);
        this.camera.position.copy(pos);
        const fwd = new THREE.Vector3(0, 0, -1).applyQuaternion(q);
        this.controls.target.copy(pos).addScaledVector(fwd, 5);
        const aspect = this.camera.aspect;
        const fov = e.info.camera.fov || 0.69;
        this.camera.fov = THREE.MathUtils.radToDeg(aspect >= 1 ? 2 * Math.atan(Math.tan(fov / 2) / aspect) : fov);
        this.camera.updateProjectionMatrix();
        return true;
      }
    }
    return false;
  }

  // ------------------------------------------------------------ animation
  async playAnimation(path, fps) {
    this.stopAnimation();
    const gltf = await new GLTFLoader().loadAsync(fileUrl(path));
    const holder = new THREE.Group();
    holder.rotation.x = Math.PI / 2; // glTF Y-up -> Z-up
    holder.add(gltf.scene);
    this.scene.add(holder);
    this.root.visible = false;
    const mixer = new THREE.AnimationMixer(gltf.scene);
    for (const clip of gltf.animations) mixer.clipAction(clip).play();
    this.anim = { holder, mixer, clips: gltf.animations };
    return gltf.animations.length;
  }

  stopAnimation() {
    if (!this.anim) return;
    this.scene.remove(this.anim.holder);
    this._dispose(this.anim.holder);
    this.anim = null;
    this.root.visible = true;
  }

  snapshot() {
    return this.renderer.domElement.toDataURL("image/png");
  }

  _info() {
    let tri = 0, verts = 0;
    for (const e of this.objects.values()) if (e.data) { tri += e.data.triangles; verts += e.data.vertices; }
    const sel = [...this.selected];
    let s = `Objets ${this.objects.size} · Sommets ${verts.toLocaleString("fr")} · Triangles ${tri.toLocaleString("fr")}`;
    if (sel.length) s += `\nSélection : ${sel.slice(0, 3).join(", ")}${sel.length > 3 ? "…" : ""}`;
    if (this.mode === "edit" && this.editData) s += `\nÉdition (${this.selectMode === "face" ? "faces" : "sommets"}) : ${this.editSel.size} sélectionné(s)`;
    this.hooks.onInfo?.(s);
  }
}
