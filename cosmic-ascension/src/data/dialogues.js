// Dialogues (B3). Format: systems/Dialogue.js. radio:true = transmission (pas besoin que le PNJ soit à l'écran).
// talked: id de PNJ compté comme « parlé » à la fin (utile pour les radios qui valident une étape `talk`).
export const DIALOGUES = {
  // ───────── Transmissions radio (Boulon) ─────────
  intro: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: '…ccchhh… Tu m’entends, petite étincelle ? Ici Boulon. Vieux robot de maintenance. C’est moi qui ai ouvert ta cuve.', n: 'b' },
    b: { t: 'Tu viens de t’échapper du Laboratoire Ω dans une capsule volée. Ils vont te chercher. Vite, pose-toi sur Pépin, la petite planète verte devant toi.', n: 'c' },
    c: { t: 'Tu sais voler ? Joystick pour diriger, ≫ pour accélérer. Quand tu es assez près: ⏎ pour atterrir.', c: [
      { l: 'Qui suis-je ?', n: 'who' }, { l: 'Compris, j’y vais !', n: 'go' }] },
    who: { t: 'Bonne question. Dans les fichiers du Labo, tu t’appelles « Spécimen Zéro ». Moi je trouve que tu ressembles à un grain d’étoile.', n: 'go' },
    go: { t: 'Absorbe la poussière cosmique là-bas, elle te nourrit. Et… tu pourrais grandir. Beaucoup. Boulon, terminé.' } } },
  comm_home: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Bienvenue sur Pépin ! Les orbes qui flottent, ce sont des fragments d’étoile. Touche-les pour les absorber.', n: 'b' },
    b: { t: 'Gare aux slimes violets: ⚔ pour attaquer, ⤒ pour sauter. Et si tu te sens… bizarre, c’est normal. Ton corps veut évoluer.' } } },
  comm_evolved: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Par mes circuits ! Tu as… des bras ! Le Labo n’avait jamais réussi ça. Ils doivent avoir capté le pic d’énergie.', n: 'b' },
    b: { t: 'Ils vont envoyer des drones. Le meilleur moyen de se défendre, c’est d’attaquer: retourne au Laboratoire Ω, je te guiderai. Il est tout près de Pépin.' } } },
  comm_lab: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Le Labo. Ça me donne des frissons de rouille. Les drones patrouillent; détruis-en quelques-uns pour ouvrir le chemin vers Quill.' } } },
  comm_act2: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Un catalyseur forgé sur Braise-9… Les Braisiens sont les meilleurs forgerons du secteur. Cap sur la planète de lave, à l’est !' } } },
  comm_stage3: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Ces ailes ! On dirait les vieilles fresques des Titans… Petite étincelle, je crois que tu n’es pas une expérience ratée. Tu es la seule réussie.' } } },
  comm_death: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Hé ! Je t’ai recousu avec du ruban d’étoile. Tu as perdu un peu de poussière. Sois prudent, d’accord ?' } } },
  comm_empire: { speaker: 'Boulon', portrait: '🤖', radio: true, nodes: {
    start: { t: 'Attention: tu entres dans la Zone Interdite. L’Empire Écarlate tire à vue. Ici, les ennemis sont bien plus coriaces.' } } },

  // ───────── Pépin ─────────
  bolt_intro: { speaker: 'Boulon', portrait: '🤖', entry: [[{ step: ['side_drone_parts', 1] }, 'repair'], [{ canStart: 'side_drone_parts', done: 'main_1_awakening' }, 'parts']], nodes: {
    start: { t: 'Enfin face à face ! Tu es encore plus petit qu’à travers la caméra. Enfin, de moins en moins petit.', c: [
      { l: 'Merci de m’avoir libéré.', n: 'thx' }, { l: 'Pourquoi m’as-tu aidé ?', n: 'why' }] },
    thx: { t: 'Pas de merci entre fugitifs. Bon, je retourne bidouiller ta capsule.' },
    why: { t: 'Parce que j’ai nettoyé ta cuve pendant 300 ans. On finit par s’attacher. Et parce que personne ne devrait être enfermé.' },
    parts: { t: 'Mes articulations grincent. Si tu trouves de l’alliage dans les épaves ou sur les drones, rapporte-m’en, je pourrai même améliorer ta capsule.', c: [
      { l: 'J’en trouverai.', fx: { quest: 'side_drone_parts' }, end: true }, { l: 'Plus tard.', end: true }] },
    repair: { t: 'Oh, de l’alliage ! Tiens bon, je me fais une beauté… Voilà ! Je cours comme un neuf. Enfin, je roule.', fx: { take: [['scrapAlloy', 4]] } } } },
  mayor_intro: { speaker: 'Maire Pipo', portrait: '🐣', entry: [[{ step: ['side_lost_pipion', 1] }, 'back'], [{ done: 'side_lost_pipion' }, 'after']], nodes: {
    start: { t: 'Oh ! Un visiteur ! Bienvenue à Pépin, bienvenue ! Je suis le maire. Le seul. Nous sommes douze.', n: 'b' },
    b: { t: 'Dites… le petit Pip a suivi un slime et n’est pas revenu. Vous pourriez chasser ces bestioles ?', c: [
      { l: 'Je m’en occupe !', fx: { quest: 'side_lost_pipion' }, end: true }, { l: 'Je dois d’abord m’occuper de moi.', end: true }] },
    back: { t: 'Pip est rentré tout seul en criant « une étoile m’a sauvé » ! Tenez, la récompense de la mairie.' },
    after: { t: 'Les Pipions parlent de vous comme d’un héros. On a même renommé la fontaine. Bon, c’était une flaque.' } } },

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
    tizon: { t: 'Tu n’as pas vu mon fils ? P’tit Tison est encore parti admirer les coulées. Il va finir en flaque !', c: [
      { l: 'Je le retrouve.', fx: { quest: 'side_tizon_lost' }, end: true }, { l: 'Pas le temps.', end: true }] },
    tizonBack: { t: 'Tison ! Petite tête de lave ! … Merci, voyageur. La forge te sera toujours ouverte.' },
    core: { t: 'La grande forge chauffe trop. Si elle explose, adieu Braise-9. Il me faudrait du givre qui ne fond jamais…', c: [
      { l: 'Je connais un endroit: Glacio.', fx: { quest: 'side_cool_the_core' }, end: true }, { l: 'Bonne chance.', end: true }] },
    cool: { t: 'Du givre éternel ! Ça siffle, ça fume… ça tient ! Prends ce fer stellaire, il vient du cœur de la forge.', fx: { take: [['frostCrystal', 4]] } } } },
  tizon: { speaker: 'P’tit Tison', portrait: '🕯', nodes: {
    start: { t: 'Wouah ! T’es une étoile ? Une vraie ? Moi je voulais juste voir la grande coulée et y’a des slimes partout !', n: 'b' },
    b: { t: 'Tu me protèges ? Après on rentre, promis. Maman va me transformer en tisonnier…' } } },

  // ───────── Cendrelune ─────────
  hermit: { speaker: 'L’Ermite Gris', portrait: '🌫', entry: [[{ step: ['side_hermit_memory', 1] }, 'echo']], nodes: {
    start: { t: '…Une lumière. Il y a longtemps qu’aucune lumière n’est venue ici. Mon peuple a brûlé dans la guerre des Titans. J’ai oublié jusqu’à son nom.', c: [
      { l: 'Je peux t’aider à te souvenir.', fx: { quest: 'side_hermit_memory' }, n: 'help' }, { l: 'Je suis désolé.', end: true }] },
    help: { t: 'Les pierres d’écho gardent les voix. On en trouve dans la Nécropole… et sur les spectres de cendre.' },
    echo: { t: '« …Cendrins… nous étions les Cendrins… » Je m’en souviens. Merci, petite lumière. Un jour, toi aussi tu auras besoin qu’on se souvienne de toi.', fx: { take: [['echoStone', 2]] } } } },

  // ───────── Ossable ─────────
  zaya: { speaker: 'Zaya des Caravanes', portrait: '🐪', entry: [[{ step: ['side_caravan_escort', 1] }, 'saved'], [{ step: ['side_titan_bones', 1] }, 'bones'], [{ canStart: 'side_titan_bones' }, 'giant']], nodes: {
    start: { t: 'Hé, l’étoile ! Mes caravanes volantes sont attaquées par des scarabées. Tu as des muscles de lumière, non ?', c: [
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
    start: { t: 'Hmmmm… petit… être… Tu… as… une… maison… ?', c: [
      { l: 'Pépin, une petite planète verte.', n: 'seed' }, { l: '(Attendre poliment)', n: 'seed' }] },
    seed: { t: 'Prends… cette… graine. Plante-la… chez toi. Elle… grandira… avec toi.', fx: { quest: 'side_seed_of_life' } },
    go: { t: 'Va… planter… Pas… de… précipitation… Hmmm.' } } },

  // ───────── Mycélia ─────────
  dreamer: { speaker: 'Rêvasse', portrait: '🍄', entry: [[{ step: ['side_dream_walk', 1] }, 'share'], [{ canStart: 'side_spore_cure', done: 'side_dream_walk' }, 'cure']], nodes: {
    start: { t: 'Chuuut… les étoiles rêvent. Tu veux savoir de quoi ? Rapporte-moi de la mousse onirique et on rêvera ensemble.', c: [
      { l: 'Rêvons !', fx: { quest: 'side_dream_walk' }, end: true }, { l: 'Je préfère rester éveillé.', end: true }] },
    share: { t: '…Je vois… un jardin hors des cartes. Des créatures dorées. Une porte qu’on ne trouve qu’en se souvenant… Étrange rêve.', fx: { take: [['dreamMoss', 3]] } },
    cure: { t: 'Les sporelins font des cauchemars. Ils mordent tout. Calme-les, veux-tu ?', c: [
      { l: 'Je m’en charge.', fx: { quest: 'side_spore_cure' }, end: true }, { l: 'Non.', end: true }] } } },

  // ───────── Thalassa ─────────
  houle: { speaker: 'Capitaine Houle', portrait: '⚓', entry: [[{ step: ['side_lighthouse', 1] }, 'lit'], [{ step: ['side_sky_whale', 1] }, 'song'], [{ canStart: 'side_sky_whale' }, 'whale']], nodes: {
    start: { t: 'Ahoy, moussaillon lumineux ! Mon phare est éteint et les baleines célestes s’échouent. Il me faut du verre de marée.', c: [
      { l: 'Je trouverai ce verre.', fx: { quest: 'side_lighthouse' }, end: true }, { l: 'Je ne suis pas marin.', end: true }] },
    lit: { t: 'Il brille ! Regarde, au loin… elles reviennent ! Merci, l’étoile.', fx: { take: [['tideGlass', 3]] } },
    whale: { t: 'Une baleine chante au-dessus du large depuis que le phare brille. Va écouter, et raconte-moi.', c: [
      { l: 'J’y vais.', fx: { quest: 'side_sky_whale' }, end: true }, { l: 'Plus tard.', end: true }] },
    song: { t: 'Elle chantait le nom d’un Titan ? Par les abysses… Les vieux disent que les Titans dorment sous chaque soleil.' } } },

  // ───────── Voltaria ─────────
  spark_rebel: { speaker: 'Étincelle', portrait: '⚡', entry: [[{ step: ['side_sabotage', 1] }, 'sab'], [{ done: 'main_3_rebellion', canStart: 'side_sabotage' }, 'askSab']], nodes: {
    start: { t: 'Toi ! Le « dieu du Labo » ? Ha, je t’imaginais plus grand. L’Empire pompe nos tempêtes pour nourrir une arme capable de te capturer.', c: [
      { l: 'Je me bats avec vous.', n: 'join' }, { l: 'Quelle arme ?', n: 'weapon' }] },
    weapon: { t: 'Une cage-étoile. L’Empereur Rubis veut t’y mettre et se servir de toi comme d’un réacteur.', n: 'join' },
    join: { t: 'Alors prouve-le: abats leurs drones écarlates. Chaque drone abattu, c’est une tempête rendue à Voltaria.' },
    askSab: { t: 'La raffinerie au sud. Trois chevaliers la gardent. Tu t’en charges, je pose les charges.', c: [
      { l: 'Allons-y.', fx: { quest: 'side_sabotage' }, end: true }, { l: 'Pas maintenant.', end: true }] },
    sab: { t: 'BOUM ! Tu entends ? C’est le son de la liberté. Prends ces éclats, ils sont à toi.' } } },

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
