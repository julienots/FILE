// Pouvoirs ultimes : singularité, charge de comète, chute titanesque, pluie d'astres, lames célestes,
// cyclone divin, gravité inversée, arrêt du temps, fissure tellurique, supernova.
// Chaque pouvoir est un « effet » actif mis à jour chaque image, avec sa mise en scène et ses dégâts.
import * as THREE from 'three';
import { G, WORLD, clamp, lerp, rand, emit } from './core.js';
import { makeBlackHole } from './heromodel.js';

export const ULTIMATES = [
  { id: 'singularity', name: 'Singularité', icon: '◉', cd: 14, desc: 'Ouvre un trou noir qui aspire arbres, maisons, débris et mortels, puis explose.' },
  { id: 'comet', name: 'Charge de comète', icon: '➶', cd: 6, desc: 'Le dieu fonce comme un astre et laisse une tranchée de feu derrière lui.' },
  { id: 'leap', name: 'Chute titanesque', icon: '⇊', cd: 9, desc: 'Un bond jusqu\'aux nuages, puis une chute qui creuse un cratère géant.' },
  { id: 'meteorshower', name: 'Pluie d\'astres', icon: '☄☄', cd: 16, desc: 'Une averse de météores s\'abat autour du point visé.' },
  { id: 'swords', name: 'Lames célestes', icon: '⚔', cd: 12, desc: 'Seize épées de lumière tombent du ciel et se plantent dans le sol.' },
  { id: 'tornado', name: 'Cyclone divin', icon: '🌀', cd: 15, desc: 'Une tornade surnaturelle traverse le monde et emporte tout.' },
  { id: 'gravity', name: 'Gravité inversée', icon: '⇈', cd: 12, desc: 'Tout s\'élève dans les airs… puis s\'écrase.' },
  { id: 'timestop', name: 'Arrêt du temps', icon: '⧗', cd: 20, desc: 'Le monde se fige. Seul le dieu se meut.' },
  { id: 'fissure', name: 'Fissure tellurique', icon: '⚡', cd: 8, desc: 'Une faille de lave déchire la terre devant le dieu.' },
  { id: 'supernova', name: 'Supernova', icon: '✹', cd: 30, desc: 'Le dieu concentre la puissance d\'une étoile et la libère.' },
  { id: 'astral', name: 'Déferlante astrale', icon: '☀⟶', cd: 0, desc: 'Une boule d\'énergie entre les mains, puis un rayon colossal qui creuse un canyon jusqu\'à l\'horizon.' },
  { id: 'moon', name: 'Chute de la lune', icon: '🌑', cd: 0, desc: 'Arrache une lune du ciel et l\'écrase sur le point visé. Rien ne survit.' },
  { id: 'lance', name: 'Lance céleste', icon: '⇓', cd: 0, desc: 'Un pilier de lumière tombe de l\'orbite et suit votre regard en brûlant tout.' },
  { id: 'quake', name: 'Séisme titanesque', icon: '⛰', cd: 0, desc: 'Le monde tremble : des pics de roche jaillissent de la terre tout autour.' },
];

const glowTexture = () => { const c = document.createElement('canvas'); c.width = c.height = 128; const x = c.getContext('2d'); const g = x.createRadialGradient(64, 64, 0, 64, 64, 64); g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.3, 'rgba(255,255,255,0.45)'); g.addColorStop(1, 'rgba(255,255,255,0)'); x.fillStyle = g; x.fillRect(0, 0, 128, 128); const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t; };
const V = (x = 0, y = 0, z = 0) => new THREE.Vector3(x, y, z);

export class Ultimates {
  constructor(scene) {
    this.scene = scene;
    this.effects = [];
    this.glow = glowTexture();
    // épée de lumière (géométrie partagée)
    const blade = new THREE.CylinderGeometry(0.05, 0.75, 16, 4, 1); blade.rotateY(Math.PI / 4); blade.scale(1, 1, 0.22); blade.translate(0, -8, 0);
    const guard = new THREE.BoxGeometry(4.2, 0.6, 0.7); guard.translate(0, 0.3, 0);
    const grip = new THREE.CylinderGeometry(0.28, 0.28, 3.2, 8); grip.translate(0, 2.2, 0);
    const pommel = new THREE.OctahedronGeometry(0.7, 0); pommel.translate(0, 4.1, 0);
    this.swordParts = [blade, guard, grip, pommel];
    // une seule lumière, créée une fois : le nombre de lumières ne change jamais en jeu
    this.light = new THREE.PointLight(0xffffff, 0, 300, 1.6);
    scene.add(this.light);
    // rayon colossal (déferlante, lance) : trois cylindres additifs, créés une seule fois
    const BV = 'varying float vF; void main(){ vec3 n = normalize(normalMatrix * normal); vec4 mv = modelViewMatrix * vec4(position, 1.0); vF = abs(dot(n, normalize(-mv.xyz))); gl_Position = projectionMatrix * mv; }';
    const BF = 'uniform vec3 uC; uniform float uO; uniform float uT; varying float vF; void main(){ float w = 0.85 + 0.15 * sin(uT * 40.0); gl_FragColor = vec4(uC * pow(vF, 1.6) * uO * w * 1.5, 1.0); }';
    this.mkBeam = () => {
      const g = new THREE.Group();
      for (const [rr, c, o] of [[0.35, 0xffffff, 1], [0.7, 0xffe08a, 0.45], [1.0, 0xff9a30, 0.18]]) {
        const m = new THREE.Mesh(new THREE.CylinderGeometry(rr, rr, 1, 20, 1, true).rotateX(Math.PI / 2).translate(0, 0, 0.5),
          new THREE.ShaderMaterial({ transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, uniforms: { uC: { value: new THREE.Color(c) }, uO: { value: 0 }, uT: { value: 0 } }, vertexShader: BV, fragmentShader: BF }));
        m.frustumCulled = false; g.add(m);
      }
      g.visible = false; scene.add(g); return g;
    };
    this.beamA = this.mkBeam(); this.beamB = this.mkBeam();
    // lune
    const tl = new THREE.TextureLoader(), moonMap = tl.load('img/tex/mossy_rock_diffuse.jpg'); moonMap.colorSpace = THREE.SRGBColorSpace; moonMap.wrapS = moonMap.wrapT = THREE.RepeatWrapping; moonMap.repeat.set(4, 2);
    this.moonMesh = new THREE.Mesh(new THREE.IcosahedronGeometry(1, 4), new THREE.MeshStandardMaterial({ map: moonMap, color: 0xb8b0a8, roughness: 1, emissive: 0xff5010, emissiveIntensity: 0 }));
    { const pp = this.moonMesh.geometry.attributes.position; for (let i = 0; i < pp.count; i++) { const k = 1 + Math.sin(pp.getX(i) * 9.1 + pp.getY(i) * 4.3) * 0.03 + Math.sin(pp.getZ(i) * 13.7) * 0.02; pp.setXYZ(i, pp.getX(i) * k, pp.getY(i) * k, pp.getZ(i) * k); } this.moonMesh.geometry.computeVertexNormals(); }
    this.moonMesh.visible = false; this.moonMesh.castShadow = true; scene.add(this.moonMesh);
  }

