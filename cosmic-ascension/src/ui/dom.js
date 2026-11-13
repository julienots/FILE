// Mini-helpers DOM (B). h('div.cls#id', {attrs/on*}, ...enfants)
export function h(sel, props = {}, ...kids) {
  const [tag, ...rest] = sel.split(/(?=[.#])/), el = document.createElement(tag || 'div');
  for (const r of rest) r[0] === '.' ? el.classList.add(r.slice(1)) : (el.id = r.slice(1));
  for (const k in props) {
    const v = props[k];
    if (k.startsWith('on')) el.addEventListener(k.slice(2).toLowerCase(), v);
    else if (k === 'style' && typeof v === 'object') Object.assign(el.style, v);
    else if (k === 'html') el.innerHTML = v; else if (k === 'text') el.textContent = v;
    else if (v !== false && v != null) el.setAttribute(k, v === true ? '' : v);
  }
  for (const c of kids.flat()) if (c != null && c !== false) el.append(c.nodeType ? c : String(c));
  return el;
}
export const fmtTime = (s) => { const hh = Math.floor(s / 3600), mm = Math.floor(s / 60) % 60; return hh ? `${hh} h ${String(mm).padStart(2, '0')}` : `${mm} min`; };
