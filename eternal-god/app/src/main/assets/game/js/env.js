// Ciel dynamique, cycle jour/nuit, météo, eau, rivière, nuages, étoiles, îles célestes, volcan.
import * as THREE from 'three';
import { G, WORLD, Q, settings, noise, clamp, lerp, smooth, mulberry32, canvasTexture, softDotTexture, rand, emit } from './core.js';
import { RIVER, riverBed, REGIONS } from './terrain.js';

const C = (r, g, b) => new THREE.Color(r, g, b);

export class Environment {
  constructor(scene, renderer) {
    this.scene = scene;
    this.renderer = renderer;
    this.dayTime = 0.30;          // 0..1 (0.25 = lever du soleil, 0.5 = midi)
    this.day = 1;
    this.weather = 'clear';       // clear | cloudy | storm
    this.weatherT = 0;            // intensité orageuse 0..1
    this.weatherTimer = rand(240, 420);
    this.lightningTimer = 5;
    this.sunDir = new THREE.Vector3();
    this.fogColor = new THREE.Color();
    this.envTimer = 0;
    this.ripples = [];
    this.islands = [];
    this.flash = 0;
  }

  build() {
    const scene = this.scene;
    // ----- Ciel photographique -----
    scene.add(G.photoSky.mesh);
    this.sky = G.photoSky;

    // ----- Lumières -----
    this.hemi = new THREE.HemisphereLight(0xbcd6ff, 0x4a4030, 0.9);
    scene.add(this.hemi);
    this.sun = new THREE.DirectionalLight(0xffffff, 3);
    const sh = Q().shadows;
    if (sh) {
      this.sun.castShadow = true;
      this.sun.shadow.mapSize.set(sh, sh);
      const cam = this.sun.shadow.camera;
      cam.left = -90; cam.right = 90; cam.top = 90; cam.bottom = -90; cam.near = 10; cam.far = 900;
      this.sun.shadow.bias = -0.0006; this.sun.shadow.normalBias = 0.6;
    }
    scene.add(this.sun); scene.add(this.sun.target);

    scene.fog = new THREE.Fog(0x9fb4c8, 260, Q().far * 0.92);

    this.buildWater();
    this.buildRiver();
    this.buildClouds();
    this.buildHorizon();
    this.buildIslands();
    this.buildVolcano();
    this.buildRain();
    this.update(0, true);
  }

