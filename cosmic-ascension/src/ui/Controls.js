import { Input } from '../core/Input.js';
import { bus } from '../core/EventBus.js';
// Contrôles tactiles paysage: joystick virtuel flottant (gauche), boutons (droite), glisser droite = caméra.
// OPUS: possède ce module (skin, haptique, réglages gaucher/droitier, boutons contextuels).
const css = `
.joy{position:absolute;left:4vmin;bottom:5vmin;width:30vmin;height:30vmin;border-radius:50%;background:rgba(255,255,255,.08);border:2px solid rgba(255,255,255,.25);backdrop-filter:blur(4px)}
.joy i{position:absolute;left:50%;top:50%;width:13vmin;height:13vmin;margin:-6.5vmin;border-radius:50%;background:radial-gradient(circle at 35% 30%,#fff,#7fd0ff);box-shadow:0 0 18px #7fd0ff99}
.btn{position:absolute;width:15vmin;height:15vmin;border-radius:50%;border:2px solid rgba(255,255,255,.5);color:#fff;font-size:5.5vmin;display:flex;align-items:center;justify-content:center;
 background:rgba(120,90,255,.35);backdrop-filter:blur(4px);box-shadow:0 0 14px rgba(150,120,255,.5)}
.btn.on{background:rgba(255,200,90,.6)}
.hint{position:absolute;top:2vmin;left:50%;transform:translateX(-50%);color:#fff;font-size:3.4vmin;text-shadow:0 2px 8px #000;text-align:center;pointer-events:none!important}
`;
export function mountControls(root) {
  const st = document.createElement('style'); st.textContent = css; document.head.appendChild(st);
  const joy = document.createElement('div'); joy.className = 'joy'; const knob = document.createElement('i'); joy.appendChild(knob); root.appendChild(joy);
  const hint = document.createElement('div'); hint.className = 'hint'; root.appendChild(hint);
  Input._touch = {};
  const mk = (key, label, right, bottom) => {
    const b = document.createElement('div'); b.className = 'btn'; b.textContent = label; b.style.right = right; b.style.bottom = bottom; root.appendChild(b);
    const set = v => { Input.buttons[key] = v; Input._touch[key] = v; b.classList.toggle('on', v); };
    b.addEventListener('pointerdown', e => { e.preventDefault(); b.setPointerCapture(e.pointerId); set(true); });
    b.addEventListener('pointerup', () => set(false)); b.addEventListener('pointercancel', () => set(false));
    return b;
  };
  mk('action', '⏎', '4vmin', '5vmin'); mk('jump', '⤒', '21vmin', '3vmin'); mk('boost', '≫', '3vmin', '22vmin'); mk('special', '✦', '20vmin', '19vmin'); mk('attack', '✸', '37vmin', '4vmin');
  let id = null, cx = 0, cy = 0; const R = () => joy.getBoundingClientRect().width / 2;
  const upd = e => { const r = joy.getBoundingClientRect(); cx = r.left + r.width / 2; cy = r.top + r.height / 2;
    let dx = (e.clientX - cx) / R(), dy = (e.clientY - cy) / R(); const l = Math.hypot(dx, dy); if (l > 1) { dx /= l; dy /= l; }
    Input.move.x = dx; Input.move.y = -dy; knob.style.transform = `translate(${dx * R() * .6}px,${dy * R() * .6}px)`; };
  joy.addEventListener('pointerdown', e => { id = e.pointerId; joy.setPointerCapture(id); upd(e); });
  joy.addEventListener('pointermove', e => { if (e.pointerId === id) upd(e); });
  const end = e => { if (e.pointerId === id) { id = null; Input.move.x = Input.move.y = 0; knob.style.transform = ''; } };
  joy.addEventListener('pointerup', end); joy.addEventListener('pointercancel', end);
  // glisser dans la moitié droite (hors boutons) = caméra
  let lx = null, ly = null; const cam = document.createElement('div'); cam.style.cssText = 'position:absolute;right:0;top:0;width:55%;height:70%'; root.prepend(cam);
  cam.addEventListener('pointerdown', e => { lx = e.clientX; ly = e.clientY; cam.setPointerCapture(e.pointerId); });
  cam.addEventListener('pointermove', e => { if (lx != null) { Input.look.x += e.clientX - lx; Input.look.y += e.clientY - ly; lx = e.clientX; ly = e.clientY; } });
  cam.addEventListener('pointerup', () => lx = null);
  bus.on('space:near', ({ planet, canLand }) => { hint.textContent = canLand ? `⏎ Atterrir sur ${planet.name}` : `${planet.name}`; });
  bus.on('npc:near', ({ npcId, name }) => { hint.textContent = npcId ? `⏎ Parler à ${name}` : '⏎ décoller · ✸ tirer · ✦ onde de choc'; });
  bus.on('scene:entered', ({ id, planet }) => { if (id === 'planet') hint.textContent = `${planet.name} — ⏎ décoller · ✸ tirer · ✦ onde de choc`; });
}