  setBeam(b, from, to, width, o, t) {
    const L = from.distanceTo(to);
    b.visible = o > 0.01; b.position.copy(from); b.lookAt(to); b.scale.set(width, width, L);
    b.children.forEach((c, i) => { c.material.uniforms.uO.value = [1, 0.5, 0.2][i] * o; c.material.uniforms.uT.value = t; });
  }

  theme() { return (G.god.model && G.god.model.theme) || { main: 0xffd27a, alt: 0xff9a40, core: 0xfff4d0 }; }

  cast(id, aim) {
    const fn = this[id];
    if (!fn) return false;
    // garde-fou invisible pour le téléphone : 10 effets massifs simultanés au plus
    if (this.effects.length >= 10) return true;
    fn.call(this, aim);
    return true;
  }

  add(effect) { this.effects.push(effect); }

  sprite(color, size) {
    const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: this.glow, color, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
    s.scale.setScalar(size); this.scene.add(s); return s;
  }

  // ---------- aides de mise en scène ----------
  pullTrees(c, r, toward = true, launch = 18) {
    if (!G.veg) return;
    for (const t of G.veg.trees) {
      if (t.state !== 0 && t.state !== 2) continue;
      const dx = c.x - t.x, dz = c.z - t.z, d = Math.hypot(dx, dz);
      if (d > r || Math.random() > 0.08) continue;
      const k = toward ? 1 : -1;
      G.veg.fell(t, dx / d * k, dz / d * k, launch * (1 - d / r) + 8);
      G.veg.dirty = true;
    }
  }
  pullPeople(c, r, power = 30) {
    if (!G.people) return;
    for (const p of G.people.list) {
      if (!p.alive || p.pos.distanceTo(c) > r || Math.random() > 0.1) continue;
      const mirror = p.pos.clone().multiplyScalar(2).sub(c); // projeté VERS le centre
      p.fling(mirror, power);
    }
  }
  attract(c, r, k, dt, swirl = 0, lift = 0) {
    if (!G.physics) return;
    for (const pool of [...G.physics.pools.values(), G.physics.rockPool]) {
      for (let i = 0; i < pool.used; i++) {
        const j = i * 3, dx = c.x - pool.p[j], dy = c.y - pool.p[j + 1], dz = c.z - pool.p[j + 2], d = Math.hypot(dx, dy, dz);
        if (d > r || d < 0.5) continue;
        const f = k * (1 - d / r) * dt;
        pool.v[j] += dx / d * f - dz / d * swirl * dt; pool.v[j + 1] += dy / d * f + lift * dt + 28 * dt * (lift ? 1 : 0.6); pool.v[j + 2] += dz / d * f + dx / d * swirl * dt;
        pool.awake[i] = 1; pool.age[i] = 0;
      }
    }
  }