  // ---------- Eau ----------
  waterMaterial(isRiver) {
    return new THREE.ShaderMaterial({
      transparent: true, depthWrite: false, fog: true,
      uniforms: THREE.UniformsUtils.merge([THREE.UniformsLib.fog, {
        uTime: { value: 0 }, uSunDir: { value: new THREE.Vector3() }, uSunCol: { value: new THREE.Color() },
        uSky: { value: new THREE.Color() }, uDeep: { value: new THREE.Color(0x0b3b4a) }, uShallow: { value: new THREE.Color(0x2f8a8a) },
        uHeight: { value: null }, uWorld: { value: WORLD.half }, uLevel: { value: WORLD.water },
        uRipples: { value: Array.from({ length: 8 }, () => new THREE.Vector4(0, 0, -100, 0)) }, uNight: { value: 0 },
      }]),
      vertexShader: `
        #include <fog_pars_vertex>
        varying vec3 vW; varying vec2 vUv;
        void main(){ vUv = uv; vec4 w = modelMatrix*vec4(position,1.0); vW = w.xyz;
          vec4 mvPosition = viewMatrix*w; gl_Position = projectionMatrix*mvPosition;
          #include <fog_vertex>
        }`,
      fragmentShader: `
        #include <fog_pars_fragment>
        uniform float uTime, uWorld, uLevel, uNight; uniform vec3 uSunDir, uSunCol, uSky, uDeep, uShallow;
        uniform sampler2D uHeight; uniform vec4 uRipples[8];
        varying vec3 vW; varying vec2 vUv;
        float hash2(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
        float vn(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
          return mix(mix(hash2(i),hash2(i+vec2(1,0)),f.x), mix(hash2(i+vec2(0,1)),hash2(i+vec2(1,1)),f.x), f.y); }
        float waves(vec2 p){
          ${isRiver ? 'p.y += uTime*3.2;' : ''}
          float h = vn(p*0.08 + uTime*0.05)*0.6 + vn(p*0.22 - uTime*0.09)*0.3 + vn(p*0.7 + vec2(uTime*0.25,0.0))*0.12;
          for(int i=0;i<8;i++){ vec4 r = uRipples[i]; float age = uTime - r.z; if(age < 0.0 || age > 4.0) continue;
            float d = distance(vW.xz, r.xy); float ring = sin((d - age*9.0)*1.6) * exp(-abs(d - age*9.0)*0.35) * (1.0 - age/4.0) * r.w;
            h += ring*0.6; }
          return h; }
        void main(){
          vec2 p = ${isRiver ? 'vec2(vUv.x*20.0, vUv.y)' : 'vW.xz'};
          float e = 0.6; float h0 = waves(p), hx = waves(p+vec2(e,0.0)), hz = waves(p+vec2(0.0,e));
          vec3 n = normalize(vec3((h0-hx)*2.2, 1.0, (h0-hz)*2.2));
          vec3 v = normalize(cameraPosition - vW);
          float fres = 0.04 + 0.96*pow(1.0 - max(dot(n,v),0.0), 5.0);
          vec2 tuv = (vW.xz + uWorld) / (uWorld*2.0);
          float ground = texture2D(uHeight, tuv).r;
          float depth = ${isRiver ? '2.2' : 'max(uLevel - ground, 0.0)'};
          vec3 base = mix(uShallow, uDeep, smoothstep(0.0, 9.0, depth)) * (0.35 + 0.65*(1.0-uNight*0.85));
          vec3 refl = uSky;
          vec3 hv = normalize(uSunDir + v);
          float spec = pow(max(dot(n,hv),0.0), 180.0) * 3.0;
          vec3 col = mix(base, refl, fres*${isRiver ? '0.3' : '0.5'}) + uSunCol*spec;
          float foam = ${isRiver ? 'smoothstep(0.38,0.5,abs(vUv.x-0.5))*0.6 + smoothstep(0.62,0.8,h0)*0.25' : '(1.0 - smoothstep(0.0, 1.2, depth))*0.7'};
          foam *= 0.6 + 0.4*vn(vW.xz*0.9 + uTime*0.6);
          col = mix(col, vec3(0.92,0.95,0.97)*(0.4+0.6*(1.0-uNight)), clamp(foam,0.0,1.0));
          float alpha = ${isRiver ? '0.8' : 'clamp(0.55 + depth*0.08 + fres*0.3, 0.0, 0.96)'};
          gl_FragColor = vec4(col, alpha);
          #include <fog_fragment>
        }`,
    });
  }

  buildWater() {
    const mat = this.waterMaterial(false);
    mat.uniforms.uHeight.value = G.terrain.dataTex;
    const geo = new THREE.PlaneGeometry(WORLD.size + 600, WORLD.size + 600, 1, 1).rotateX(-Math.PI / 2);
    this.water = new THREE.Mesh(geo, mat);
    this.water.position.y = WORLD.water;
    this.water.renderOrder = 2;
    this.scene.add(this.water);
  }

