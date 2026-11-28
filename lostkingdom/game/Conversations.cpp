// LOST KINGDOM — 3.8.0 the story told in conversations. The main quests used to be one line of text; each turning
// point is now a scene between the hero and the person in front of them, line by line, each with a mood that
// drives the gestures of whoever speaks (Game::dialogGesture) and the framing of the camera. Keys:
//   meet:<npc>    the first time you speak to a main character
//   offer:<quest> when the quest is proposed
//   done:<quest>  when it is handed in
// Speaker: 'N' the person you speak to, 'H' the hero. Mood: 0 talk, 1 warm, 2 grave, 3 firm/angry, 4 wonder,
// 5 greeting (a wave).
#include "Game.h"
#include <map>

namespace lk {

const std::vector<ConvLine>* conversation(const std::string& key) {
    static const std::map<std::string, std::vector<ConvLine>> C = {
        // ======================================================================= ACT I — the Crown
        {"meet:maelle", {
            {'N', "Doucement… Assieds-toi, tu tiens à peine debout. Les pêcheurs t'ont trouvé à l'aube, couché dans le varech.", 5},
            {'H', "Je ne me souviens de rien. Ni de mon nom, ni de la mer.", 2},
            {'N', "La mer rend parfois ce qu'elle a pris il y a longtemps. Montre-moi ton poignet.", 0},
            {'N', "Cette marque… Une couronne fendue en six. Je l'ai vue une seule fois : brodée sur les tapisseries du Haut-Roi.", 4},
            {'H', "Le Haut-Roi ? Celui des chansons ?", 4},
            {'N', "Aldric. Il a brisé la Couronne-Mère pour sceller l'Abîme, il y a trois siècles. On dit qu'il a lié son âme au sceau.", 2},
            {'N', "Je suis Maëlle, la doyenne de Brindlemere. Repose-toi. Ensuite, nous chercherons ensemble qui tu es.", 1},
        }},
        {"offer:m2", {
            {'N', "J'ai fouillé les vieux registres toute la nuit. Il existe un moyen d'écouter le passé.", 0},
            {'N', "Au sud du village, la Grotte des Murmures garde une Pierre d'écho. Elle répète les voix de ceux qui l'ont touchée.", 2},
            {'H', "Si Aldric l'a touchée… je pourrais entendre sa voix ?", 4},
            {'N', "Peut-être même la tienne. Mais les gobelins ont fait de la grotte leur terrier. Prends ce qu'il te faut, et sois prudent.", 3},
        }},
        {"done:m2", {
            {'H', "La voici. Elle vibre dans ma main, comme si elle respirait.", 4},
            {'N', "Pose-la sur la table… Écoute.", 2},
            {'N', "« Six gardiens. Six fragments. Quand le sceau faiblira, je reviendrai par la mer. »", 2},
            {'H', "Cette voix… c'est la mienne.", 4},
            {'N', "C'est la voix d'Aldric, Errant. Une âme liée au sceau ne meurt pas : elle attend. Et le sceau a cédé.", 2},
            {'N', "Tu n'es pas un naufragé. Tu es un roi qui revient.", 1},
        }},
        {"offer:m3", {
            {'N', "Les six fragments sont entre les mains de gardiens corrompus. Mais pour reforger la Couronne, il faut le savoir de l'Ordre.", 0},
            {'N', "Va à Highspire, au nord-est. Demande Frère Alaric, l'archiviste. Il ne quitte jamais ses livres.", 0},
            {'H', "Et s'il ne me croit pas ?", 2},
            {'N', "Il te croira. Donne-lui cette lettre. Je l'ai scellée de la cire de ma mère, il la reconnaîtra.", 1},
        }},
        {"meet:alaric", {
            {'N', "Un pèlerin ? Les archives ferment au crépuscule, mais… attendez.", 5},
            {'N', "(Il pose sa plume et vous regarde longtemps.) Ce visage. Les portraits de la grande galerie…", 4},
            {'H', "Maëlle de Brindlemere m'envoie. Vous êtes Frère Alaric ?", 0},
            {'N', "Je le suis. Pardonnez-moi, sire — pardonne-moi, voyageur. Les vieux mots me reviennent. Que veux-tu à l'Ordre ?", 2},
        }},
        {"done:m3", {
            {'N', "La cire de Brindlemere… (Il lit, et ses mains tremblent.) Elle dit que la marque brûle encore.", 4},
            {'H', "Elle brûle quand je pense à l'Abîme.", 2},
            {'N', "Alors c'est vrai. Les archives ont attendu ce jour pendant trois siècles. Moi aussi, en un sens.", 1},
        }},
        {"offer:m4", {
            {'N', "Écoute bien. Six gardiens ont reçu un fragment de la Couronne, et six ont été corrompus par ce qui remonte de l'Abîme.", 2},
            {'N', "Le Gardien des Racines à Elderwood. Le Roi Squelette sous Gravemoor. Le Colosse de Glace à Frostheim.", 0},
            {'N', "Le Dragon Cendré dans les terres brûlées. La Reine des Araignées dans son Antre. Et le Titan de Pierre, ici, dans nos cimes.", 0},
            {'H', "Six combats. Par lequel commencer ?", 3},
            {'N', "Par celui que tu choisiras. Un roi choisit son chemin. Rapporte-moi les fragments : la forge de l'Ordre t'attendra.", 1},
        }},
        {"offer:m5", {
            {'N', "Six. Tu les as tous. Je n'aurais jamais cru vivre ce jour.", 4},
            {'H', "Ils sont lourds. Plus lourds que de l'or.", 2},
            {'N', "C'est le poids de trois siècles. Donne-les-moi. La forge de l'Ordre attend depuis trop longtemps.", 1},
        }},
        {"done:m5", {
            {'N', "(Le métal chante sous le marteau. Les six éclats se fondent en un seul anneau de lumière.)", 4},
            {'N', "Voilà. La Couronne-Mère. Elle est à vous, sire. Elle l'a toujours été.", 1},
            {'H', "Cette cicatrice, sur votre menton… je l'ai déjà vue. La nuit de la Brisure. Un novice tenait les tenailles.", 4},
            {'N', "(Il sourit, sans répondre. Puis il éteint la forge.) Les vieux souvenirs reviennent vite, quand la Couronne est proche.", 2},
        }},
        {"offer:m6", {
            {'N', "Il n'y a plus de temps pour les questions. Sire Corwin garde l'entrée de l'Abîme, à Morneval.", 2},
            {'N', "Il était votre champion. Il ne laissera passer que son roi… s'il le reconnaît. Sinon, il faudra qu'il tombe.", 3},
            {'H', "Et vous, Alaric ? Vous venez ?", 0},
            {'N', "J'ai mes propres comptes à régler. Nous nous reverrons au fond, sire. Je vous le promets.", 2},
        }},
        // ======================================================================= ACT II — the Silence
        {"offer:n1", {
            {'N', "Errant ! Un pêcheur de la côte sud a remonté ceci dans ses filets. Approche-le de ton oreille.", 5},
            {'N', "(Dans le coquillage, une voix de femme, à peine un souffle.) « Le septième ne dort pas à Aelindor. »", 2},
            {'N', "« Retrouve-moi à Port-Azur, avant que la marée tourne. » C'est Kaede, n'est-ce pas ?", 4},
            {'H', "Un septième joyau ? La Couronne n'en avait que six.", 4},
            {'N', "Les chansons mentent souvent par omission. Port-Azur est tout au sud, après Gravemoor. Va.", 3},
        }},
        {"meet:orla", {
            {'N', "Encore un étranger. Le vent a tourné ce matin, il tourne toujours quand l'un d'eux arrive.", 5},
            {'H', "Je cherche une femme. Kaede. Elle m'a donné rendez-vous ici.", 0},
            {'N', "La ninja ? Elle n'a pas attendu. Elle ne attend jamais personne, celle-là. Je suis Orla, la maîtresse du port.", 3},
        }},
        {"offer:n2", {
            {'N', "Kaede est partie vers Shirokawa il y a trois jours. Elle m'a laissé un mot : « Ne la cherche pas, Orla. Fais tomber Mareval. »", 0},
            {'N', "Mareval, c'est le capitaine des Corsaires du Pont du Tonnerre. Il étrangle mon port, et il tient la carte d'un temple oublié.", 3},
            {'H', "Une carte vers le septième joyau ?", 4},
            {'N', "Vers un passage sous le temple de Shirokawa. Dégage la crique, abats Mareval, et rapporte-moi cette carte.", 3},
        }},
        {"done:n2", {
            {'N', "La carte… et du sang de corsaire sur le coin. Tu n'as pas fait les choses à moitié.", 1},
            {'N', "Regarde : Shirokawa, et là, sous le sanctuaire, une porte dessinée à l'encre noire.", 4},
            {'H', "Kaede est déjà là-bas.", 2},
            {'N', "Alors ne la fais pas attendre. Port-Azur te doit sa mer, voyageur. Je ne l'oublierai pas.", 1},
        }},
        {"meet:hanzo", {
            {'N', "Tu es celui dont Kaede m'a parlé. Elle parle peu, alors je l'ai écoutée.", 5},
            {'H', "Où est-elle ?", 3},
            {'N', "Là où elle veut être : devant la mer, à attendre. Je suis Hanzo. Le silence de ce temple est ma charge.", 2},
        }},
        {"offer:n3", {
            {'N', "Sous ce temple dort le Silence, le septième joyau. Il prend la voix de ceux qui l'approchent.", 2},
            {'N', "La porte du sanctuaire demande l'eau et le feu. L'eau, ce sont trois perles de marée : des souvenirs de la mer.", 0},
            {'H', "Où les trouver ?", 0},
            {'N', "Dans la Grotte des Marées, à l'ouest de Port-Azur. Les noyés et les sirènes en portent parfois. Va.", 0},
        }},
        {"done:n3", {
            {'N', "Trois perles, claires comme des yeux. Écoute-les… elles se souviennent de la houle.", 4},
            {'N', "L'eau est prête. Reste le feu, et c'est le plus dur.", 2},
        }},
        {"offer:n4", {
            {'N', "Le feu, c'est le Sceau du Soleil. Le Roi-Soleil Sekhmar le garde dans son temple, au cœur des Sables d'Ambre.", 2},
            {'H', "Un roi ? Encore un ?", 4},
            {'N', "Un roi qui a marchandé avec le soleil pour ne jamais mourir. Son royaume est devenu du sable. Lui est resté.", 2},
            {'N', "Seul, tu n'y arriveras pas. Va à Solmar, demande l'émira Zahra. Elle a vu mourir assez de caravanes pour savoir où ne pas aller.", 0},
        }},
        {"meet:zahra", {
            {'N', "Assieds-toi, voyageur. Le thé refroidit plus vite que la patience.", 5},
            {'H', "Hanzo de Shirokawa m'envoie. Je cherche le temple de Sekhmar.", 0},
            {'N', "Hanzo… Le vieux sage du lagon. Bois d'abord. Ensuite, je te dirai pourquoi personne n'en revient.", 1},
        }},
        {"offer:n5", {
            {'N', "Le temple est scellé par une clé d'ambre. Une Reine Scorpion la porte dans ses pinces.", 2},
            {'N', "Mes caravanes sont tombées en essayant de lui échapper. Des pilleurs de dunes la servent, et ses enfants couvrent les crêtes.", 3},
            {'H', "Alors je commence par eux.", 3},
            {'N', "Voilà une réponse que j'aime. Le Nid est à l'est de Solmar. Rapporte-moi la clé, et les caravanes passeront de nouveau.", 1},
        }},
        {"done:n5", {
            {'N', "La clé d'ambre… Il y a trente ans qu'on la cherche. Mon père est mort avec ce nom à la bouche.", 2},
            {'H', "Il peut reposer, maintenant.", 1},
            {'N', "Oui. Merci, voyageur. Solmar te doit plus que du thé.", 1},
        }},
        {"offer:n6", {
            {'N', "Le temple s'ouvre. Je dois te dire une chose avant que tu y entres.", 2},
            {'N', "Sekhmar était mon ancêtre. Il était bon, avant. Il voulait seulement que son peuple ne meure jamais de soif.", 2},
            {'H', "Que dois-je lui dire ?", 0},
            {'N', "Que Solmar lui a pardonné. Puis rapporte-moi le Sceau, ou rien.", 3},
        }},
        {"done:n6", {
            {'N', "Le Sceau… Il est encore chaud. (Elle le tient longtemps contre son cœur.)", 4},
            {'H', "Il a souri, à la fin. Je crois qu'il a entendu.", 1},
            {'N', "Alors il dort enfin. Va dire à Hanzo que le feu est prêt.", 1},
        }},
        {"offer:n7", {
            {'N', "Le Sceau du Soleil, et trois perles de marée. L'eau et le feu côte à côte, comme on me l'a enseigné.", 4},
            {'N', "Donne-les-moi, que j'ouvre la porte.", 0},
        }},
        {"done:n7", {
            {'N', "(La pierre glisse. Un souffle froid monte du sanctuaire, et toutes les cloches du temple se taisent.)", 2},
            {'N', "La porte est ouverte. Descends, et ne laisse rien parler à ta place.", 3},
        }},
        {"offer:n8", {
            {'N', "Tu l'entendras avant de le voir. Ou plutôt, tu n'entendras plus rien du tout.", 2},
            {'N', "Si une voix te parle, ne réponds pas. Si ton nom résonne, ne le reconnais pas.", 3},
            {'H', "Et si c'est la voix d'Aldric ?", 2},
            {'N', "Surtout si c'est la sienne. Prends le joyau, et refermons cette porte pour de bon.", 3},
        }},
        {"done:n8", {
            {'N', "Écoute… les vagues. Les oiseaux. Le monde reprend sa voix.", 4},
            {'H', "Là-dessous, il parlait avec la mienne.", 2},
            {'N', "Et tu ne lui as pas répondu. Va montrer le joyau à Kaede. Elle attend depuis quinze ans.", 1},
        }},
        // ======================================================================= 3.9.0 Aldrenbourg
        {"meet:gaspard", {
            {'N', "Un étranger qui regarde la fontaine au lieu des étals ! Ça ne m'arrive pas souvent.", 5},
            {'H', "La statue… ce chevalier, l'épée levée. Qui est-ce ?", 4},
            {'N', "Le Haut-Roi Aldric, voyons ! Le sculpteur ne l'a jamais vu, remarque. Il l'a fait d'après les chansons.", 1},
            {'N', "Gaspard de Vaux, bourgmestre d'Aldrenbourg. La plus grande ville libre de Greenhaven : quatre portes, vingt tours et un marché tous les jours.", 0},
            {'N', "Et dans trois jours, la Grande Foire. Si elle a lieu. Entre les voleurs et le four de Colette, je ne dors plus.", 2},
            {'H', "Je peux peut-être aider.", 0},
            {'N', "Ah ! Voilà une phrase que j'aime entendre. Va voir Colette à la boulangerie, et le Sergent Garin au guet. Ensuite, reviens me voir.", 1},
        }},
        {"offer:al_fair", {
            {'N', "Colette m'a raconté. Le four ronfle, la fournée sera prête !", 1},
            {'N', "Mais une Foire sans confiseries, c'est un enterrement avec des étals. Il me faut du miel, trois pots.", 3},
            {'H', "Et la musique ?", 0},
            {'N', "Mélisande cherche encore la chanson d'ouverture. Elle dit qu'il lui manque « une histoire neuve ». Toi, peut-être ?", 4},
        }},
        {"done:al_fair", {
            {'N', "(Les tambours roulent sur la place. Mélisande chante l'histoire d'un naufragé sans nom, et toute la ville reprend le refrain.)", 4},
            {'H', "C'est… mon histoire.", 4},
            {'N', "Les meilleures chansons parlent toujours de quelqu'un qui ne se souvient pas de son nom. C'est elle qui le dit.", 1},
            {'N', "La Grande Foire est ouverte ! Aldrenbourg te doit sa fête. Prends ceci, et reviens quand tu veux : ici, tu es chez toi.", 1},
        }},
        {"meet:melisande", {
            {'N', "Chut… écoute la fontaine. Elle a un rythme, tu l'entends ? Trois temps, comme une valse.", 5},
            {'H', "Je n'entends que de l'eau.", 0},
            {'N', "C'est bien le problème des héros : ils n'écoutent jamais. Je suis Mélisande. Je chante ce que les autres oublient.", 1},
        }},
        {"meet:kaede_s", {
            {'N', "Tu as fini par venir. La mer te l'avait dit.", 5},
            {'H', "Tu es partie sans m'attendre, à Port-Azur.", 3},
            {'N', "Je ne reviens jamais sur mes pas. Mais pour toi, je suis revenue jusqu'ici. C'est déjà beaucoup.", 1},
        }},
        {"offer:n9", {
            {'N', "Il est là. Je le sens, même sans le toucher.", 2},
            {'N', "Donne-le-moi. Pas pour le garder : pour l'enterrer là où personne n'ira. La mer est assez grande pour ça.", 2},
            {'H', "Tu es sûre ? Ce joyau pourrait rendre un roi invincible.", 4},
            {'N', "C'est bien pour ça. Les rois invincibles finissent toujours par se taire. Comme Sekhmar.", 3},
        }},
        {"done:n9", {
            {'N', "(Le joyau s'enfonce dans l'eau sombre, sans un bruit.)", 2},
            {'N', "Voilà. C'est fini. Pour la première fois depuis quinze ans, je n'entends plus que la marée.", 1},
            {'H', "Et maintenant ?", 0},
            {'N', "Maintenant, on regarde la mer. Ça, tu peux le garder.", 1},
        }},
    };
    auto it = C.find(key);
    return it == C.end() ? nullptr : &it->second;
}

// ============================================================================================ the scene
void Game::playConversation(const std::string& key, bool prepend) {
    const std::vector<ConvLine>* c = conversation(key);
    if (!c) return;
    dialog.who.resize(dialog.lines.size(), 'N');
    dialog.mood.resize(dialog.lines.size(), 0);
    std::vector<std::string> L; std::vector<char> W; std::vector<uint8_t> M;
    for (auto& l : *c) { L.push_back(l.text); W.push_back(l.who); M.push_back((uint8_t)l.mood); }
    auto at = prepend ? 0 : dialog.lines.size();
    dialog.lines.insert(dialog.lines.begin() + at, L.begin(), L.end());
    dialog.who.insert(dialog.who.begin() + at, W.begin(), W.end());
    dialog.mood.insert(dialog.mood.begin() + at, M.begin(), M.end());
    dialog.shownLine = -1;
}

static ClipId moodClip(int mood, bool hero) {
    switch (mood) {
        case 1: return C_TALK_WARM;
        case 2: return C_TALK_GRAVE;
        case 3: return C_TALK_FIRM;
        case 4: return C_TALK_WONDER;
        case 5: return hero ? C_BOW_H : C_WAVE;
        default: return C_TALK;
    }
}

// A new line starts: the speaker makes the gesture of its mood, the listener nods now and then.
void Game::dialogLineStart() {
    dialog.shownLine = dialog.line;
    dialog.blip = 0;
    if (player == NullEntity) return;
    bool hero = dialog.whoAt(dialog.line) == 'H';
    int mood = dialog.moodAt(dialog.line);
    Character* npc = reg.valid(dialog.ent) ? reg.get<Character>(dialog.ent) : nullptr;
    Character& p = pc();
    Character* spk = hero ? &p : npc;
    Character* lis = hero ? npc : &p;
    if (spk && spk->vis.anim.skeleton) spk->vis.anim.play(&clip(moodClip(mood, hero)), 1.f, true, 0.2f);
    if (lis && lis->vis.anim.skeleton) {
        if (lis == &p && p.vis.anim.action() == &clip(C_TALK)) p.vis.anim.stopAction(0.25f);
        if (hero || (dialog.line % 2 == 1 && mood != 5)) lis->vis.anim.play(&clip(C_NOD), 1.f, true, 0.2f);
    }
    if (mood == 4) play("ui_open", 0.25f, 1.4f);
}

// Each frame: the two face each other, the hero's talk stops with his line, voice blips while text appears, the
// hero comes alive when left standing (a look around, a stretch).
void Game::updateDialogScene(float dt) {
    if (player == NullEntity) return;
    Character& p = pc();
    if (dialog.open) {
        dialog.openT += dt;
        Character* npc = reg.valid(dialog.ent) ? reg.get<Character>(dialog.ent) : nullptr;
        if (npc && !riding && driving < 0) {
            float yp = std::atan2(npc->pos.x - p.pos.x, npc->pos.z - p.pos.z);
            float yn = std::atan2(p.pos.x - npc->pos.x, p.pos.z - npc->pos.z);
            p.yaw = dampAngle(p.yaw, yp, 6.f, dt);
            npc->yaw = dampAngle(npc->yaw, yn, 6.f, dt);
        }
        if (dialog.whoAt(dialog.line) != 'H' && p.vis.anim.action() == &clip(C_TALK)) p.vis.anim.stopAction(0.3f);
        heroIdleT = 0;
        return;
    }
    dialog.openT = 0;
    // idle life
    float sp = length(vec2(p.vel.x, p.vel.z));
    bool calm = state == GameState::Playing && menu == Menu::None && !cine.active && !riding && driving < 0 && p.grounded && !p.swimming && !p.dead && bossEnt == NullEntity;
    if (calm && sp < 0.15f && !p.vis.anim.action()) heroIdleT += dt; else heroIdleT = 0;
    if (heroIdleT > 9.f) {
        heroIdleT = -4.f - (float)((day * 7 + (int)gameTime) % 5);   // the next one comes a little later
        p.vis.anim.play(&clip(((int)gameTime / 9) % 3 == 2 ? C_IDLE_STRETCH : C_IDLE_LOOK), 1.f, true, 0.3f);
    }
}

}  // namespace lk