  // ---------- 1. Singularité ----------
  singularity(aim) {
    const T = this.theme(), god = G.god;
    god.playAction('push', 1.2);
    const c = aim.point.clone(); c.y = Math.max(c.y, G.terrain.groundAt(c.x, c.z)) + 16;
    const bh = makeBlackHole(1, false);
    bh.position.copy(c); bh.scale.setScalar(0.01); this.scene.add(bh);
    const halo = this.sprite(T.main, 1);
    const L = this.light; L.color.set(T.main); L.distance = 120; L.position.copy(c);
    emit('journal', { icon: '◉', text: 'Une singularité déchire l\'espace.' });
    if (G.audio) G.audio.roar(c, 0.5);
    let t = 0, tick = 0;
    this.add((dt) => {
      t += dt; tick -= dt;
      const grow = clamp(t / 0.8, 0, 1), end = t > 5.6;
      const R = end ? lerp(7, 0.1, clamp((t - 5.6) / 0.35, 0, 1)) : 7 * grow;
      bh.scale.setScalar(Math.max(0.01, R)); bh.userData.tick(G.time * 2);
      halo.position.copy(c); halo.scale.setScalar(R * 9); halo.material.opacity = 0.5 + Math.sin(t * 8) * 0.1;
      L.intensity = 2500 * grow;
      if (!end) {
        this.attract(c, 90, 70, dt, 14);
        G.uniforms.gust.value.set(c.x, c.y, c.z, 6 * grow);
        if (tick <= 0) {
          tick = 0.25;
          this.pullTrees(c, 75, true, 26); this.pullPeople(c, 60, 28);
          if (G.villages) G.villages.damageArea(c.x, c.z, 34, 160, c);
          const g = c.clone(); g.y = G.terrain.heightAt(c.x, c.z);
          if (c.y - g.y < 30) G.terrain.crater(c.x, c.z, 16, 0.7, { rim: 0, burn: 0.05 });
          G.fx.shake(0.12, c);
          if (G.people) G.people.notify('destroy', c.x, c.z, 60, 60);
        }
        // flux de matière qui spirale vers le centre
        for (let n = 0; n < 6; n++) {
          const a = Math.random() * 6.28, r = rand(18, 45), off = V(Math.cos(a) * r, rand(-8, 8), Math.sin(a) * r);
          const v = off.clone().multiplyScalar(-1.6).add(V(-off.z, 0, off.x).multiplyScalar(0.9));
          const col = new THREE.Color(Math.random() < 0.5 ? T.main : T.alt);
          G.fx.add.emit(c.x + off.x, c.y + off.y, c.z + off.z, v.x, v.y, v.z, rand(0.6, 1.6), 0.9, col.r, col.g, col.b, 0.9, 0.6, 0, 0);
        }
      }
      if (t > 6.0) {
        this.scene.remove(bh); this.scene.remove(halo); L.intensity = 0;
        const g = c.clone(); g.y = G.terrain.heightAt(c.x, c.z);
        G.fx.sphere(c, 70, 1.4, T.main); G.fx.sphere(c, 40, 0.9, 0xffffff); G.fx.ring(g, 260, 2, T.main); G.fx.ring(g, 160, 1.4, T.alt);
        G.fx.flash(c, 14000, 1.2, T.main); G.fx.shake(1, c); if (G.env) G.env.flash = 0.6;
        G.terrain.crater(g.x, g.z, 30, 9, { rim: 0.35, burn: 0.8, heat: 0.4 });
        G.fx.rocks(g, 50, 30); G.fx.dust(g, 26, 40, [0.3, 0.25, 0.35]);
        G.powers.areaDamage(g, 60, 1400, 'blast', 'singularity');
        G.uniforms.gust.value.w = 0;
        if (G.audio) G.audio.explosion(c, 1.3);
        return false;
      }
      return true;
    });
  }

  // ---------- 2. Charge de comète ----------
  comet() {
    const T = this.theme(), god = G.god;
    const dir = V(Math.sin(god.heading), 0, Math.cos(god.heading));
    if (god.state === 'fly') { const cd = new THREE.Vector3(); G.camera.getWorldDirection(cd); dir.copy(cd).normalize(); }
    god.playAction('dash', 1.0);
    god.dash = { dir, t: 0, dur: 0.85, speed: 150 };
    if (G.audio) G.audio.whoosh(god.pos);
    let tick = 0;
    this.add((dt) => {
      const d = god.dash; if (!d) return false;
      tick -= dt;
      const p = god.pos.clone().add(V(0, 2.5, 0));
      for (let n = 0; n < 5; n++) { const c = new THREE.Color(Math.random() < 0.6 ? T.main : T.alt); G.fx.add.emit(p.x + rand(-1.5, 1.5), p.y + rand(-1.5, 1.5), p.z + rand(-1.5, 1.5), -d.dir.x * 20 + rand(-3, 3), rand(-2, 4), -d.dir.z * 20 + rand(-3, 3), rand(1.5, 3.5), rand(0.4, 0.9), c.r, c.g, c.b, 1, 0.6, 0, 1); }
      if (tick <= 0) {
        tick = 0.045;
        const g = god.pos.clone(); g.y = G.terrain.heightAt(g.x, g.z);
        if (god.pos.y - g.y < 12) { G.terrain.scorch(g.x, g.z, 4.5, 1.8, 0.9, 0.8); G.fx.decal(g, 7); G.fx.rocks(g, 3, 14); G.fx.fire(g, 4); }
        G.powers.areaDamage(god.pos.clone(), 11, 520, 'blast', 'comet');
        G.fx.shake(0.12, god.pos);
      }
      return true;
    });
  }