  buildRiver() {
    const pts = RIVER.map(([x, z]) => new THREE.Vector3(x, 0, z));
    const curve = new THREE.CatmullRomCurve3(pts, false, 'centripetal');
    const S = 260, W = 20;
    const pos = [], uv = [], idx = [];
    let len = 0, prev = null;
    for (let i = 0; i <= S; i++) {
      const t = i / S;
      const p = curve.getPointAt(t), tg = curve.getTangentAt(t);
      if (prev) len += p.distanceTo(prev); prev = p;
      const nx = -tg.z, nz = tg.x;
      const y = Math.max(riverBed(t) + 2.3, WORLD.water - 0.3);
      for (let s = 0; s < 2; s++) {
        const o = s === 0 ? -W / 2 : W / 2;
        pos.push(p.x + nx * o, y, p.z + nz * o);
        uv.push(s, len * 0.05);
      }
      if (i < S) { const a = i * 2; idx.push(a, a + 2, a + 1, a + 1, a + 2, a + 3); }
    }
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    g.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
    g.setIndex(idx);
    const mat = this.waterMaterial(true);
    mat.uniforms.uHeight.value = G.terrain.dataTex;
    mat.side = THREE.DoubleSide;
    this.river = new THREE.Mesh(g, mat);
    this.river.renderOrder = 1;
    this.riverCurve = curve;
    this.scene.add(this.river);
  }

  addRipple(x, z, strength = 1) {
    const arr = this.water.material.uniforms.uRipples.value;
    let oldest = 0;
    for (let i = 1; i < 8; i++) if (arr[i].z < arr[oldest].z) oldest = i;
    arr[oldest].set(x, z, G.uniforms.time.value, strength);
  }

  // ---------- Nuages (billboards instanciés, poussés par le vol du dieu) ----------
  buildClouds() {
    const tex = canvasTexture(128, 128, (ctx, w) => {
      const r = mulberry32(5);
      for (let i = 0; i < 26; i++) {
        const x = w * (0.25 + r() * 0.5), y = w * (0.3 + r() * 0.45), rad = w * (0.12 + r() * 0.2);
        const g = ctx.createRadialGradient(x, y, 0, x, y, rad);
        g.addColorStop(0, 'rgba(255,255,255,0.32)'); g.addColorStop(1, 'rgba(255,255,255,0)');
        ctx.fillStyle = g; ctx.fillRect(0, 0, w, w);
      }
    });
    const clusters = 46, per = 7;
    this.cloudClusters = [];
    const total = clusters * per;
    const geo = new THREE.InstancedBufferGeometry();
    const base = new THREE.PlaneGeometry(1, 1);
    geo.index = base.index; geo.attributes.position = base.attributes.position; geo.attributes.uv = base.attributes.uv;
    this.cloudOffset = new Float32Array(total * 4);
    geo.setAttribute('aOffset', new THREE.InstancedBufferAttribute(this.cloudOffset, 4));
    this.cloudShade = new Float32Array(total * 2);
    geo.setAttribute('aShade', new THREE.InstancedBufferAttribute(this.cloudShade, 2));
    geo.instanceCount = total;
    const rnd = mulberry32(42);
    for (let c = 0; c < clusters; c++) {
      const cl = { x: (rnd() - 0.5) * 2600, y: 170 + rnd() * 120, z: (rnd() - 0.5) * 2600, vx: 0, vz: 0, spread: 1, alpha: 1, puffs: [] };
      const size = 50 + rnd() * 70;
      for (let p = 0; p < per; p++) cl.puffs.push({ dx: (rnd() - 0.5) * size * 1.6, dy: (rnd() - 0.3) * size * 0.35, dz: (rnd() - 0.5) * size, s: size * (0.6 + rnd() * 0.6), shade: rnd() });
      this.cloudClusters.push(cl);
    }
    this.cloudMat = new THREE.ShaderMaterial({
      transparent: true, depthWrite: false,
      uniforms: { uMap: { value: tex }, uLit: { value: new THREE.Color(1, 1, 1) }, uShadow: { value: new THREE.Color(0.6, 0.65, 0.75) }, uFogCol: { value: new THREE.Color() }, uFar: { value: 2200 }, uCover: { value: 0.0 } },
      vertexShader: `attribute vec4 aOffset; attribute vec2 aShade; varying vec2 vUv; varying float vShade; varying float vAlpha; varying float vDist;
        void main(){ vUv = uv; vShade = aShade.x; vAlpha = aShade.y;
          vec4 mv = viewMatrix * vec4(aOffset.xyz, 1.0); mv.xy += position.xy * aOffset.w; vDist = -mv.z;
          gl_Position = projectionMatrix * mv; }`,
      fragmentShader: `uniform sampler2D uMap; uniform vec3 uLit, uShadow, uFogCol; uniform float uFar, uCover; varying vec2 vUv; varying float vShade; varying float vAlpha; varying float vDist;
        void main(){ vec4 t = texture2D(uMap, vUv); float a = t.a * vAlpha * uCover;
          vec3 col = mix(uShadow, uLit, clamp(vUv.y*0.9 + vShade*0.25, 0.0, 1.0));
          float f = smoothstep(uFar*0.45, uFar, vDist); col = mix(col, uFogCol, f*0.8);
          a *= 1.0 - smoothstep(uFar*0.8, uFar, vDist); a *= smoothstep(8.0, 40.0, vDist);
          if (a < 0.004) discard; gl_FragColor = vec4(col, a); }`,
    });
    this.clouds = new THREE.Mesh(geo, this.cloudMat);
    this.clouds.frustumCulled = false;
    this.clouds.renderOrder = 5;
    this.scene.add(this.clouds);
  }

