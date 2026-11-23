// Écran de lancement animé « SuperEssence. STUDIO » :
// un cube jaune tombe, rebondit, tourne puis se transforme en point final du logo
// pendant que les lettres apparaissent une à une.
import { el } from './dom.js';

export function playIntro() {
  return new Promise((resolve) => {
    const word = [...'Super'].map((c) => `<span class="se-l w">${c}</span>`).join('') + [...'Essence'].map((c) => `<span class="se-l y">${c}</span>`).join('');
    const root = el(`<div id="intro">
      <div class="se-flash"></div>
      <div class="se-stage">${word}<span class="se-dotslot"></span>
        <div class="se-cube"><div class="f f1"></div><div class="f f2"></div><div class="f f3"></div><div class="f f4"></div><div class="f f5"></div><div class="f f6"></div></div>
      </div>
      <div class="se-studio">STUDIO</div>
      <div class="se-line"></div>
      <div class="se-presents">PRÉSENTE</div>
      <div class="se-skip">TOUCHER POUR PASSER</div>
    </div>`);
    document.body.appendChild(root);
    const letters = [...root.querySelectorAll('.se-l')];
    const cube = root.querySelector('.se-cube');
    const slot = root.querySelector('.se-dotslot');
    const stage = root.querySelector('.se-stage');
    let done = false;
    const finish = () => {
      if (done) return; done = true;
      root.classList.add('out');
      setTimeout(() => { root.remove(); resolve(); }, 600);
    };
    root.addEventListener('pointerdown', finish);

    // Animation du cube avec Web Animations API.
    const sr = stage.getBoundingClientRect();
    const dr = slot.getBoundingClientRect();
    const toX = dr.left + dr.width / 2 - (sr.left + sr.width / 2);
    const toY = dr.top + dr.height / 2 - (sr.top + sr.height / 2);
    const fs = parseFloat(getComputedStyle(stage).fontSize);
    const startY = -window.innerHeight * 0.55;
    const a1 = cube.animate([
      { transform: `translate(0px, ${startY}px) rotateX(-25deg) rotateY(35deg) scale(1)` },
      { transform: 'translate(0px, 0px) rotateX(-25deg) rotateY(125deg) scale(1)', offset: 0.45, easing: 'ease-out' },
      { transform: `translate(0px, ${-fs * 0.6}px) rotateX(-25deg) rotateY(215deg) scale(1)`, offset: 0.62, easing: 'ease-in' },
      { transform: 'translate(0px, 0px) rotateX(-25deg) rotateY(305deg) scale(1)', offset: 0.78, easing: 'ease-out' },
      { transform: `translate(0px, ${-fs * 0.18}px) rotateX(-25deg) rotateY(350deg) scale(1)`, offset: 0.88 },
      { transform: 'translate(0px, 0px) rotateX(-25deg) rotateY(405deg) scale(1)' },
    ], { duration: 1150, easing: 'ease-in', fill: 'forwards' });
    a1.onfinish = () => {
      if (done) return;
      root.querySelector('.se-flash').classList.add('on');
      // le cube glisse vers la place du point en tournant et en s'arrondissant
      cube.animate([
        { transform: 'translate(0px, 0px) rotateX(-25deg) rotateY(405deg) scale(1)' },
        { transform: `translate(${toX}px, ${toY - fs * 0.5}px) rotateX(-10deg) rotateY(675deg) scale(0.62)`, offset: 0.6 },
        { transform: `translate(${toX}px, ${toY}px) rotateX(0deg) rotateY(720deg) scale(0.46)` },
      ], { duration: 900, easing: 'cubic-bezier(.4,0,.2,1)', fill: 'forwards' });
      cube.querySelectorAll('.f').forEach((f) => f.animate([{ borderRadius: '0.08em' }, { borderRadius: '50%' }], { duration: 900, delay: 300, fill: 'forwards', easing: 'ease-in-out' }));
      letters.forEach((l, i) => setTimeout(() => l.classList.add('on'), 60 + i * 62));
      setTimeout(() => { root.querySelector('.se-studio').classList.add('on'); root.querySelector('.se-line').classList.add('on'); }, 1100);
      setTimeout(() => root.querySelector('.se-presents').classList.add('on'), 1750);
      setTimeout(() => stage.animate([{ transform: 'scale(1)' }, { transform: 'scale(1.06)' }, { transform: 'scale(1)' }], { duration: 500, easing: 'ease-in-out' }), 1000);
      setTimeout(finish, 3300);
    };
  });
}

// Logo de l'application : cube isométrique habité par un petit bonhomme.
export const APP_LOGO_SVG = `<svg viewBox="0 0 120 120" xmlns="http://www.w3.org/2000/svg" aria-label="Cubiverse">
  <defs><linearGradient id="lgT" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff1b8"/><stop offset="1" stop-color="#ffd64f"/></linearGradient>
  <linearGradient id="lgL" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffc928"/><stop offset="1" stop-color="#ff9f1a"/></linearGradient>
  <linearGradient id="lgR" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#8d7bff"/><stop offset="1" stop-color="#5b46e6"/></linearGradient></defs>
  <path d="M60 10 L104 34 L60 58 L16 34 Z" fill="url(#lgT)"/>
  <path d="M16 34 L60 58 L60 110 L16 86 Z" fill="url(#lgL)"/>
  <path d="M104 34 L60 58 L60 110 L104 86 Z" fill="url(#lgR)"/>
  <path d="M60 10 L104 34 L104 86 L60 110 L16 86 L16 34 Z" fill="none" stroke="#1d1b26" stroke-width="4" stroke-linejoin="round"/>
  <path d="M16 34 L60 58 L104 34 M60 58 L60 110" fill="none" stroke="#1d1b26" stroke-width="3" stroke-linejoin="round"/>
  <g transform="translate(37 62) skewY(28)" stroke="#1d1b26" stroke-width="3.2" stroke-linecap="round" fill="none">
    <circle cx="0" cy="0" r="5.6" fill="#fff" stroke-width="2.6"/>
    <path d="M0 6 L0 20 M0 20 L-5 30 M0 20 L5 30 M0 10 L-7 5 M0 10 L7 3"/>
  </g>
  <g transform="translate(83 58) skewY(-28)" stroke="#fff" stroke-width="3" stroke-linecap="round" fill="none" opacity=".95">
    <circle cx="0" cy="0" r="5" fill="#fff" stroke="none"/>
    <path d="M0 6 L0 19 M0 19 L-5 28 M0 19 L5 28 M0 9 L-7 14 M0 9 L7 14"/>
  </g>
</svg>`;