  // ---------- 3. Chute titanesque ----------
  leap(aim) {
    const god = G.god, T = this.theme();
    const target = aim.point.clone();
    god.state = 'air'; god.vel.set(0, 118, 0); god.playAction('summon', 1.4);
    const g0 = god.pos.clone(); g0.y = G.terrain.groundAt(g0.x, g0.z);
    G.fx.ring(g0, 60, 0.8, T.main); G.fx.dust(g0, 10, 20); G.terrain.crater(g0.x, g0.z, 6, 2, { rim: 0.4 });
    if (G.audio) G.audio.impact(g0, 0.6);
    let phase = 0, t = 0;
    this.add((dt) => {
      t += dt;
      if (phase === 0 && (god.vel.y < 6 || t > 3)) {
        phase = 1; god.playAction('dive', 2);
        const to = target.clone().sub(god.pos).normalize();
        god.vel.copy(to.multiplyScalar(170));
        god.heading = Math.atan2(god.vel.x, god.vel.z);
        if (G.audio) G.audio.meteorWhistle();
      }
      if (phase === 1) {
        const p = god.pos;
        for (let n = 0; n < 6; n++) { const c = new THREE.Color(Math.random() < 0.5 ? T.main : T.alt); G.fx.add.emit(p.x + rand(-2, 2), p.y + 3 + rand(-2, 2), p.z + rand(-2, 2), -god.vel.x * 0.1, -god.vel.y * 0.1, -god.vel.z * 0.1, rand(2, 4), 0.6, c.r, c.g, c.b, 1, 0.5, 0, 1); }
        if (god.state === 'ground') {
          const g = god.pos.clone();
          G.terrain.crater(g.x, g.z, 34, 12, { rim: 0.45, burn: 0.6, heat: 0.7 });
          G.fx.sphere(g.clone().add(V(0, 5, 0)), 40, 1.0, T.main); G.fx.ring(g, 300, 2.2, T.main); G.fx.ring(g, 180, 1.5, 0xffffff);
          G.fx.flash(g.clone().add(V(0, 10, 0)), 12000, 1, T.alt); G.fx.shake(1, g); G.fx.rocks(g, 70, 32); G.fx.dust(g, 30, 50); G.fx.decal(g, 80);
          G.powers.areaDamage(g, 75, 1500, 'blast', 'leap');
          if (G.env) G.env.flash = 0.5;
          if (G.audio) G.audio.explosion(g, 1.2);
          return false;
        }
      }
      return t < 8;
    });
  }

  // ---------- 4. Pluie d'astres ----------
  meteorshower(aim) {
    G.god.playAction('summon', 1.2);
    const c = aim.point.clone();
    let n = 0, t = 0;
    emit('journal', { icon: '☄', text: 'Le ciel se fend : une pluie d\'astres s\'abat.' });
    this.add((dt) => {
      t += dt;
      while (n < 14 && t > n * 0.3) {
        const a = Math.random() * 6.28, r = Math.sqrt(Math.random()) * 90;
        const p = V(c.x + Math.cos(a) * r, 0, c.z + Math.sin(a) * r); p.y = G.terrain.heightAt(p.x, p.z);
        G.powers.spawnMeteor(p, rand(0.45, 0.8));
        n++;
      }
      return n < 14;
    });
  }

  // ---------- 5. Lames célestes ----------
  swords(aim) {
    const T = this.theme(), god = G.god;
    god.playAction('summon', 1.6);
    const mat = new THREE.MeshStandardMaterial({ color: 0x222222, emissive: T.main, emissiveIntensity: 2.6, metalness: 0.6, roughness: 0.25 });
    const coreMat = new THREE.MeshBasicMaterial({ color: 0xffffff });
    const c = aim.point.clone();
    const list = [];
    for (let i = 0; i < 16; i++) {
      const a = i * 2.39996, r = 6 + Math.sqrt(i / 16) * 46;
      const target = V(c.x + Math.cos(a) * r, 0, c.z + Math.sin(a) * r); target.y = G.terrain.heightAt(target.x, target.z);
      const grp = new THREE.Group();
      this.swordParts.forEach((g, k) => { const m = new THREE.Mesh(g, k === 0 ? mat : (k === 3 ? coreMat : mat)); m.castShadow = true; grp.add(m); });
      grp.scale.setScalar(1.4); grp.visible = false;
      grp.rotation.set(rand(-0.15, 0.15), rand(0, 6.28), rand(-0.15, 0.15));
      this.scene.add(grp);
      const halo = this.sprite(T.main, 0.1);
      list.push({ grp, halo, target, delay: 0.5 + i * 0.13, state: 0, y: target.y + 140 });
    }
    let t = 0;
    this.add((dt) => {
      t += dt;
      let alive = 0;
      for (const s of list) {
        if (s.state === 3) continue;
        alive++;
        if (s.state === 0 && t > s.delay) { s.state = 1; s.grp.visible = true; }
        if (s.state === 1) {
          s.y -= 210 * dt;
          s.grp.position.set(s.target.x, Math.max(s.y, s.target.y + 8), s.target.z);
          s.halo.position.copy(s.grp.position).add(V(0, -6, 0)); s.halo.scale.setScalar(14);
          G.fx.add.emit(s.grp.position.x, s.grp.position.y + 4, s.grp.position.z, 0, 30, 0, 2.5, 0.35, 1, 0.9, 0.7, 0.8, 0.6, 0, 0);
          if (s.y <= s.target.y + 8) {
            s.state = 2; s.t = 0;
            const g = s.target;
            G.terrain.crater(g.x, g.z, 6, 2.4, { rim: 0.4, burn: 0.3, heat: 0.4 });
            G.fx.ring(g, 40, 0.7, T.main); G.fx.sparks(g.clone().add(V(0, 1, 0)), 40, [1, 0.9, 0.6], 20); G.fx.dust(g, 5, 8); G.fx.decal(g, 10); G.fx.shake(0.3, g);
            G.fx.flash(g.clone().add(V(0, 4, 0)), 3000, 0.3, T.main);
            G.powers.areaDamage(g, 11, 560, 'blast', 'sword');
            if (G.audio) G.audio.impact(g, 0.5);
          }
        }
        if (s.state === 2) {
          s.t += dt;
          s.halo.material.opacity = Math.max(0, 1 - s.t / 2);
          if (s.t > 7) { const k = Math.max(0, 1 - (s.t - 7) / 0.8); s.grp.scale.setScalar(1.4 * k); if (k <= 0) { s.state = 3; this.scene.remove(s.grp); this.scene.remove(s.halo); } }
        }
      }
      return alive > 0;
    });
  }

