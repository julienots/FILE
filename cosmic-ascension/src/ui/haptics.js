import { State } from '../core/State.js';
// Retour haptique: Capacitor Haptics si présent (APK), sinon navigator.vibrate. Respecte le réglage.
export function buzz(ms = 12) {
  if (!State.data.settings.haptics) return;
  const H = globalThis.Capacitor?.Plugins?.Haptics;
  try { if (H) H.impact({ style: ms > 30 ? 'HEAVY' : ms > 15 ? 'MEDIUM' : 'LIGHT' }); else navigator.vibrate?.(ms); } catch { /* ignore */ }
}
