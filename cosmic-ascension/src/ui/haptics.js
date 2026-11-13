import { State } from '../core/State.js';
import { native } from './native.js';
// Retour haptique: Capacitor Haptics si présent (APK), sinon navigator.vibrate. Respecte le réglage.
export function buzz(ms = 12) {
  if (!State.data.settings.haptics) return;
  try { if (native.haptics) native.haptics(ms); else navigator.vibrate?.(ms); } catch { /* ignore */ }
}