  // ---------- 6. Cyclone divin ----------
  tornado(aim) {
    const T = this.theme(), god = G.god;
    god.playAction('summon', 1.0);
    const pos = god.pos.clone().add(V(Math.sin(god.heading) * 25, 0, Math.cos(god.heading) * 25));
    const target = aim.point.clone();
    const mat = new THREE.ShaderMaterial({ transparent: true, depthWrite: false, side: THREE.DoubleSide, blending: THREE.NormalBlending,
      uniforms: { uT: { value: 0 }, uC: { value: new THREE.Color(T.main) } },
      vertexShader: 'varying vec2 vUv; void main(){ vUv = uv; vec3 p = position; p.xz *= 0.35 + uv.y * 1.4 + sin(uv.y * 9.0) * 0.06; gl_Position = projectionMatrix * modelViewMatrix * vec4(p, 1.0); }',
      fragmentShader: `uniform float uT; uniform vec3 uC; varying vec2 vUv;
        float h(vec2 p){ return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
        float n(vec2 p){ vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f); return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y); }
        void main(){ float s = n(vec2(vUv.x * 10.0 + uT * 3.0 - vUv.y * 6.0, vUv.y * 8.0 - uT * 2.0)) * 0.7 + n(vec2(vUv.x * 24.0 - uT * 5.0, vUv.y * 20.0)) * 0.3;
          float a = smoothstep(0.35, 0.8, s) * smoothstep(0.0, 0.12, vUv.y) * smoothstep(1.0, 0.75, vUv.y) * 0.75;
          vec3 col = mix(vec3(0.35, 0.32, 0.3), uC, 0.35 + vUv.y * 0.3) * (0.6 + s * 0.6);
          gl_FragColor = vec4(col, a); }` });
    const tw = new THREE.Group();
    for (let i = 0; i < 2; i++) { const m = new THREE.Mesh(new THREE.CylinderGeometry(12, 3, 80, 24, 1, true).translate(0, 40, 0), mat); m.scale.setScalar(1 - i * 0.18); tw.add(m); }
    this.scene.add(tw);
    let t = 0, tick = 0;
    emit('journal', { icon: '🌀', text: 'Un cyclone divin se lève.' });
    this.add((dt) => {
      t += dt; tick -= dt;
      const dir = target.clone().sub(pos).setY(0); const L = dir.length();
      if (L > 3) pos.addScaledVector(dir.normalize(), Math.min(L, 16 * dt));
      pos.y = G.terrain.groundAt(pos.x, pos.z);
      tw.position.copy(pos); tw.rotation.y += dt * 4; mat.uniforms.uT.value = t;
      const grow = clamp(t / 1, 0, 1) * clamp((9.5 - t) / 1, 0, 1); tw.scale.set(grow, grow, grow);
      this.attract(pos.clone().add(V(0, 20, 0)), 40, 30, dt, 40, 18);
      G.uniforms.gust.value.set(pos.x, pos.y, pos.z, 6);
      for (let n = 0; n < 5; n++) { const a = Math.random() * 6.28, r = rand(3, 14), h = rand(0, 60); G.fx.norm.emit(pos.x + Math.cos(a) * r, pos.y + h, pos.z + Math.sin(a) * r, -Math.sin(a) * 22, rand(4, 10), Math.cos(a) * 22, rand(2, 5), 1.2, 0.5, 0.46, 0.4, 0.4, 1.2, -2, 0.2); }
      if (tick <= 0) {
        tick = 0.2;
        this.pullTrees(pos, 26, true, 30); this.pullPeople(pos, 24, 22);
        if (G.villages) G.villages.damageArea(pos.x, pos.z, 14, 220, pos);
        G.terrain.crater(pos.x, pos.z, 8, 0.3, { rim: 0 });
        G.fx.shake(0.08, pos);
        if (G.people) G.people.notify('destroy', pos.x, pos.z, 40, 50);
      }
      if (t > 10) { this.scene.remove(tw); G.uniforms.gust.value.w = 0; return false; }
      return true;
    });
  }

  // ---------- 7. Gravité inversée ----------
  gravity(aim) {
    const T = this.theme(), god = G.god;
    god.playAction('charge', 2.2);
    const c = aim.point.clone();
    G.fx.ring(c, 110, 1.2, T.main);
    G.fx.rocks(c, 60, 4);
    let t = 0, slammed = false;
    this.add((dt) => {
      t += dt;
      if (t < 2.2) {
        this.attract(c.clone().add(V(0, 30, 0)), 60, 0, dt, 0, 34);
        if (Math.random() < dt * 10) { this.pullTrees(c, 50, true, 4); }
        if (G.people && Math.random() < dt * 6) for (const p of G.people.list) if (p.alive && p.pos.distanceTo(c) < 50 && Math.random() < 0.2) { p.fling(p.pos.clone().add(V(0, -5, 0)), 4); p.vel.set(rand(-2, 2), rand(14, 20), rand(-2, 2)); }
        for (let n = 0; n < 6; n++) { const a = Math.random() * 6.28, r = Math.sqrt(Math.random()) * 55; const col = new THREE.Color(T.main); G.fx.add.emit(c.x + Math.cos(a) * r, G.terrain.heightAt(c.x + Math.cos(a) * r, c.z + Math.sin(a) * r), c.z + Math.sin(a) * r, 0, rand(12, 30), 0, rand(0.4, 1), 1.4, col.r, col.g, col.b, 0.9, 0.7, 0, 0); }
        return true;
      }
      if (!slammed) {
        slammed = true;
        god.playAction('slam', 0.6);
        this.attract(c.clone().add(V(0, -40, 0)), 90, 220, 1, 0, 0);
        G.fx.ring(c, 200, 1.6, T.alt); G.fx.shake(0.9, c); G.fx.dust(c, 30, 40);
        G.terrain.crater(c.x, c.z, 26, 5, { rim: 0.4, burn: 0.2 });
        G.powers.areaDamage(c, 60, 900, 'blast', 'gravity');
        if (G.audio) G.audio.impact(c, 1);
      }
      return t < 3;
    });
  }

