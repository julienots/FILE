// État d'entrée unifié: joystick tactile (écrit par ui/Controls.js) + clavier (debug bureau).
// Les scènes lisent UNIQUEMENT Input.move / Input.look / Input.buttons — jamais le DOM.
export const Input = {
  move: { x: 0, y: 0 },      // joystick gauche, [-1,1], y>0 = avant
  look: { x: 0, y: 0 },      // delta caméra (glisser sur la droite), remis à 0 par la scène
  buttons: { action: false, attack: false, boost: false, jump: false, special: false },
  pressed: {},               // front montant (1 frame), géré par endFrame()
  _prev: {},
  endFrame() {
    for (const k in this.buttons) this.pressed[k] = this.buttons[k] && !this._prev[k];
    for (const k in this.buttons) this._prev[k] = this.buttons[k];
    this.look.x = 0; this.look.y = 0;
  },
};
const keys = new Set();
const KEYMAP = { Space: 'jump', KeyE: 'action', KeyJ: 'attack', ShiftLeft: 'boost', KeyK: 'special' };
function kb() {
  let x = 0, y = 0;
  if (keys.has('KeyA') || keys.has('ArrowLeft')) x -= 1;
  if (keys.has('KeyD') || keys.has('ArrowRight')) x += 1;
  if (keys.has('KeyW') || keys.has('ArrowUp')) y += 1;
  if (keys.has('KeyS') || keys.has('ArrowDown')) y -= 1;
  if (x || y) { Input.move.x = x; Input.move.y = y; Input._kbMove = true; }
  else if (Input._kbMove) { Input.move.x = 0; Input.move.y = 0; Input._kbMove = false; }
  for (const c in KEYMAP) Input.buttons[KEYMAP[c]] = Input.buttons[KEYMAP[c]] && Input._touch?.[KEYMAP[c]] || keys.has(c);
}
addEventListener('keydown', e => { keys.add(e.code); kb(); });
addEventListener('keyup', e => { keys.delete(e.code); kb(); });
