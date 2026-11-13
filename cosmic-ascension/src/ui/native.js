import { Capacitor } from '@capacitor/core';
import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
// Pont natif Android (B9) via Capacitor. Sur le web, tout est no-op.
// Bouton retour → onBack, mise en arrière-plan → pause + sauvegarde, vibrations via Haptics.
export const isNative = Capacitor.isNativePlatform();
export const native = { haptics: null };
export async function initNative({ onBack } = {}) {
  if (!isNative) return;
  const [{ App }, { Haptics, ImpactStyle }, { StatusBar }] = await Promise.all([import('@capacitor/app'), import('@capacitor/haptics'), import('@capacitor/status-bar')]);
  StatusBar.hide().catch(() => {});
  native.haptics = (ms) => Haptics.impact({ style: ms > 30 ? ImpactStyle.Heavy : ms > 15 ? ImpactStyle.Medium : ImpactStyle.Light }).catch(() => {});
  App.addListener('backButton', () => onBack?.());
  App.addListener('appStateChange', ({ isActive }) => { if (!isActive) { State.save(); bus.emit('app:background'); } else { StatusBar.hide().catch(() => {}); bus.emit('app:foreground'); } });
}
