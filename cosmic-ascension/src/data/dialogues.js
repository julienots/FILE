// Dialogues (B3). Format: systems/Dialogue.js. radio:true = transmission (pas besoin que le PNJ soit à l'écran).
// talked: id de PNJ compté comme « parlé » à la fin (utile pour les radios qui valident une étape `talk`).
export const DIALOGUES = {
  // ───────── Transmissions radio (Boulon) ─────────
  intro: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: '…kzzt… Spécimen Zéro, tu me reçois ? Ici Boulon, unité de maintenance du Laboratoire Ω. C’est moi qui ai déverrouillé ta cuve.', n: 'b' },
    b: { t: 'Tu as quitté le Laboratoire dans une capsule de secours. Leurs traqueurs sont déjà en route. Pose-toi sur Pépin, la planète devant toi : leurs capteurs y portent mal.', n: 'c' },
    c: { t: 'Commandes de vol : joystick pour l’assiette, ≫ pour la poussée. À portée d’atmosphère, ⏎ lance la descente.', c: [
      { l: 'Qui suis-je ?', n: 'who' }, { l: 'Compris, j’y vais !', n: 'go' }] },
    who: { t: 'Les archives disent « Spécimen Zéro ». Le reste du dossier est chiffré. Ce que je sais : rien de ce qu’ils ont cultivé n’avait survécu avant toi.', n: 'go' },
    go: { t: 'Absorbe la poussière stellaire en surface : ton organisme s’en nourrit. Et il va changer. Boulon, terminé.' } } },
  comm_home: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Contact au sol confirmé. Les orbes en suspension sont des fragments stellaires : absorbe-les.', n: 'b' },
    b: { t: 'Formes hostiles détectées à proximité. ⚔ pour frapper, ⤒ pour sauter. Si ton corps réagit étrangement, laisse faire : c’est une mutation.' } } },
  comm_evolved: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Pic d’énergie enregistré. Ta morphologie a changé, des membres se sont formés. Le Labo n’avait jamais obtenu ça, et ils l’ont forcément détecté.', n: 'b' },
    b: { t: 'Ils vont envoyer des drones. Mieux vaut frapper la source : le Laboratoire Ω est en orbite proche. Je te guide.' } } },
  comm_lab: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Le Laboratoire. Les drones patrouillent en boucle ; neutralises-en assez pour ouvrir un accès jusqu’au Dr Quill.' } } },
  comm_act2: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Un catalyseur forgé sur Braise-9. Les Braisiens sont les seuls à maîtriser ces alliages. Cap à l’est, vers le monde volcanique.' } } },
  comm_stage3: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Ces structures dorsales… identiques aux fresques des Titans. Tu n’es pas une expérience ratée, Spécimen. Tu es la seule qui ait abouti.' } } },
  comm_death: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Signes vitaux rétablis. Tu as perdu une partie de ta réserve stellaire. La prochaine fois, je ne garantis rien.' } } },
  comm_empire: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Attention: tu entres dans la Zone Interdite. L’Empire Écarlate tire à vue. Ici, les ennemis sont bien plus coriaces.' } } },

  // ───────── Pépin ─────────
  bolt_intro: { speaker: 'Boulon', portrait: '🤖', entry: [[{ step: ['side_drone_parts', 1] }, 'repair'], [{ canStart: 'side_drone_parts', done: 'main_1_awakening' }, 'parts']], nodes: {
    start: { t: 'Enfin face à face. Tu as déjà changé depuis la dernière image que j’ai de toi.', c: [
      { l: 'Merci de m’avoir libéré.', n: 'thx' }, { l: 'Pourquoi m’as-tu aidé ?', n: 'why' }] },
    thx: { t: 'Entre fugitifs, on ne compte pas. Je retourne sur ta capsule.' },
    why: { t: 'Trois cents ans à entretenir ta cuve. On finit par se demander qui est vraiment enfermé.' },
    parts: { t: 'Mes servomoteurs lâchent. Rapporte-moi de l’alliage, prélevé sur les drones ou les épaves, et j’améliorerai aussi ta capsule.', c: [
      { l: 'J’en trouverai.', fx: { quest: 'side_drone_parts' }, end: true }, { l: 'Plus tard.', end: true }] },
    repair: { t: 'Alliage reçu. Remplacement des servos… terminé. Opérationnel à 100 %.', fx: { take: [['scrapAlloy', 4]] } } } },
  mayor_intro: { speaker: 'Pipo, porte-parole', portrait: '🕯', entry: [[{ step: ['side_lost_pipion', 1] }, 'back'], [{ done: 'side_lost_pipion' }, 'after']], nodes: {
    start: { t: 'Un étranger. Ici, on ne voit plus personne depuis que le Labo a tout verrouillé. Je suis Pipo, je parle au nom de la colonie.', n: 'b' },
    b: { t: 'Un de nos jeunes, Pip, est parti sur les traces des créatures violettes. Il n’est pas revenu. Vous pourriez dégager la zone ?', c: [
      { l: 'Je m’en occupe !', fx: { quest: 'side_lost_pipion' }, end: true }, { l: 'Je dois d’abord m’occuper de moi.', end: true }] },
    back: { t: 'Pip est rentré. Il parle d’une lumière qui l’a protégé. Prenez ceci, c’est tout ce que la colonie peut offrir.' },
    after: { t: 'La colonie parle de vous à voix basse. Certains ont peur, d’autres espèrent.' } } },

  // ───────── Laboratoire Ω ─────────
  quill_lab: { speaker: 'Dr Quill', portrait: '🥽', nodes: {
    start: { t: 'Spécimen Zéro ! Tu… tu as grandi ! C’est fascinant. Terrifiant, mais fascinant.', c: [
      { l: 'Pourquoi m’avoir enfermé ?', n: 'why' }, { l: 'Rends-toi.', n: 'threat' }] },
    why: { t: 'Enfermé ? Nous t’avons CULTIVÉ. Tu n’as pas idée de ce que tu es. Les commanditaires paieraient une galaxie pour toi.', n: 'cat' },
    threat: { t: 'Me rendre ? Ha ! Je… d’accord, d’accord, ne me frappe pas !', n: 'cat' },
    cat: { t: 'Si tu veux comprendre ce que tu deviens, trouve le catalyseur. Les forgerons de Braise-9 savent le fabriquer. Maintenant laisse-moi, ils me surveillent aussi…' } } },

  // ───────── Braise-9 ─────────
  forga_intro: { speaker: 'Forga la Fondeuse', portrait: '🔥',
    entry: [[{ step: ['main_2_forge', 3] }, 'forge'], [{ step: ['side_tizon_lost', 2] }, 'tizonBack'], [{ step: ['side_cool_the_core', 1] }, 'cool'], [{ quest: 'main_2_forge' }, 'start'], [{ canStart: 'side_tizon_lost' }, 'tizon'], [{ canStart: 'side_cool_the_core' }, 'core']], nodes: {
    start: { t: 'Un petit être qui brille comme une braise neuve. Tu cherches le catalyseur ? Je vois ça dans tes yeux, gamin.', n: 'b' },
    b: { t: 'Je peux le forger. Il me faut trois Cœurs de Braise. On en trouve sur les slimes de magma… s’ils ne te font pas fondre avant.' },
    forge: { t: 'Trois cœurs, parfaits ! Recule… *CLANG* *CLANG* … Voilà. Ce catalyseur va réveiller ce qui dort en toi. Tiens-toi prêt.', fx: { take: [['emberCore', 3]], give: [['cosmicDust', 30]] } },
    tizon: { t: 'Mon fils, Tison, est parti vers les coulées. Avec les slimes de magma dans le secteur, il ne tiendra pas longtemps.', c: [
      { l: 'Je le retrouve.', fx: { quest: 'side_tizon_lost' }, end: true }, { l: 'Pas le temps.', end: true }] },
    tizonBack: { t: 'Tison… Merci, voyageur. La forge te sera toujours ouverte.' },
    core: { t: 'La grande forge chauffe trop. Si elle explose, adieu Braise-9. Il me faudrait du givre qui ne fond jamais…', c: [
      { l: 'Je connais un endroit: Glacio.', fx: { quest: 'side_cool_the_core' }, end: true }, { l: 'Bonne chance.', end: true }] },
    cool: { t: 'Du givre éternel ! Ça siffle, ça fume… ça tient ! Prends ce fer stellaire, il vient du cœur de la forge.', fx: { take: [['frostCrystal', 4]] } } } },
  tizon: { speaker: 'Tison', portrait: '🕯', nodes: {
    start: { t: 'Tu… tu brilles. C’est le Labo qui t’envoie ? Je voulais juste voir la grande coulée, et ils sont arrivés de partout.', n: 'b' },
    b: { t: 'Couvre-moi jusqu’à la forge. Ma mère doit déjà me chercher.' } } },

  // ───────── Cendrelune ─────────
  hermit: { speaker: 'L’Ermite Gris', portrait: '🌫', entry: [[{ step: ['side_hermit_memory', 1] }, 'echo']], nodes: {
    start: { t: '…Une lumière. Il y a longtemps qu’aucune lumière n’est venue ici. Mon peuple a brûlé dans la guerre des Titans. J’ai oublié jusqu’à son nom.', c: [
      { l: 'Je peux t’aider à te souvenir.', fx: { quest: 'side_hermit_memory' }, n: 'help' }, { l: 'Je suis désolé.', end: true }] },
    help: { t: 'Les pierres d’écho gardent les voix. On en trouve dans la Nécropole… et sur les spectres de cendre.' },
    echo: { t: '« …Cendrins… nous étions les Cendrins… » Je m’en souviens. Merci, petite lumière. Un jour, toi aussi tu auras besoin qu’on se souvienne de toi.', fx: { take: [['echoStone', 2]] } } } },

  // ───────── Ossable ─────────
  zaya: { speaker: 'Zaya des Caravanes', portrait: '🐪', entry: [[{ step: ['side_caravan_escort', 1] }, 'saved'], [{ step: ['side_titan_bones', 1] }, 'bones'], [{ canStart: 'side_titan_bones' }, 'giant']], nodes: {
    start: { t: 'Toi. Mes caravanes sont harcelées par des scarabées depuis trois cycles. Tu as l’air capable de te battre.', c: [
      { l: 'Je repousse les scarabées.', fx: { quest: 'side_caravan_escort' }, end: true }, { l: 'Une autre fois.', end: true }] },
    saved: { t: 'Les scarabées fuient ! Tiens, des perles des dunes. Et si tu repasses, j’ai une histoire de géant à te raconter.' },
    giant: { t: 'Ce squelette, là-bas ? Ce n’est pas un animal. C’est un Titan. Et il n’est pas mort… il dort. Trouve des reliques dans le sable, je saurai les lire.', c: [
      { l: 'Je fouille.', fx: { quest: 'side_titan_bones' }, end: true }, { l: 'Laisse-le dormir.', end: true }] },
    bones: { t: 'Ces glyphes… « Kaal ». Son nom est Kaal. Et ici: « Quand le dieu-enfant s’éveillera, les Titans se lèveront. » Le dieu-enfant… c’est toi ?' } } },

  // ───────── Glacio ─────────
  neve_intro: { speaker: 'Aïeule Névé', portrait: '❄', entry: [[{ step: ['side_frozen_ship', 1] }, 'ship'], [{ done: 'main_3_glacio', canStart: 'side_frozen_ship' }, 'shipAsk']], nodes: {
    start: { t: 'Les banquises ont chanté ton arrivée, enfant d’étoile. Elles chantent rarement. La dernière fois, c’était pour un Titan.', c: [
      { l: 'Que savez-vous des Titans ?', n: 'titans' }, { l: 'Pouvez-vous m’aider ?', n: 'help' }] },
    titans: { t: 'Qu’ils sont nés d’étoiles mourantes, et que le Laboratoire a été construit pour en fabriquer un nouveau. Un qui obéirait.', n: 'help' },
    help: { t: 'Les slimes de givre assiègent notre village. Chasse-les, et les Givrins se souviendront de toi.' },
    shipAsk: { t: 'Il y a un vaisseau pris dans la glace depuis mille ans. Ses cristaux brillent encore. Va voir, et dis-moi ce que tu y trouves.', c: [
      { l: 'J’y vais.', fx: { quest: 'side_frozen_ship' }, end: true }, { l: 'Plus tard.', end: true }] },
    ship: { t: 'Ces cristaux portent le sceau du Labo… Ils sont venus ici avant toi, il y a très longtemps. Prends ceci pour ta peine.' } } },

  // ───────── Aurorée ─────────
  oracle: { speaker: 'L’Oracle Prisme', portrait: '🔮', entry: [[{ canStart: 'side_prism_tuning', done: 'main_3_oracle' }, 'tune']], nodes: {
    start: { t: 'Je t’ai vu dans la lumière avant que tu naisses. Tu as dix visages. Le dernier contient une galaxie.', c: [
      { l: 'Que dois-je faire ?', n: 'do' }, { l: 'Dix visages ?', n: 'faces' }] },
    faces: { t: 'Chaque évolution t’arrache à ce que tu étais. Garde quelque chose de toi, ou tu deviendras ce que le Labo voulait.', n: 'do' },
    do: { t: 'Un empire rouge marche vers toi. Son empereur veut te porter comme une couronne. Deviens assez fort pour l’en empêcher.', fx: { flag: 'empire_revealed' } },
    tune: { t: 'Mes prismes sonnent faux depuis que les chauves-éclats y nichent. Chasse-les et rapporte de la sève lumineuse.', c: [
      { l: 'Avec plaisir.', fx: { quest: 'side_prism_tuning' }, end: true }, { l: 'Non.', end: true }] } } },

  // ───────── Sylva ─────────
  sylvana_intro: { speaker: 'Sylvana', portrait: '🌿', entry: [[{ step: ['side_lost_saplings', 1] }, 'saplings'], [{ done: 'main_3_sylva', canStart: 'side_lost_saplings' }, 'askSap']], nodes: {
    start: { t: 'La Mère souffre. Ses racines sont empoisonnées et ses bêtes-ronces deviennent folles. Toi… tu sens la lumière neuve.', c: [
      { l: 'Je vais la soigner.', n: 'yes' }, { l: 'Pourquoi moi ?', n: 'why' }] },
    why: { t: 'Parce que tu es né d’une étoile, comme elle. Les morts se reconnaissent, les vivants aussi.', n: 'yes' },
    yes: { t: 'Calme les bêtes-ronces et récolte la soie de spore: je tisserai un remède.' },
    askSap: { t: 'Des créatures-pousses ont fui pendant la maladie. Si tu les retrouves, ramène-les-moi.', c: [
      { l: 'D’accord.', fx: { quest: 'side_lost_saplings' }, end: true }, { l: 'Plus tard.', end: true }] },
    saplings: { t: 'Les voilà ! Elles chantent déjà. La Mère te remercie, et moi aussi.' } } },
  root_elder: { speaker: 'Vieille-Racine', portrait: '🌳', entry: [[{ quest: 'side_seed_of_life' }, 'go']], nodes: {
    start: { t: '…Jeune pousse d’étoile… As-tu… un monde… à toi… ?', c: [
      { l: 'Pépin, une petite planète verte.', n: 'seed' }, { l: '(Attendre poliment)', n: 'seed' }] },
    seed: { t: 'Prends… cette… graine. Plante-la… chez toi. Elle… grandira… avec toi.', fx: { quest: 'side_seed_of_life' } },
    go: { t: 'Va… La graine… attendra… le temps… qu’il faudra.' } } },

  // ───────── Mycélia ─────────
  dreamer: { speaker: 'Rêvasse', portrait: '🍄', entry: [[{ step: ['side_dream_walk', 1] }, 'share'], [{ canStart: 'side_spore_cure', done: 'side_dream_walk' }, 'cure']], nodes: {
    start: { t: 'Silence… les étoiles rêvent. Si tu veux voir leurs rêves, rapporte-moi de la mousse onirique.', c: [
      { l: 'Rêvons !', fx: { quest: 'side_dream_walk' }, end: true }, { l: 'Je préfère rester éveillé.', end: true }] },
    share: { t: '…Je vois… un jardin hors des cartes. Des créatures dorées. Une porte qu’on ne trouve qu’en se souvenant… Étrange rêve.', fx: { take: [['dreamMoss', 3]] } },
    cure: { t: 'Les sporelins font des cauchemars. Ils mordent tout. Calme-les, veux-tu ?', c: [
      { l: 'Je m’en charge.', fx: { quest: 'side_spore_cure' }, end: true }, { l: 'Non.', end: true }] } } },

  // ───────── Thalassa ─────────
  houle: { speaker: 'Capitaine Houle', portrait: '⚓', entry: [[{ step: ['side_lighthouse', 1] }, 'lit'], [{ step: ['side_sky_whale', 1] }, 'song'], [{ canStart: 'side_sky_whale' }, 'whale']], nodes: {
    start: { t: 'Mon phare est mort, et sans lui les baleines célestes s’échouent sur les récifs. Il me faut du verre de marée pour le rallumer.', c: [
      { l: 'Je trouverai ce verre.', fx: { quest: 'side_lighthouse' }, end: true }, { l: 'Je ne suis pas marin.', end: true }] },
    lit: { t: 'Il brûle à nouveau. Regarde au large… elles reviennent. Merci.', fx: { take: [['tideGlass', 3]] } },
    whale: { t: 'Une baleine chante au-dessus du large depuis que le phare brille. Va écouter, et raconte-moi.', c: [
      { l: 'J’y vais.', fx: { quest: 'side_sky_whale' }, end: true }, { l: 'Plus tard.', end: true }] },
    song: { t: 'Elle chantait le nom d’un Titan ? Par les abysses… Les vieux disent que les Titans dorment sous chaque soleil.' } } },

  // ───────── Voltaria ─────────
  spark_rebel: { speaker: 'Étincelle', portrait: '⚡', entry: [[{ step: ['side_sabotage', 1] }, 'sab'], [{ done: 'main_3_rebellion', canStart: 'side_sabotage' }, 'askSab']], nodes: {
    start: { t: 'C’est toi, le « dieu du Labo » ? L’Empire siphonne nos tempêtes pour alimenter une arme conçue pour te capturer.', c: [
      { l: 'Je me bats avec vous.', n: 'join' }, { l: 'Quelle arme ?', n: 'weapon' }] },
    weapon: { t: 'Une cage-étoile. L’Empereur Rubis veut t’y mettre et se servir de toi comme d’un réacteur.', n: 'join' },
    join: { t: 'Alors prouve-le: abats leurs drones écarlates. Chaque drone abattu, c’est une tempête rendue à Voltaria.' },
    askSab: { t: 'La raffinerie au sud. Trois chevaliers la gardent. Tu t’en charges, je pose les charges.', c: [
      { l: 'Allons-y.', fx: { quest: 'side_sabotage' }, end: true }, { l: 'Pas maintenant.', end: true }] },
    sab: { t: 'Les charges ont sauté. La raffinerie est hors service. Prends ces éclats, tu les as gagnés.' } } },

  // ───────── Bastion ─────────
  garance: { speaker: 'Chevalier Garance', portrait: '🛡', entry: [[{ step: ['side_defector', 1] }, 'free']], nodes: {
    start: { t: 'Ne tire pas ! Je… je ne veux plus servir Rubis. Il a ordonné de brûler Cendrelune. Aide-moi à fuir.', c: [
      { l: 'Je te couvre.', fx: { quest: 'side_defector' }, end: true }, { l: 'Pourquoi te croire ?', n: 'why' }] },
    why: { t: 'Parce que j’enlève mon casque devant toi. Personne ne fait ça dans l’Empire.', c: [
      { l: 'D’accord, je te couvre.', fx: { quest: 'side_defector' }, end: true }, { l: 'Non.', end: true }] },
    free: { t: 'Libre… Je rejoins Étincelle. Si tu affrontes Rubis, vise la couronne: c’est la source de son pouvoir.', fx: { flag: 'emperor_weakness' } } } },

  // ───────── Nécropole ─────────
  echo_keeper: { speaker: 'Gardienne-Écho', portrait: '👻', entry: [[{ step: ['side_echo_names', 1] }, 'names'], [{ done: 'main_4_titans', canStart: 'side_echo_names' }, 'askNames']], nodes: {
    start: { t: 'Tu viens réveiller les morts, ou les rejoindre ? … Ah. Tu es celui qu’ils attendent. Les Titans se lèvent, enfant. Pour toi.', c: [
      { l: 'Pour me tuer ?', n: 'kill' }, { l: 'Pour me servir ?', n: 'serve' }] },
    kill: { t: 'Pour te mettre à l’épreuve. Un dieu qui ne peut vaincre un Titan n’est qu’une étoile qui se prend pour le ciel.', n: 'go' },
    serve: { t: 'Les Titans ne servent personne. Pas même les dieux. Surtout pas les dieux.', n: 'go' },
    go: { t: 'Kaal s’éveille sur Ossable. Le Serpent Ouroboros rôde entre les mondes. Vaincs-les, et la vérité t’attendra sur Umbra.' },
    askNames: { t: 'Chaque épave garde l’écho d’un nom oublié. Rapporte-moi leurs reliques, et je les graverai.', c: [
      { l: 'Je les rapporterai.', fx: { quest: 'side_echo_names' }, end: true }, { l: 'Plus tard.', end: true }] },
    names: { t: 'Mille noms. Mille équipages. Ils ne seront plus oubliés. Et toi, enfant… tu sais maintenant où se cache le premier jardin.' } } },

  // ───────── Cœur Galactique ─────────
  first_one: { speaker: 'Le Premier', portrait: '✨', nodes: {
    start: { t: 'Te voilà enfin. J’étais le premier dieu. Aelion est le second. Tu es le troisième, si tu le veux.', c: [
      { l: 'Qui a créé le Laboratoire ?', n: 'lab' }, { l: 'Que veut Aelion ?', n: 'aelion' }] },
    lab: { t: 'Aelion. Il a peur de s’éteindre. Il voulait un successeur docile, cultivé en cuve. Il ne t’attendait pas… libre.', n: 'aelion' },
    aelion: { t: 'Il veut te consumer pour vivre encore une éternité. Pour l’affronter, deviens l’Architecte. Ensuite… à toi de choisir ce que sera l’univers.' } } },
  ending: { speaker: 'Le Premier', portrait: '✨', radio: true, nodes: {
    start: { t: 'Aelion s’éteint. Le trône du ciel est vide. Il t’attend. Que feras-tu de ta divinité ?', c: [
      { l: '🌱 Créer: semer de nouveaux mondes.', fx: { flag: ['ending_create', 'ending_chosen'] }, n: 'create' },
      { l: '🛡 Protéger: veiller sur ceux que j’aime.', fx: { flag: ['ending_protect', 'ending_chosen'] }, n: 'protect' },
      { l: '✺ Transcender: devenir l’univers lui-même.', fx: { flag: ['ending_transcend', 'ending_chosen'] }, n: 'transcend' }] },
    create: { t: 'Alors chaque grain de poussière deviendra une étoile, et chaque étoile, un monde. Boulon aura beaucoup de capsules à réparer.' },
    protect: { t: 'Alors tu redescendras sur Pépin, et aucune cuve ne retiendra plus jamais personne. Le ciel aura un gardien.' },
    transcend: { t: 'Alors tu n’auras plus de visage, mais tous les visages. Quelque part, un grain d’étoile ouvre les yeux…' } } },
};