  updateClouds(dt) {
    const god = G.god, wind = 2.5 + this.weatherT * 6;
    let o = 0;
    for (const cl of this.cloudClusters) {
      cl.x += (wind + cl.vx) * dt; cl.z += (wind * 0.4 + cl.vz) * dt;
      cl.vx *= Math.exp(-0.25 * dt); cl.vz *= Math.exp(-0.25 * dt);
      if (cl.x > 1400) cl.x -= 2800; if (cl.z > 1400) cl.z -= 2800; if (cl.x < -1400) cl.x += 2800; if (cl.z < -1400) cl.z += 2800;
      // le dieu traverse le nuage : il le disperse et le pousse
      if (god) {
        const dx = cl.x - god.pos.x, dy = cl.y - god.pos.y, dz = cl.z - god.pos.z;
        const d = Math.hypot(dx, dy * 1.5, dz);
        const sp = god.vel.length();
        if (d < 110 && sp > 8) {
          const k = (1 - d / 110) * dt;
          cl.vx += (god.vel.x * 0.6 + dx * 0.3) * k; cl.vz += (god.vel.z * 0.6 + dz * 0.3) * k;
          cl.spread = Math.min(2.4, cl.spread + k * 2.5); cl.alpha = Math.max(0.25, cl.alpha - k * 1.5);
        }
      }
      cl.spread = lerp(cl.spread, 1, 1 - Math.exp(-0.05 * dt)); cl.alpha = lerp(cl.alpha, 1, 1 - Math.exp(-0.04 * dt));
      for (const p of cl.puffs) {
        this.cloudOffset[o * 4] = cl.x + p.dx * cl.spread; this.cloudOffset[o * 4 + 1] = cl.y + p.dy * cl.spread; this.cloudOffset[o * 4 + 2] = cl.z + p.dz * cl.spread;
        this.cloudOffset[o * 4 + 3] = p.s * (0.8 + cl.spread * 0.2);
        this.cloudShade[o * 2] = p.shade; this.cloudShade[o * 2 + 1] = cl.alpha;
        o++;
      }
    }
    this.clouds.geometry.attributes.aOffset.needsUpdate = true;
    this.clouds.geometry.attributes.aShade.needsUpdate = true;
  }