  // ---------- 8. Arrêt du temps ----------
  timestop() {
    const T = this.theme(), god = G.god;
    god.playAction('pose', 1.2);
    G.timeStop = true;
    G.fx.sphere(god.pos.clone().add(V(0, 3, 0)), 200, 1.2, T.main);
    if (G.audio) G.audio.teleport();
    const cv = G.renderer.domElement;
    cv.style.transition = 'filter 0.6s'; cv.style.filter = 'grayscale(0.92) contrast(1.15) brightness(0.95)';
    emit('journal', { icon: '⧗', text: 'Le temps s\'arrête. Le monde retient son souffle.' });
    let t = 0;
    this.add((dt) => {
      t += dt;
      if (t > 6) { G.timeStop = false; cv.style.filter = ''; G.fx.ring(god.pos, 120, 1, T.main); return false; }
      return true;
    });
  }

  // ---------- 9. Fissure tellurique ----------
  fissure() {
    const god = G.god, T = this.theme();
    god.playAction('slam', 0.8);
    const dir = V(Math.sin(god.heading), 0, Math.cos(god.heading));
    const start = god.pos.clone();
    let t = 0, done = 0;
    this.add((dt) => {
      t += dt;
      const reach = Math.min(170, t * 110);
      while (done < reach) {
        done += 5;
        const wob = Math.sin(done * 0.12) * 4;
        const p = start.clone().addScaledVector(dir, done).add(V(-dir.z * wob, 0, dir.x * wob)); p.y = G.terrain.heightAt(p.x, p.z);
        G.terrain.crater(p.x, p.z, 5 + Math.random() * 2, 5, { rim: 0.5, burn: 0.6, heat: 1 });
        G.fx.rocks(p, 4, 16); G.fx.fire(p.clone().add(V(0, 1, 0)), 5); G.fx.dust(p, 4, 3); G.fx.shake(0.15, p);
        if (done % 20 === 0) G.fx.flash(p.clone().add(V(0, 3, 0)), 2500, 0.3, T.alt);
        G.powers.areaDamage(p, 12, 600, 'blast', 'fissure');
      }
      return reach < 170;
    });
    if (G.audio) G.audio.impact(start, 1);
  }

  // ---------- 10. Supernova ----------
  supernova() {
    const god = G.god, T = this.theme();
    god.playAction('charge', 2.4);
    god.state = 'fly'; god.vel.set(0, 7, 0);
    const core = this.sprite(0xffffff, 1);
    const L = this.light; L.color.set(T.alt); L.distance = 300;
    emit('journal', { icon: '✹', text: 'Le dieu concentre la puissance d\'une étoile…' });
    let t = 0, boom = false;
    this.add((dt) => {
      t += dt;
      const c = god.pos.clone().add(V(0, 3, 0));
      if (t < 2.4) {
        const k = t / 2.4;
        core.position.copy(c); core.scale.setScalar(4 + k * 26); L.position.copy(c); L.intensity = 4000 * k;
        for (let n = 0; n < 10; n++) { const d = V(rand(-1, 1), rand(-1, 1), rand(-1, 1)).normalize(); const r = rand(30, 70); const col = new THREE.Color(Math.random() < 0.5 ? T.main : T.alt); G.fx.add.emit(c.x + d.x * r, c.y + d.y * r, c.z + d.z * r, -d.x * r * 1.6, -d.y * r * 1.6, -d.z * r * 1.6, rand(1, 2.5), 0.6, col.r, col.g, col.b, 1, 0.5, 0, 0); }
        G.fx.shake(0.05 + k * 0.1, c);
        return true;
      }
      if (!boom) {
        boom = true;
        this.scene.remove(core); L.intensity = 0;
        const g = c.clone(); g.y = G.terrain.heightAt(c.x, c.z);
        G.fx.sphere(c, 160, 2.2, T.alt); G.fx.sphere(c, 90, 1.4, 0xffffff); G.fx.sphere(c, 220, 2.8, T.main);
        G.fx.ring(g, 520, 3, T.alt); G.fx.ring(g, 320, 2.2, 0xffffff);
        G.fx.flash(c, 30000, 2.2, T.alt); G.fx.shake(1, c); if (G.env) G.env.flash = 1;
        G.terrain.crater(g.x, g.z, 60, 22, { rim: 0.35, burn: 1, heat: 0.8 });
        G.fx.rocks(g, 120, 45); G.fx.dust(g, 40, 70, [0.3, 0.27, 0.25]);
        G.powers.areaDamage(g, 140, 2600, 'blast', 'supernova');
        if (G.veg) G.veg.damageArea(g.x, g.z, 170, 3, 'burn');
        emit('journal', { icon: '✹', text: 'SUPERNOVA. Le paysage est rayé de la carte.' });
        if (G.audio) G.audio.explosion(c, 1.5);
      }
      return t < 4;
    });
  }

