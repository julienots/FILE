// Déclencheurs narratifs (B3) — systems/Story.js. once = id unique (flag story_<id>), delay en ms.
export const STORY = [
  { on: 'game:start', once: 'intro', dialogue: 'intro', delay: 1500 },
  { on: 'planet:enter', match: { id: 'home' }, once: 'home', dialogue: 'comm_home', delay: 1800 },
  { on: 'planet:enter', match: { id: 'lab' }, once: 'lab', dialogue: 'comm_lab', delay: 1800 },
  { on: 'hero:evolved', match: { stage: 1 }, once: 'evo1', dialogue: 'comm_evolved', delay: 5000 },
  { on: 'hero:evolved', match: { stage: 3 }, once: 'evo3', dialogue: 'comm_stage3', delay: 5000 },
  { on: 'quest:started', match: { id: 'main_2_forge' }, once: 'act2', dialogue: 'comm_act2', delay: 2500 },
  { on: 'world:discovered', match: { id: 'volta' }, once: 'empire', dialogue: 'comm_empire' },
  { on: 'hero:died', once: 'death1', dialogue: 'comm_death', delay: 3200 },
  { on: 'quest:started', match: { id: 'secret_eden' }, once: 'eden', fx: { reveal: 'eden' } },
  { on: 'boss:defeated', match: { id: 'celestial_god' }, once: 'ending', dialogue: 'ending', delay: 4000 },
];