  // ---------- Montagnes lointaines (au-delà des bords du monde) ----------
  buildHorizon() {
    const segs = 160, rings = 6, R0 = 1000, R1 = 2900;
    const pos = [], col = [], idx = [];
    for (let r = 0; r <= rings; r++) {
      const t = r / rings, rad = lerp(R0, R1, t);
      for (let s = 0; s <= segs; s++) {
        const a = s / segs * Math.PI * 2;
        const x = Math.cos(a) * rad, z = Math.sin(a) * rad;
        const ridge = noise.ridged(Math.cos(a) * 3 + 50, Math.sin(a) * 3 + t * 2, 5);
        let y = (t === 0 ? 80 : 120 + ridge * 520 * Math.sin(t * Math.PI) + 200 * t) ;
        if (r === rings) y = 40;
        pos.push(x, y, z);
        const snow = y > 420 ? 1 : 0;
        col.push(snow ? 0.8 : 0.16, snow ? 0.82 : 0.2, snow ? 0.88 : 0.24);
      }
    }
    for (let r = 0; r < rings; r++) for (let s = 0; s < segs; s++) {
      const a = r * (segs + 1) + s, b = a + 1, c = a + segs + 1, d = c + 1;
      idx.push(a, c, b, b, c, d);
    }
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    g.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
    g.setIndex(idx); g.computeVertexNormals();
    this.horizon = new THREE.Mesh(g, new THREE.MeshLambertMaterial({ vertexColors: true }));
    this.scene.add(this.horizon);
  }

  // ---------- Archipel céleste : îles flottantes où l'on peut se poser ----------
  buildIslands() {
    const defs = [
      { x: -260, z: -170, y: 235, r: 34 }, { x: 140, z: -330, y: 290, r: 26 }, { x: -430, z: 260, y: 215, r: 40 },
      { x: 60, z: 420, y: 305, r: 30 }, { x: 330, z: -40, y: 260, r: 22 },
    ];
    const tl = new THREE.TextureLoader();
    const rMap = tl.load('img/tex/mossy_rock_diffuse.jpg'), rNor = tl.load('img/tex/mossy_rock_nor_gl.jpg');
    rMap.colorSpace = THREE.SRGBColorSpace;
    for (const t of [rMap, rNor]) { t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(6, 3); t.anisotropy = 4; }
    const rock = new THREE.MeshStandardMaterial({ map: rMap, normalMap: rNor, color: 0xb8b0a4, roughness: 0.95 });
    const gMap = tl.load('img/tex/rocky_terrain_02_diffuse.jpg'); gMap.colorSpace = THREE.SRGBColorSpace; gMap.wrapS = gMap.wrapT = THREE.RepeatWrapping; gMap.repeat.set(5, 5);
    const grass = new THREE.MeshStandardMaterial({ map: gMap, roughness: 0.9 });
    const marble = new THREE.MeshStandardMaterial({ color: 0xf1ece0, roughness: 0.4 });
    const crystal = new THREE.MeshStandardMaterial({ color: 0x9fe6ff, emissive: 0x58c8ff, emissiveIntensity: 1.6, roughness: 0.15, metalness: 0.1, transparent: true, opacity: 0.9 });
    const fallMat = new THREE.ShaderMaterial({
      transparent: true, depthWrite: false, side: THREE.DoubleSide, uniforms: { uTime: G.uniforms.time },
      vertexShader: 'varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix*modelViewMatrix*vec4(position,1.0); }',
      fragmentShader: `uniform float uTime; varying vec2 vUv; float h(vec2 p){ return fract(sin(dot(p, vec2(12.9898,78.233)))*43758.5453); }
        void main(){ float s = h(vec2(floor(vUv.x*24.0), floor(vUv.y*30.0 + uTime*14.0 + h(vec2(floor(vUv.x*24.0),1.0))*30.0)));
          float a = (0.35 + 0.5*s) * smoothstep(0.0,0.15,vUv.x)*smoothstep(1.0,0.85,vUv.x) * smoothstep(0.0,0.3,vUv.y);
          gl_FragColor = vec4(vec3(0.85,0.93,1.0), a); }`,
    });
    const rnd = mulberry32(777);
    for (const d of defs) {
      const grp = new THREE.Group();
      const cone = new THREE.ConeGeometry(d.r, d.r * 2.2, 28, 14);
      cone.rotateX(Math.PI);
      const p = cone.attributes.position;
      for (let i = 0; i < p.count; i++) {
        const y = p.getY(i); const k = 1 + (rnd() - 0.5) * 0.35;
        if (y < d.r * 1.05) { const n = 1 + 0.25 * Math.sin(p.getX(i) * 0.3 + p.getZ(i) * 0.2) + 0.15 * Math.sin(y * 0.4 + p.getX(i)); p.setX(i, p.getX(i) * n); p.setZ(i, p.getZ(i) * n); p.setY(i, y + Math.sin(p.getX(i) * 0.5) * d.r * 0.06); }
      }
      cone.computeVertexNormals();
      const cm = new THREE.Mesh(cone, rock); cm.position.y = -d.r * 1.1; cm.castShadow = true;
      grp.add(cm);
      const top = new THREE.Mesh(new THREE.CylinderGeometry(d.r * 1.02, d.r * 0.98, 2.4, 20), grass);
      top.position.y = -1.2; top.receiveShadow = true;
      grp.add(top);
      // ruines de marbre
      const cols = 6 + Math.floor(rnd() * 4);
      for (let i = 0; i < cols; i++) {
        const a = i / cols * Math.PI * 2, rr = d.r * 0.55;
        const hgt = 5 + rnd() * 9 * (rnd() < 0.3 ? 0.3 : 1);
        const c = new THREE.Mesh(new THREE.CylinderGeometry(0.9, 1.05, hgt, 10), marble);
        c.position.set(Math.cos(a) * rr, hgt / 2, Math.sin(a) * rr); c.castShadow = true;
        grp.add(c);
      }
      const altar = new THREE.Mesh(new THREE.BoxGeometry(6, 1.2, 6), marble); altar.position.y = 0.6; grp.add(altar);
      const cr = new THREE.Mesh(new THREE.OctahedronGeometry(2.2, 0), crystal); cr.position.y = 4.5; cr.scale.y = 1.8; grp.add(cr);
      d.crystal = cr;
      // cascade qui tombe dans le vide
      const fall = new THREE.Mesh(new THREE.PlaneGeometry(7, 140), fallMat);
      const fa = rnd() * Math.PI * 2;
      fall.position.set(Math.cos(fa) * d.r, -70, Math.sin(fa) * d.r); fall.lookAt(grp.position.x + Math.cos(fa) * d.r * 3, -70, Math.sin(fa) * d.r * 3);
      grp.add(fall);
      grp.position.set(d.x, d.y, d.z);
      this.scene.add(grp);
      d.group = grp; d.phase = rnd() * 6;
      this.islands.push(d);
    }
  }

