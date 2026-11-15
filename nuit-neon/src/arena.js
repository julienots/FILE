// Arène du mode Foot Turbo : terrain fermé, deux buts, ballon.
import * as THREE from 'three';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { canvasTex, captureEnv, skyDome } from './world.js';

export const ARENA = { hx: 42, hz: 62, wallH: 16, ceil: 24, goalW: 10, goalH: 7.5, goalD: 6, ballR: 1.7 };
export const TEAM_COL = [0x19b8ff, 0xff6a1f]; // bleus (attaquent vers -z), orange (attaquent vers +z)

function box(w, h, d, x, y, z) { const g = new THREE.BoxGeometry(w, h, d); g.translate(x, y, z); return g; }

export function buildArena(renderer, q) {
  const A = ARENA, group = new THREE.Group(), disposables = [];
  const add = (geo, mat, layer1) => { const m = new THREE.Mesh(geo, mat); if (layer1) m.layers.set(1); group.add(m); disposables.push(geo, mat); return m; };
  const th = { sky: [[0.01, 0.015, 0.06], [0.05, 0.05, 0.22], [0.20, 0.10, 0.40]], fog: 0x0a0a22, dens: 0.0022 };
  const fog = new THREE.FogExp2(th.fog, th.dens);
  const sky = skyDome(th); group.add(sky); disposables.push(sky.geometry, sky.material);
  const cB = '#19b8ff', cO = '#ff6a1f';

  // pelouse synthétique sombre et brillante, avec ses lignes
  const W = 512, Hh = 756;
  const floorTex = canvasTex(W, Hh, (g) => {
    for (let i = 0; i < 12; i++) { g.fillStyle = i % 2 ? '#0c1626' : '#0a1220'; g.fillRect(0, i * Hh / 12, W, Hh / 12); }
    const gb = g.createLinearGradient(0, 0, 0, Hh); gb.addColorStop(0, 'rgba(25,184,255,0.20)'); gb.addColorStop(0.5, 'rgba(0,0,0,0)'); gb.addColorStop(1, 'rgba(255,106,31,0.20)');
    g.fillStyle = gb; g.fillRect(0, 0, W, Hh);
    g.strokeStyle = '#e8f0ff'; g.lineWidth = 5; g.strokeRect(8, 8, W - 16, Hh - 16);
    g.beginPath(); g.moveTo(8, Hh / 2); g.lineTo(W - 8, Hh / 2); g.stroke();
    g.beginPath(); g.arc(W / 2, Hh / 2, 70, 0, 7); g.stroke();
    const gw = A.goalW / A.hx * W / 2;
    g.strokeStyle = cB; g.strokeRect(W / 2 - gw * 1.6, 8, gw * 3.2, 90); g.strokeStyle = cO; g.strokeRect(W / 2 - gw * 1.6, Hh - 98, gw * 3.2, 90);
  }, { wrapS: THREE.ClampToEdgeWrapping, wrapT: THREE.ClampToEdgeWrapping });
  const floor = add(new THREE.PlaneGeometry(A.hx * 2, A.hz * 2), new THREE.MeshStandardMaterial({ map: floorTex, roughness: 0.16, metalness: 0.35, envMapIntensity: 1.3, emissive: 0xffffff, emissiveMap: floorTex, emissiveIntensity: 0.22 }));
  floor.rotation.x = -Math.PI / 2;
  const out = add(new THREE.PlaneGeometry(900, 900, 24, 24), new THREE.MeshStandardMaterial({ color: 0x05060c, roughness: 0.8, polygonOffset: true, polygonOffsetFactor: 2, polygonOffsetUnits: 8 }));
  out.rotation.x = -Math.PI / 2; out.position.y = -0.3; out.renderOrder = -1;
  disposables.push(floorTex);

  // parois vitrées quadrillées
  const gridTex = canvasTex(64, 64, (g) => { g.clearRect(0, 0, 64, 64); g.strokeStyle = 'rgba(160,200,255,0.9)'; g.lineWidth = 2; g.strokeRect(1, 1, 62, 62); });
  const wallMat = (rx, ry) => { const t = gridTex.clone(); t.needsUpdate = true; t.repeat.set(rx, ry); disposables.push(t); return new THREE.MeshBasicMaterial({ map: t, transparent: true, opacity: 0.32, side: THREE.DoubleSide, depthWrite: false, color: 0x9fc0ff }); };
  for (const s of [-1, 1]) {
    const w = add(new THREE.PlaneGeometry(A.hz * 2, A.wallH), wallMat(31, 4), true); w.rotation.y = Math.PI / 2; w.position.set(s * A.hx, A.wallH / 2, 0);
    // fond : deux pans de chaque côté du but + un pan au-dessus
    const side = (A.hx - A.goalW) / 2 + A.goalW;
    for (const sx of [-1, 1]) { const e = add(new THREE.PlaneGeometry(A.hx - A.goalW, A.wallH), wallMat(8, 4), true); e.position.set(sx * side, A.wallH / 2, s * A.hz); }
    const top = add(new THREE.PlaneGeometry(A.goalW * 2, A.wallH - A.goalH), wallMat(5, 2), true); top.position.set(0, (A.wallH + A.goalH) / 2, s * A.hz);
  }
  disposables.push(gridTex);

  // buts lumineux, arêtes néon, tribunes
  const neon = [[], []], dark = [];
  for (const t of [0, 1]) {
    const s = t === 0 ? 1 : -1; // le but des bleus est en +z (ils attaquent vers -z)
    const z0 = s * A.hz, z1 = s * (A.hz + A.goalD);
    for (const sx of [-1, 1]) neon[t].push(box(0.4, A.goalH, 0.4, sx * A.goalW, A.goalH / 2, z0));
    neon[t].push(box(A.goalW * 2 + 0.4, 0.4, 0.4, 0, A.goalH, z0));
    neon[t].push(box(A.goalW * 2, 0.15, 0.15, 0, A.goalH, z1), box(A.goalW * 2, 0.15, 0.15, 0, 0.1, z1));
    dark.push(box(A.goalW * 2 + 1, A.goalH + 0.5, 0.4, 0, A.goalH / 2, z1 + s * 0.3), box(0.4, A.goalH, A.goalD, -A.goalW - 0.2, A.goalH / 2, (z0 + z1) / 2), box(0.4, A.goalH, A.goalD, A.goalW + 0.2, A.goalH / 2, (z0 + z1) / 2), box(A.goalW * 2 + 1, 0.4, A.goalD, 0, A.goalH + 0.2, (z0 + z1) / 2));
    // bandes au sol et en haut des parois, de la couleur de l'équipe
    for (const sx of [-1, 1]) neon[t].push(box(0.3, 0.3, A.hz, sx * A.hx, A.wallH, s * A.hz / 2), box(0.2, 0.12, A.hz, sx * (A.hx - 0.15), 0.06, s * A.hz / 2));
    neon[t].push(box(A.hx * 2, 0.3, 0.3, 0, A.wallH, z0));
  }
  neon.forEach((gs, t) => add(mergeGeometries(gs), new THREE.MeshBasicMaterial({ color: new THREE.Color(TEAM_COL[t]).multiplyScalar(3) })));
  const crowd = canvasTex(256, 64, (g) => {
    g.fillStyle = '#05060c'; g.fillRect(0, 0, 256, 64);
    for (let i = 0; i < 1500; i++) { g.fillStyle = ['#19b8ff', '#ff6a1f', '#ffffff', '#ffd23f', '#3a4466'][Math.floor(Math.random() * 5)]; g.globalAlpha = 0.25 + Math.random() * 0.6; g.fillRect(Math.random() * 256, Math.random() * 64, 2, 3); }
  });
  crowd.repeat.set(6, 1);
  const stand = [];
  for (const s of [-1, 1]) {
    for (let r = 0; r < 5; r++) stand.push(box(4, 2, A.hz * 2 + 30, s * (A.hx + 5 + r * 4), 3 + r * 2.6, 0));
    for (let r = 0; r < 4; r++) stand.push(box(A.hx * 2 + 46, 2, 4, 0, 9 + r * 2.6, s * (A.hz + A.goalD + 6 + r * 4)));
  }
  add(mergeGeometries(stand), new THREE.MeshBasicMaterial({ map: crowd, color: new THREE.Color(1.1, 1.1, 1.1) }));
  disposables.push(crowd);
  // mâts d'éclairage
  const lamps = [];
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) { dark.push(box(1, 34, 1, sx * (A.hx + 30), 17, sz * (A.hz + 22))); lamps.push(box(7, 3.5, 0.6, sx * (A.hx + 29.5), 34, sz * (A.hz + 21.5))); }
  add(mergeGeometries(dark), new THREE.MeshStandardMaterial({ color: 0x12141e, metalness: 0.8, roughness: 0.4 }));
  add(mergeGeometries(lamps), new THREE.MeshBasicMaterial({ color: new THREE.Color(3, 3, 3.4) }));

  group.add(new THREE.HemisphereLight(0x9fb4ff, 0x1a1030, 1.5));
  const key = new THREE.DirectionalLight(0xffffff, 2.0); key.position.set(30, 60, 20); group.add(key);
  const rim = new THREE.DirectionalLight(0x8090ff, 1.0); rim.position.set(-30, 40, -30); group.add(rim);

  // ballon
  const ballTex = canvasTex(256, 128, (g) => {
    g.fillStyle = '#d8dce8'; g.fillRect(0, 0, 256, 128); g.strokeStyle = '#20242e'; g.lineWidth = 5;
    for (let y = 0; y < 4; y++) for (let x = 0; x < 8; x++) { const cx = x * 32 + (y % 2 ? 16 : 0), cy = y * 32 + 16; g.beginPath(); for (let k = 0; k < 6; k++) { const a = k / 6 * 6.283; k ? g.lineTo(cx + Math.cos(a) * 15, cy + Math.sin(a) * 15) : g.moveTo(cx + Math.cos(a) * 15, cy + Math.sin(a) * 15); } g.closePath(); g.stroke(); }
  });
  const ball = add(new THREE.SphereGeometry(A.ballR, 28, 18), new THREE.MeshStandardMaterial({ map: ballTex, roughness: 0.3, metalness: 0.5, envMapIntensity: 1.4, emissive: 0x334466, emissiveIntensity: 0.6 }));
  disposables.push(ballTex);
  const shTex = canvasTex(64, 64, (g) => { const gr = g.createRadialGradient(32, 32, 2, 32, 32, 32); gr.addColorStop(0, 'rgba(0,0,0,0.85)'); gr.addColorStop(1, 'rgba(0,0,0,0)'); g.fillStyle = gr; g.fillRect(0, 0, 64, 64); });
  const ballShadow = add(new THREE.PlaneGeometry(1, 1), new THREE.MeshBasicMaterial({ map: shTex, transparent: true, depthWrite: false }), true);
  ballShadow.rotation.x = -Math.PI / 2; ballShadow.position.y = 0.03; ballShadow.renderOrder = 2;
  disposables.push(shTex);

  // pastilles de boost
  const pads = [];
  const padGeo = new THREE.CylinderGeometry(1.5, 1.5, 0.12, 20), padMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(3.2, 2.4, 0.4) });
  disposables.push(padGeo, padMat);
  for (const [x, z] of [[-34, -50], [34, -50], [-34, 50], [34, 50], [-36, 0], [36, 0], [0, -30], [0, 30], [-18, -18], [18, -18], [-18, 18], [18, 18]]) {
    const m = new THREE.Mesh(padGeo, padMat); m.position.set(x, 0.07, z); group.add(m); pads.push({ x, z, t: 0, mesh: m });
  }

  let env = null;
  const capture = (scene) => { env = captureEnv(renderer, scene, new THREE.Vector3(0, 3, 0), q.env, null, new THREE.Color(0x0a1220)); return env; };
  const dispose = () => { for (const d of disposables) d.dispose && d.dispose(); if (env) env.dispose(); };
  return { arena: true, group, fog, road: null, rain: null, updateRain() {}, update() {}, captureEnv: capture, dispose, ball, ballShadow, pads, day: false, theme: th };
}
