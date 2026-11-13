// Textes d'interface (B). t('clé', {var}) → chaîne. Les noms propres du monde vivent dans src/data/.
const FR = {
  'title.play': 'Jouer', 'title.continue': 'Continuer', 'title.new': 'Nouvelle partie', 'title.settings': 'Réglages',
  'title.tagline': 'De grain d’étoile à divinité', 'title.confirmNew': 'Effacer la sauvegarde et recommencer ?',
  'pause.title': 'Pause', 'pause.resume': 'Reprendre', 'pause.settings': 'Réglages', 'pause.map': 'Carte galactique', 'pause.codex': 'Codex',
  'pause.quit': 'Écran titre', 'pause.quests': 'Quêtes', 'pause.inventory': 'Inventaire', 'soon': 'Bientôt',
  'set.title': 'Réglages', 'set.quality': 'Qualité graphique', 'set.q.auto': 'Auto', 'set.q.0': 'Bas', 'set.q.1': 'Moyen', 'set.q.2': 'Haut',
  'set.music': 'Musique', 'set.sfx': 'Effets', 'set.lefty': 'Mode gaucher', 'set.haptics': 'Vibrations', 'set.reset': 'Effacer la sauvegarde',
  'set.resetConfirm': 'Vraiment tout effacer ? Ta progression sera perdue.', 'back': 'Retour', 'yes': 'Oui', 'no': 'Non',
  'hud.stage': 'Stade {n}', 'hud.max': 'Forme ultime', 'hud.xpTo': '{need} XP avant {name}',
  'hud.discovered': 'Planètes découvertes', 'hud.objective': 'Objectif',
  'act.land': 'Atterrir', 'act.takeoff': 'Décoller', 'act.talk': 'Parler', 'act.use': 'Utiliser',
  'near.land': 'Appuie sur ⏎ pour atterrir sur {name}', 'near.far': '{name} · {dist} km',
  'evo.title': 'ÉVOLUTION !', 'evo.ability': 'Nouvelle capacité : {name}',
  'xp.discover': 'Découverte', 'xp.landing': 'Premier pas', 'xp.pickup': 'Collecte', 'xp.kill': 'Victoire', 'xp.quest': 'Quête',
  'obj.m1': 'Explore Pépin, puis décolle et visite d’autres planètes',
  'stats.time': 'Temps de jeu', 'stats.planets': 'Planètes', 'stats.kills': 'Ennemis vaincus', 'stats.stage': 'Forme',
};
const LANGS = { fr: FR };
let cur = FR;
export const setLang = (l) => { cur = LANGS[l] || FR; };
export function t(key, vars) {
  let s = cur[key] ?? FR[key] ?? key;
  if (vars) for (const k in vars) s = s.replaceAll('{' + k + '}', vars[k]);
  return s;
}