  // Altitude du sol des îles (pour se poser dessus)
  islandGround(x, z, y) {
    for (const d of this.islands) {
      const gy = d.group.position.y;
      if (Math.hypot(x - d.x, z - d.z) < d.r * 0.97 && y > gy - 4) return gy;
    }
    return -Infinity;
  }

  // ---------- Volcan : lac de lave et fumées ----------
  buildVolcano() {
    const V = REGIONS.volcano;
    const y = G.terrain.heightAt(V.x, V.z) + 3;
    this.lava = new THREE.Mesh(new THREE.CircleGeometry(46, 32).rotateX(-Math.PI / 2), new THREE.ShaderMaterial({
      uniforms: { uTime: G.uniforms.time },
      vertexShader: 'varying vec2 vP; void main(){ vP = position.xz; gl_Position = projectionMatrix*modelViewMatrix*vec4(position,1.0); }',
      fragmentShader: `uniform float uTime; varying vec2 vP; float h(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }
        float n(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f); return mix(mix(h(i),h(i+vec2(1,0)),f.x), mix(h(i+vec2(0,1)),h(i+vec2(1,1)),f.x), f.y); }
        void main(){ float v = n(vP*0.12 + uTime*0.1) * 0.6 + n(vP*0.4 - uTime*0.2)*0.4; float c = smoothstep(0.35,0.75,v);
          vec3 col = mix(vec3(0.35,0.03,0.0), vec3(1.6,0.55,0.08), c) + vec3(1.5,1.0,0.3)*smoothstep(0.82,0.95,v);
          gl_FragColor = vec4(col, 1.0); }`,
    }));
    this.lava.position.set(V.x, y, V.z);
    this.scene.add(this.lava);
    this.volcanoSmokeTimer = 0;
  }

