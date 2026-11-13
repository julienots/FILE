// Bus d'événements global — SEUL canal de communication entre modules (voir docs/CONTRACTS.md).
const map = new Map();
export const bus = {
  on(evt, fn) { (map.get(evt) || map.set(evt, new Set()).get(evt)).add(fn); return () => bus.off(evt, fn); },
  off(evt, fn) { map.get(evt)?.delete(fn); },
  emit(evt, data) { const s = map.get(evt); if (s) for (const fn of [...s]) fn(data); },
};
