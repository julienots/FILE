import { initProgression } from './Progression.js';
import { initCombat } from './Combat.js';
import { initQuests } from './Quests.js';
import { initDialogue } from './Dialogue.js';
import { initStory } from './Story.js';
import { initBosses } from './Bosses.js';
// Point d'entrée des systèmes de B (logique de jeu). Appelé une fois par main.js après State.load().
export function initSystems(opts = {}) { initCombat(opts); initProgression(); initQuests(); initDialogue(); initStory(); initBosses(); }