  // ---------- Pluie ----------
  buildRain() {
    const n = 2600;
    const pos = new Float32Array(n * 6);
    this.rainSeed = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) { this.rainSeed[i * 3] = Math.random() * 120 - 60; this.rainSeed[i * 3 + 1] = Math.random() * 70; this.rainSeed[i * 3 + 2] = Math.random() * 120 - 60; }
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    this.rain = new THREE.LineSegments(g, new THREE.LineBasicMaterial({ color: 0xaabbd0, transparent: true, opacity: 0.0, depthWrite: false }));
    this.rain.frustumCulled = false;
    this.scene.add(this.rain);
  }

  updateRain(dt) {
    const a = this.weatherT;
    this.rain.visible = a > 0.05;
    if (!this.rain.visible) return;
    this.rain.material.opacity = 0.35 * a;
    const cam = G.camera.position, p = this.rain.geometry.attributes.position.array, s = this.rainSeed;
    const t = G.time;
    for (let i = 0; i < s.length / 3; i++) {
      const x = cam.x + ((s[i * 3] - cam.x * 0.0) % 120 + 180) % 120 - 60;
      const z = cam.z + ((s[i * 3 + 2]) % 120 + 180) % 120 - 60;
      const y = cam.y + 40 - ((s[i * 3 + 1] + t * 38) % 70);
      p[i * 6] = x; p[i * 6 + 1] = y; p[i * 6 + 2] = z;
      p[i * 6 + 3] = x - 0.15; p[i * 6 + 4] = y - 1.6; p[i * 6 + 5] = z - 0.05;
    }
    this.rain.geometry.attributes.position.needsUpdate = true;
  }

  setWeather(w) {
    if (this.weather === w) return;
    this.weather = w;
    emit('journal', { icon: w === 'storm' ? '⛈' : w === 'cloudy' ? '☁' : '☀', text: w === 'storm' ? 'Un orage gronde sur le monde.' : w === 'cloudy' ? 'Le ciel se couvre.' : 'Le ciel s\'éclaircit.' });
  }

  hour() { return (this.dayTime * 24) % 24; }
  isNight() { const h = this.hour(); return h < 5.5 || h > 20.5; }

  update(dt, force = false) {
    const dayLen = settings.dayLength * 60;
    if (!G.freezeTime) this.dayTime += dt / dayLen;
    if (this.dayTime >= 1) { this.dayTime -= 1; this.day++; emit('newDay', this.day); }
    // météo
    this.weatherTimer -= dt;
    if (this.weatherTimer <= 0) {
      const r = Math.random();
      this.setWeather(r < 0.25 ? 'storm' : r < 0.55 ? 'cloudy' : 'clear');
      this.weatherTimer = this.weather === 'storm' ? rand(100, 180) : rand(220, 420);
    }
    const target = this.weather === 'storm' ? 1 : 0;
    this.weatherT = lerp(this.weatherT, target, 1 - Math.exp(-0.15 * dt));
    const cover = this.weather === 'clear' ? 0.0 : this.weather === 'cloudy' ? 0.7 : 1.05;
    this.cloudMat.uniforms.uCover.value = lerp(this.cloudMat.uniforms.uCover.value, cover, 1 - Math.exp(-0.2 * dt));

    // soleil (mesuré sur les ciels photographiques)
    const focus = G.god ? G.god.pos : new THREE.Vector3();
    this.sky.update(this.dayTime, this.weatherT, this.flash, G.camera, this.scene);
    this.sunDir.copy(this.sky.sunDir);
    const elev = this.sunDir.y;
    const day = this.sky.day, night = this.sky.night, dusk = this.sky.dusk;
    this.dayF = day; this.nightF = night;
    const lightDir = elev > -0.04 ? this.sunDir : this.sky.moonDir;
    this.sun.position.copy(focus).addScaledVector(lightDir, 400);
    this.sun.target.position.copy(focus);
    const storm = 1 - this.weatherT * 0.7;
    if (elev > -0.04) {
      this.sun.color.copy(this.sky.sunColor).lerp(C(1, 0.62, 0.36), dusk * 0.5);
      this.sun.intensity = (smooth(-0.04, 0.12, elev) * 3.4) * storm;
    } else { this.sun.color.setRGB(0.55, 0.66, 1.0); this.sun.intensity = 0.35 * night * storm; }
    this.hemi.intensity = 0.12 + day * 0.18;
    this.hemi.color.copy(this.sky.fog);
    this.hemi.groundColor.copy(C(0.3, 0.27, 0.2).lerp(C(0.04, 0.05, 0.07), night));

    // brouillard
    this.fogColor.copy(this.sky.fog).lerp(C(0.3, 0.32, 0.36), this.weatherT * (1 - night * 0.7));
    if (this.flash > 0) this.fogColor.lerp(C(0.9, 0.92, 1), this.flash * 0.6);
    this.scene.fog.color.copy(this.fogColor);
    const far = Q().far;
    const alt = G.camera.position.y;
    this.scene.fog.near = lerp(160, 40, this.weatherT) + alt * 0.4;
    this.scene.fog.far = lerp(far * 1.25, far * 0.45, this.weatherT) + alt * 1.5;
    this.renderer.toneMappingExposure = 1.0 + this.flash * 1.5;

    // nuages
    const cm = this.cloudMat.uniforms;
    cm.uLit.value.copy(C(1, 1, 1).lerp(C(1, 0.72, 0.55), dusk).lerp(C(0.13, 0.15, 0.24), night).lerp(C(0.5, 0.52, 0.56), this.weatherT));
    cm.uShadow.value.copy(cm.uLit.value).multiplyScalar(0.62);
    cm.uFogCol.value.copy(this.fogColor);
    this.updateClouds(dt);

    // eau
    for (const m of [this.water.material, this.river.material]) {
      m.uniforms.uTime.value = G.uniforms.time.value;
      m.uniforms.uSunDir.value.copy(lightDir);
      m.uniforms.uSunCol.value.copy(this.sun.color).multiplyScalar(this.sun.intensity * 0.5);
      m.uniforms.uSky.value.copy(this.fogColor).lerp(C(0.45, 0.62, 0.85), day * 0.4);
      m.uniforms.uNight.value = night;
    }

    // îles flottantes : lente oscillation
    for (const d of this.islands) {
      d.group.position.y = d.y + Math.sin(G.time * 0.25 + d.phase) * 2.5;
      d.crystal.rotation.y += dt * 0.6;
    }

    // orage : éclairs naturels
    if (this.weatherT > 0.6) {
      this.lightningTimer -= dt;
      if (this.lightningTimer <= 0) {
        this.lightningTimer = rand(4, 14);
        const a = Math.random() * Math.PI * 2, r = rand(80, 600);
        const x = focus.x + Math.cos(a) * r, z = focus.z + Math.sin(a) * r;
        if (G.terrain.inWorld(x, z, 20)) emit('naturalLightning', { x, z });
      }
    }
    this.flash = Math.max(0, this.flash - dt * 3);
    this.updateRain(dt);

    // fumée du volcan
    this.volcanoSmokeTimer -= dt;
    if (this.volcanoSmokeTimer <= 0 && G.fx) {
      this.volcanoSmokeTimer = 0.25;
      const V = REGIONS.volcano;
      G.fx.smoke(new THREE.Vector3(V.x + rand(-25, 25), this.lava.position.y + 4, V.z + rand(-25, 25)), 1, 20, 0.25);
    }

    this.scene.environmentIntensity = lerp(0.35, 1.0, day);
  }
}