  // ---------- 11. Déferlante astrale ----------
  astral(aim) {
    const god = G.god, T = this.theme();
    if (this.beamBusy) return; this.beamBusy = true;
    god.playAction('push', 4.2);
    const ball = this.sprite(0xffe9a0, 1);
    const L = this.light; L.color.set(0xffd27a); L.distance = 200;
    emit('journal', { icon: '☀', text: 'Le dieu rassemble l\'énergie des astres entre ses mains…' });
    let t = 0, front = 0, fired = false;
    const B = this.beamA;
    this.add((dt) => {
      t += dt;
      const a = G.cam.aim(1200);
      const fwd = a.dir.clone(); fwd.y = Math.max(-0.6, Math.min(0.35, fwd.y)); fwd.normalize();
      god.heading = Math.atan2(fwd.x, fwd.z);
      const hand = god.pos.clone().add(V(0, god.height * 0.5, 0)).addScaledVector(fwd, god.height * 0.35);
      if (t < 1.3) { // charge
        const k = t / 1.3;
        ball.position.copy(hand); ball.scale.setScalar(1 + k * 7 * (god.height / 5.2 + 0.4)); L.position.copy(hand); L.intensity = 1500 * k;
        for (let n = 0; n < 6; n++) { const d = V(rand(-1, 1), rand(-1, 1), rand(-1, 1)).normalize(); const r = rand(8, 20); G.fx.add.emit(hand.x + d.x * r, hand.y + d.y * r, hand.z + d.z * r, -d.x * r * 3, -d.y * r * 3, -d.z * r * 3, rand(0.6, 1.4), 0.33, 1, 0.9, 0.5, 1, 0.2, 0, 0); }
        G.fx.shake(0.04 + k * 0.08, hand);
        if (t + dt >= 1.3 && G.audio) { G.audio.laser(true); G.audio.explosion(hand, 0.6); }
        return true;
      }
      if (!fired) { fired = true; G.fx.sphere(hand, 18, 0.5, 0xfff0c0); G.fx.ring(hand, 60, 0.7, 0xffd27a); G.fx.shake(0.8, hand); if (G.env) G.env.flash = 0.6; }
      const k = t < 3.9 ? Math.min(1, (t - 1.3) * 5) : Math.max(0, 1 - (t - 3.9) / 0.4);
      front = Math.min(650, front + dt * 520);
      const hit = G.terrain.raycast(hand, fwd, front);
      const end = hit ? hit.point.clone() : hand.clone().addScaledVector(fwd, front);
      this.setBeam(B, hand, end, 9 * k * (G.god.power > 1 ? 1.4 : 1), k, t);
      ball.position.copy(hand); ball.scale.setScalar(14 * k); L.position.copy(hand); L.intensity = 2500 * k;
      // creuse un canyon le long du rayon (quelques points par image seulement)
      this.astT = (this.astT || 0) - dt;
      if (this.astT <= 0 && k > 0.5) {
        this.astT = 0.06;
        const len = hand.distanceTo(end);
        for (let n = 0; n < 3; n++) {
          const q = hand.clone().addScaledVector(fwd, rand(20, len));
          const gy = G.terrain.heightAt(q.x, q.z);
          if (q.y - gy < 22) { G.terrain.crater(q.x, q.z, 11, 5, { rim: 0.3, burn: 1, heat: 1 }); G.fx.fire(q.clone().setY(gy + 1), 10); G.fx.rocks(q.clone().setY(gy), 4, 20); }
          G.powers.areaDamage(q, 22, 900, 'blast', 'astral');
        }
        if (hit) { G.terrain.crater(end.x, end.z, 22, 10, { rim: 0.4, burn: 1, heat: 1 }); G.fx.sphere(end, 26, 0.4, 0xffd890); G.fx.dust(end, 20, 10); G.powers.areaDamage(end, 45, 1800, 'blast', 'astral'); G.fx.shake(0.35, end); }
      }
      if (t > 4.3) { B.visible = false; this.scene.remove(ball); L.intensity = 0; this.beamBusy = false; if (G.audio) G.audio.laser(false); emit('journal', { icon: '☀', text: 'Un canyon fumant raye désormais le monde.' }); return false; }
      return true;
    });
  }

  // ---------- 12. Chute de la lune ----------
  moon(aim) {
    if (this.moonBusy) return; this.moonBusy = true;
    const god = G.god, M = this.moonMesh;
    god.playAction('summon', 2.5);
    const tgt = aim.point.clone(); tgt.y = G.terrain.heightAt(tgt.x, tgt.z);
    const R = 55;
    const start = tgt.clone().add(V(rand(-300, 300), 900, rand(-300, 300)));
    M.visible = true; M.scale.setScalar(R); M.position.copy(start); M.material.emissiveIntensity = 0;
    emit('journal', { icon: '🌑', text: 'Le dieu arrache une lune au firmament…' });
    if (G.audio) G.audio.meteorWhistle();
    let t = 0, boom = false; const dur = 6.5;
    this.add((dt) => {
      t += dt;
      if (t < dur) {
        const k = t / dur, e = k * k * k;
        M.position.lerpVectors(start, tgt.clone().add(V(0, R * 0.6, 0)), e);
        M.rotation.x += dt * 0.15; M.rotation.z += dt * 0.08;
        M.material.emissiveIntensity = k * 1.2;
        if (k > 0.5) for (let n = 0; n < 6; n++) { const q = M.position.clone().add(V(rand(-1, 1), rand(-1, 1), rand(-1, 1)).normalize().multiplyScalar(R)); G.fx.fire(q, 30); }
        G.fx.shake(0.05 + k * 0.35, tgt);
        if (k > 0.75) { this.pullTrees(tgt, 220, false, 10); G.uniforms.gust.value.set(tgt.x, tgt.y, tgt.z, 6); }
        return true;
      }
      if (!boom) {
        boom = true; M.visible = false;
        const c = tgt.clone();
        G.terrain.crater(c.x, c.z, 130, 38, { rim: 0.5, burn: 1, heat: 1 });
        G.fx.sphere(c.clone().add(V(0, 20, 0)), 260, 3, 0xffb060); G.fx.sphere(c, 140, 1.8, 0xffffff);
        G.fx.ring(c, 900, 4, 0xffc890); G.fx.ring(c, 500, 2.6, 0xffffff);
        G.fx.flash(c.clone().add(V(0, 40, 0)), 40000, 3, 0xffa050); G.fx.shake(1, c); if (G.env) G.env.flash = 1;
        G.fx.rocks(c, 160, 60); G.fx.dust(c, 80, 90, [0.33, 0.29, 0.26]);
        for (let n = 0; n < 50; n++) G.fx.fire(c.clone().add(V(rand(-120, 120), rand(0, 20), rand(-120, 120))), 20);
        G.powers.areaDamage(c, 260, 5000, 'blast', 'moon');
        if (G.veg) G.veg.damageArea(c.x, c.z, 300, 3, 'burn');
        if (G.audio) { G.audio.explosion(c, 2); setTimeout(() => G.audio.thunder(c, 1), 400); }
        emit('journal', { icon: '🌑', text: 'La lune s\'est écrasée. Le monde ne sera plus jamais le même.' });
      }
      if (t > dur + 2) { this.moonBusy = false; return false; }
      return true;
    });
  }

  // ---------- 13. Lance céleste ----------
  lance() {
    if (this.lanceBusy) return; this.lanceBusy = true;
    const god = G.god, B = this.beamB, L = this.light;
    god.playAction('raise', 1.2);
    L.color.set(0xd8e8ff); L.distance = 250;
    emit('journal', { icon: '⇓', text: 'Une lance de lumière tombe de l\'orbite.' });
    if (G.audio) G.audio.thunder(god.pos, 1);
    let t = 0; const p = G.cam.aim(900).point.clone();
    this.add((dt) => {
      t += dt;
      const a = G.cam.aim(900).point; p.lerp(a, 1 - Math.exp(-2.5 * dt)); // suit le regard
      const gy = G.terrain.heightAt(p.x, p.z); p.y = gy;
      const k = t < 0.3 ? t / 0.3 : (t > 5 ? Math.max(0, 1 - (t - 5) / 0.4) : 1);
      this.setBeam(B, p.clone().add(V(0, 1400, 0)), p, 16 * k, k, t);
      L.position.copy(p).add(V(0, 20, 0)); L.intensity = 5000 * k;
      this.lanceT = (this.lanceT || 0) - dt;
      if (this.lanceT <= 0 && k > 0.5) {
        this.lanceT = 0.08;
        G.terrain.crater(p.x, p.z, 14, 4, { rim: 0.4, burn: 1, heat: 1 });
        G.fx.sparks(p, 30, [0.8, 0.9, 1], 40); G.fx.fire(p.clone().add(V(0, 2, 0)), 14); G.fx.smoke(p, 2, 16, 0.4); G.fx.shake(0.25, p);
        G.powers.areaDamage(p, 26, 1100, 'burn', 'lance');
        if (G.veg) G.veg.damageArea(p.x, p.z, 30, 2, 'burn');
      }
      if (t > 5.4) { B.visible = false; L.intensity = 0; this.lanceBusy = false; return false; }
      return true;
    });
  }

  // ---------- 14. Séisme titanesque ----------
  quake() {
    const god = G.god, c = god.pos.clone();
    god.playAction('slam', 0.9);
    emit('journal', { icon: '⛰', text: 'Le dieu frappe la terre : le monde se fend.' });
    if (G.audio) G.audio.impact(c, 1.2);
    let t = 0, n = 0;
    this.add((dt) => {
      t += dt;
      G.fx.shake(0.35 * Math.max(0, 1 - t / 4), c);
      if (t > 0.45 + n * 0.09 && n < 36) {
        n++;
        const a = rand(0, 6.28), d = 25 + n * 7 + rand(-10, 10);
        const x = c.x + Math.cos(a) * d, z = c.z + Math.sin(a) * d;
        const q = V(x, G.terrain.heightAt(x, z), z);
        const r0 = rand(7, 13), h0 = rand(12, 28);
        G.terrain.crater(x, z, r0, -h0 * 0.5, { rim: 0 }); G.terrain.crater(x, z, r0 * 0.45, -h0 * 0.6, { rim: 0 }); // pic de roche qui jaillit
        G.fx.rocks(q, 10, 26); G.fx.dust(q, 10, 12); G.fx.ring(q, 22, 0.6, 0xc8b8a0);
        G.powers.areaDamage(q, 18, 900, 'blast', 'quake');
        if (n % 6 === 0 && G.audio) G.audio.impact(q, 0.7);
      }
      if (G.villages && t < 3 && Math.random() < dt * 6) G.villages.damageArea(c.x + rand(-250, 250), c.z + rand(-250, 250), 20, 400, c);
      return t < 4.5;
    });
  }

  update(dt) {
    for (let i = this.effects.length - 1; i >= 0; i--) {
      let keep = true;
      try { keep = this.effects[i](dt); } catch (e) { console.error(e); keep = false; }
      if (!keep) this.effects.splice(i, 1);
    }
  }
}
