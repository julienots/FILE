// LOST KINGDOM — 1.12.0 the story told in chapters: multi-shot cinematics at the turning points of the main quest,
// the reveal of Frère Alaric (the novice with the scar, who never aged), Corwin recognising his king, and an ending
// the player chooses (take Aldric's place in the seal again, or break the Crown for ever). The Objectives tab lists
// the chapters seen, with a button to watch each one again.
#include "Game.h"

namespace lk {

const std::vector<ChapterDef>& chapters() {
    static const std::vector<ChapterDef> C = {
        {"prologue", "Prologue · La Chute des Couronnes",
         "Le Haut-Roi Aldric unit six royaumes sous la Couronne-Mère. Son mage Malgrath ouvrit l'Abîme sous Morneval. Aldric brisa la Couronne en six, la confia à six Gardiens et lia son âme au sceau. Trois siècles plus tard, la mer rejette un naufragé qui porte sa marque."},
        {"ch_echo", "I · La voix dans la pierre",
         "La Pierre d'écho a parlé avec la voix d'Aldric : une âme liée au sceau ne meurt pas, elle attend. Quand le sceau cède, elle est rejetée, comme une épave sur la grève."},
        {"ch_guardians", "II · Les six Gardiens",
         "À Highspire, Frère Alaric a nommé les six Gardiens corrompus. Il a dit « sire », puis s'est repris. Il attend les six fragments pour rallumer la forge de l'Ordre."},
        {"ch_scar", "III · Le novice à la cicatrice",
         "La Couronne-Mère est reforgée. Alaric était le novice qui tenait les tenailles la nuit de la Brisure : l'élève de Malgrath, qui a cessé de compter les années. Il veut que le roi porte la Couronne jusqu'à Morneval."},
        {"ch_corwin", "IV · Le champion qui attendait",
         "Sire Corwin a reconnu son roi au troisième assaut. Avant de tomber en cendre, il a prévenu : une couronne reforgée n'est pas une clé pour fermer, c'est une clé pour ouvrir."},
        {"ending", "V · Le choix d'Aldric",
         "Malgrath est tombé au fond de l'Abîme. Aldric a choisi le sort de la Couronne-Mère."},
        // ---- 2.0.0 Act II
        {"ch_act2", "ACTE II · I · Le message de la marée",
         "Un coquillage est arrivé de la côte sud : la voix de Kaede. Le septième joyau de la Couronne, le Silence, ne dort pas à Aelindor. Il dort de l'autre côté du désert, ou sous la mer."},
        {"ch_mareval", "ACTE II · II · La dette du capitaine",
         "Mareval a vendu son équipage à la mer pour une carte. La mer a gardé la dette, et lui avec. La carte désigne un temple à Shirokawa, et un passage sous le sanctuaire."},
        {"ch_sun_king", "ACTE II · III · Le soleil qui ne se couche pas",
         "Sekhmar a marchandé avec le soleil pour ne jamais mourir. Son royaume est devenu du sable. Il a rendu le Sceau, et il a enfin dormi."},
        {"ch_sanctum", "ACTE II · IV · L'eau et le feu",
         "Trois perles de marée et le Sceau du Soleil : Maître Hanzo ouvre la porte du sanctuaire, sous le temple de Shirokawa. Tout se tait."},
        {"ch_legacy", "ACTE II · V · L'héritage",
         "Le Silence est enterré au fond de la mer. Kaede, pour la première fois depuis quinze ans, regarde l'océan en face."},
    };
    return C;
}

// ------------------------------------------------------------------------------------------------ shots
std::vector<CineShot> Game::chapterShots(const std::string& id) {
    std::vector<CineShot> s;
    const World& w = world;
    auto at = [&](vec2 p, float dx, float dz, float up) { return vec3(p.x + dx, w.height(p.x + dx, p.y + dz) + up, p.y + dz); };
    auto P = [&](const char* id, vec2 def) { const Poi* p = w.poiById(id); return p ? p->pos : def; };
    vec2 B = P("brindlemere", {1480, 1620}), H = P("highspire", {2350, 820}), M = P("morneval", {1520, 2690}), A = P("abyss", {1080, 2840});
    vec2 cave = P("whisper_cave", {1660, 1900}), beach = B + vec2(20.f, 60.f);
    vec2 L = H + vec2(4.f, -14.f);   // the forecourt in front of the Order's monastery (Alaric's scenes, camera outside)
    const vec3 VIOLET(0.78f, 0.55f, 1.f), ALARIC(0.95f, 0.78f, 0.45f), CORWIN(0.62f, 0.78f, 1.f), GOLD(1.f, 0.85f, 0.4f);
    // an orbiting shot around a place: camera from (ang0, dist0, up0) to (ang1, dist1, up1), looking at the place
    auto orbit = [&](vec2 c, float a0, float a1, float d0, float d1, float u0, float u1, float lookUp) {
        CineShot k;
        k.from = at(c, std::cos(a0) * d0, std::sin(a0) * d0, u0); k.to = at(c, std::cos(a1) * d1, std::sin(a1) * d1, u1);
        k.lookFrom = at(c, 0, 0, lookUp); k.lookTo = at(c, 0, 0, lookUp + 1.f);
        return k;
    };
    // 1.12.0 no camera inside a building: an eye within a structure's footprint (and under its roof) goes above it
    auto safe = [&](vec3 e) {
        for (const Structure& st : w.structures) {
            if (st.kind == S_FIELD || st.kind == S_PEN || st.kind == S_BRIDGE || length(st.pos - e.xz()) > 30.f) continue;
            vec2 fp = structureFootprint(st.kind) * st.scale + vec2(2.f);
            vec2 l = rotate(e.xz() - st.pos, st.rot);
            float top = w.height(st.pos.x, st.pos.y) + (st.kind == S_MONASTERY || st.kind == S_KEEP || st.kind == S_TOWER || st.kind == S_CASTLE_TOWER || st.kind == S_CHAPEL ? 24.f : 12.f);
            if (std::fabs(l.x) < fp.x && std::fabs(l.y) < fp.y && e.y < top) e.y = top + 2.f;
        }
        return e;
    };
    CineShot c;
    if (id == "ch_echo") {
        c = orbit(B, 0.6f, 1.1f, 70, 50, 30, 18, 3); c.dur = 7.5f; c.tod = 19.4f; c.weather = W_CLEAR; c.music = "title";
        c.speaker = "La Pierre d'écho"; c.speakerColor = VIOLET; c.magic = vec3(0.7f, 0.45f, 1.f);
        c.text = "« Si tu entends ceci, le sceau se fissure. Moi, Aldric, j'ai lié mon âme à l'Abîme pour le tenir fermé. »";
        s.push_back(c);
        c = orbit(cave, 2.2f, 2.7f, 40, 26, 16, 8, 2); c.dur = 7.5f; c.tod = 21.f; c.weather = W_CLOUDY; c.fogTint = vec3(0.5f, 0.4f, 0.8f); c.fogMul = 0.4f;
        c.speaker = "La Pierre d'écho"; c.speakerColor = VIOLET; c.magic = vec3(0.7f, 0.45f, 1.f);
        c.text = "« Une âme liée au sceau ne meurt pas. Elle attend. Et quand le sceau cède… elle est rejetée. Comme une épave sur la grève. »";
        s.push_back(c);
        c = CineShot(); c.dur = 7.f; c.tod = 6.2f; c.weather = W_CLEAR; c.wristGlow = true; c.music = "greenhaven";
        c.from = at(beach, 26, -30, 9); c.to = at(beach, 14, -16, 4); c.lookFrom = at(beach, 0, 0, 0.5f); c.lookTo = at(beach, 0, 0, 1.f);
        c.speaker = "La Pierre d'écho"; c.speakerColor = VIOLET;
        c.text = "« Celui qui porte ma marque trouvera les six. Et il se souviendra. »";
        c.title = "CHAPITRE I"; c.subtitle = "La voix dans la pierre";
        s.push_back(c);
    } else if (id == "ch_guardians") {
        c = orbit(H, 3.6f, 4.2f, 110, 70, 50, 34, 18); c.dur = 7.f; c.tod = 17.6f; c.weather = W_CLEAR; c.music = "highcrowns";
        c.speaker = "Frère Alaric"; c.speakerColor = ALARIC;
        c.text = "« Six fragments, sire… Pardon. Six fragments, Naufragé. Six Gardiens qui ont trop longtemps veillé. »";
        s.push_back(c);
        struct G { const char* poi; vec2 def; const char* name; const char* line; int weather; float tod; };
        static const G gs[] = {
            {"root_glade", {640, 1860}, "Le Gardien des Racines", "Dans Elderwood, l'arbre que tu as planté toi-même a pris racine dans la colère.", 1, 16.5f},
            {"graveyard", {900, 2560}, "Le Roi Squelette", "Sous Gravemoor, ton sénéchal tient encore son serment. Sa chair, elle, n'a pas tenu.", 1, 20.5f},
            {"frozen_lake", {1180, 360}, "Le Colosse de Glace", "Au cœur du lac de Givre, l'hiver noir n'a jamais fondu.", 4, 13.f},
            {"ash_nest", {2460, 1960}, "Le Dragon Cendré", "Dans les Ashen Lands, le feu garde un fragment qui ne refroidit pas.", 6, 18.f},
            {"spider_den", {700, 2300}, "La Reine des Araignées", "Son Antre a tissé trois siècles de toile autour de ce qu'elle protège.", 1, 21.f},
            {"titan_cirque", {2650, 1060}, "Le Titan de Pierre", "Dans les cimes, la montagne s'est levée pour garder le dernier.", 0, 11.f},
        };
        for (int i = 0; i < 6; i++) {
            vec2 p = P(gs[i].poi, gs[i].def);
            c = orbit(p, 0.8f + i * 1.1f, 1.3f + i * 1.1f, 80, 55, 34, 24, 4); c.dur = 6.f; c.tod = gs[i].tod; c.weather = gs[i].weather; c.fogMul = 0.25f;
            c.title = gs[i].name; c.subtitle = std::string("Gardien ") + (char)('1' + i) + " sur 6";
            c.text = gs[i].line;
            s.push_back(c);
        }
        c = orbit(L, 4.4f, 4.8f, 44, 30, 26, 18, 3); c.dur = 7.f; c.tod = 18.2f; c.weather = W_CLEAR;
        c.speaker = "Frère Alaric"; c.speakerColor = ALARIC;
        c.text = "« Rapporte-les-moi. La forge de l'Ordre attend depuis trois siècles. Moi aussi. »";
        c.title = "CHAPITRE II"; c.subtitle = "Les six Gardiens";
        s.push_back(c);
    } else if (id == "ch_scar") {
        c = orbit(L, 4.2f, 4.6f, 38, 26, 26, 17, 3); c.dur = 7.f; c.tod = 22.f; c.weather = W_CLEAR; c.music = "shadows"; c.magic = GOLD;
        c.speaker = "Frère Alaric"; c.speakerColor = ALARIC;
        c.text = "« La Couronne-Mère. Entière. Trois siècles que j'attends de la tenir… et de vous la rendre, sire. »";
        s.push_back(c);
        c = orbit(L, 4.5f, 4.75f, 14, 9, 4, 2.6f, 1.6f); c.dur = 7.f; c.tod = 22.1f; c.wristGlow = true;
        c.text = "La cicatrice au menton. Le regard vif. Le novice qui tenait les tenailles la nuit de la Brisure n'a pas pris une ride.";
        s.push_back(c);
        c = orbit(L, 4.8f, 5.0f, 11, 9, 3, 2.6f, 1.8f); c.dur = 8.f; c.tod = 22.2f; c.magic = vec3(0.7f, 0.3f, 1.f); c.shake = 0.08f;
        c.speaker = "Frère Alaric"; c.speakerColor = ALARIC;
        c.text = "« Oui, sire. C'était moi. Malgrath m'a appris que les années se comptent… et qu'on peut cesser de les compter. »";
        s.push_back(c);
        c = orbit(M, 0.4f, 0.9f, 140, 90, 60, 38, 12); c.dur = 8.f; c.tod = 21.4f; c.weather = W_STORM; c.fogTint = vec3(0.55f, 0.3f, 0.85f); c.fogMul = 0.4f; c.sfx = "thunder";
        c.speaker = "Frère Alaric"; c.speakerColor = ALARIC;
        c.text = "« Portez-la à Morneval. Le sceau ne s'ouvre qu'à la main de son roi. Je vous attendrai de l'autre côté. »";
        s.push_back(c);
        c = orbit(L, 5.0f, 5.1f, 16, 13, 5, 4, 1.5f); c.dur = 7.f; c.tod = 22.3f; c.weather = W_CLEAR; c.magic = vec3(0.5f, 0.2f, 0.9f);
        c.text = "Il disparaît dans une gerbe d'ombre. La Couronne pèse dans ta main. Elle est tiède. Elle bat, comme un cœur.";
        c.title = "CHAPITRE III"; c.subtitle = "Le novice à la cicatrice";
        s.push_back(c);
    } else if (id == "ch_corwin") {
        c = orbit(M, 2.5f, 2.9f, 46, 32, 22, 13, 7); c.dur = 7.5f; c.tod = 19.f; c.weather = W_CLOUDY; c.fogTint = vec3(0.45f, 0.4f, 0.6f); c.music = "night";
        c.speaker = "Sire Corwin"; c.speakerColor = CORWIN;
        c.text = "« Sire… Je vous ai reconnu au troisième assaut. Vous tenez toujours l'épée trop haut. »";
        s.push_back(c);
        c = orbit(M, 3.0f, 3.3f, 30, 22, 12, 8, 5); c.dur = 8.5f; c.tod = 19.1f;
        c.speaker = "Sire Corwin"; c.speakerColor = CORWIN;
        c.text = "« J'ai attendu, comme vous l'aviez dit. Écoutez-moi : une couronne reforgée n'est pas une clé pour fermer. C'est une clé pour ouvrir. Alaric le sait. »";
        s.push_back(c);
        c = orbit(A, 0.3f, 0.8f, 90, 55, 40, 22, 2); c.dur = 7.5f; c.tod = 21.8f; c.weather = W_STORM; c.fogTint = vec3(0.6f, 0.25f, 0.85f); c.fogMul = 0.4f; c.shake = 0.15f; c.sfx = "thunder"; c.magic = vec3(0.7f, 0.3f, 1.f);
        c.text = "Corwin pose son épée à tes pieds, et sa forme se défait en cendre claire. Sous Morneval, l'Abîme respire.";
        c.title = "CHAPITRE IV"; c.subtitle = "Le champion qui attendait";
        s.push_back(c);
    } else if (id == "ch_act2") {
        vec2 PA = P("port_azur", {1500, 4300});
        c = orbit(PA, 2.1f, 2.6f, 130, 80, 55, 36, 8); c.dur = 7.5f; c.tod = 17.9f; c.weather = W_CLEAR; c.music = "greenhaven";
        c.speaker = "Kaede"; c.speakerColor = vec3(0.6f, 0.85f, 1.f);
        c.text = "« Le septième ne dort pas à Aelindor. Retrouve-moi à Port-Azur, avant que la marée tourne. »";
        s.push_back(c);
        c = orbit(P("phare", {2150, 4420}), 3.6f, 4.0f, 70, 46, 28, 20, 12); c.dur = 7.f; c.tod = 18.8f; c.weather = W_CLEAR;
        c.text = "Le sud n'a pas de fin : une côte de sable blanc, de lagons turquoise, de palmiers. Et, plus loin, un désert d'ambre qui brûle sous un soleil sans pitié.";
        s.push_back(c);
        c = orbit(P("solmar", {3850, 2700}), 1.0f, 1.5f, 150, 100, 60, 40, 8); c.dur = 7.5f; c.tod = 16.5f; c.weather = W_CLEAR; c.music = "ashen";
        c.text = "Sept joyaux. Six ont forgé une Couronne. Le septième, personne n'a jamais dit où il était. Ce que l'on sait : il a faim de silence.";
        c.title = "ACTE II"; c.subtitle = "L'héritage du septième";
        s.push_back(c);
    } else if (id == "ch_mareval") {
        vec2 MV = P("storm_deck", {1700, 4290});
        c = orbit(MV, 0.6f, 1.1f, 60, 38, 24, 14, 4); c.dur = 7.f; c.tod = 19.4f; c.weather = W_STORM; c.fogTint = vec3(0.4f, 0.6f, 0.7f); c.fogMul = 0.4f; c.music = "night"; c.sfx = "thunder";
        c.speaker = "Le Capitaine Mareval"; c.speakerColor = vec3(0.45f, 0.95f, 0.85f);
        c.text = "« J'ai vendu trente hommes pour cette carte. La mer en veut trente-et-un. Tu m'as rendu un service. »";
        s.push_back(c);
        c = orbit(P("shirokawa", {2500, 3850}), 3.2f, 3.8f, 110, 70, 50, 30, 14); c.dur = 7.5f; c.tod = 6.6f; c.weather = W_CLEAR; c.music = "greenhaven";
        c.text = "La carte montre un temple sur un lagon, des toits relevés comme des ailes, et, dessous, un passage qui descend plus bas que la mer.";
        c.title = "ACTE II · II"; c.subtitle = "La dette du capitaine";
        s.push_back(c);
    } else if (id == "ch_sun_king") {
        vec2 ST = P("sun_temple", {4080, 3560});
        c = orbit(ST, 0.2f, 0.7f, 90, 56, 40, 26, 10); c.dur = 7.5f; c.tod = 13.f; c.weather = W_CLEAR; c.music = "ashen"; c.magic = vec3(1.f, 0.8f, 0.3f);
        c.speaker = "Sekhmar"; c.speakerColor = vec3(1.f, 0.82f, 0.35f);
        c.text = "« J'ai demandé au soleil de ne jamais se coucher sur mon royaume. Il a obéi. J'aurais dû préciser : sur mes sujets. »";
        s.push_back(c);
        c = orbit(ST, 1.9f, 2.4f, 50, 34, 22, 12, 8); c.dur = 7.f; c.tod = 18.4f; c.weather = W_CLEAR; c.magic = vec3(1.f, 0.7f, 0.2f);
        c.speaker = "Sekhmar"; c.speakerColor = vec3(1.f, 0.82f, 0.35f);
        c.text = "« Prends le sceau. Dis à Zahra… dis-lui que le sable a la mémoire longue, mais pas éternelle. Que je suis désolé. »";
        c.title = "ACTE II · III"; c.subtitle = "Le soleil qui ne se couche pas";
        s.push_back(c);
    } else if (id == "ch_sanctum") {
        vec2 SH = P("shirokawa", {2500, 3850});
        c = orbit(SH, 4.2f, 4.7f, 50, 32, 28, 16, 8); c.dur = 7.f; c.tod = 21.2f; c.weather = W_CLEAR; c.music = "title"; c.magic = vec3(0.5f, 0.9f, 1.f);
        c.speaker = "Maître Hanzo"; c.speakerColor = vec3(0.95f, 0.9f, 0.8f);
        c.text = "« L'eau d'abord. Le feu ensuite. Ce que la mer a pris, le soleil le rend. Écoute. »";
        s.push_back(c);
        c = orbit(SH, 4.7f, 5.0f, 24, 14, 10, 5, 2); c.dur = 7.5f; c.tod = 21.4f; c.magic = vec3(0.8f, 1.f, 1.f); c.shake = 0.08f; c.sfx = "thunder";
        c.text = "La porte s'ouvre sans un bruit. En fait, plus aucun bruit : plus la mer, plus le vent, plus ton souffle. Seulement un escalier qui descend dans le blanc.";
        c.title = "ACTE II · IV"; c.subtitle = "L'eau et le feu";
        s.push_back(c);
    } else if (id == "ch_legacy") {
        vec2 SH = P("shirokawa", {2500, 3850});
        c = orbit(SH, 1.0f, 1.5f, 70, 44, 32, 20, 6); c.dur = 7.5f; c.tod = 6.1f; c.weather = W_CLEAR; c.music = "greenhaven";
        c.speaker = "Kaede"; c.speakerColor = vec3(0.6f, 0.85f, 1.f);
        c.text = "« On ne détruit pas le silence. On lui donne un endroit où se coucher. La mer est assez grande. »";
        s.push_back(c);
        c = orbit(P("port_azur", {1500, 4300}), 2.4f, 2.9f, 90, 60, 40, 26, 6); c.dur = 8.f; c.tod = 17.6f; c.weather = W_CLEAR;
        c.text = "Les voiles de Port-Azur reprennent la mer. À Solmar, on grave de nouveaux noms sur le mur. Le monde est un peu plus fort, et un peu plus bruyant. C'est bien.";
        c.title = "ACTE II · FIN"; c.subtitle = "L'héritage";
        s.push_back(c);
    } else if (id == "ending_seal" || id == "ending_break") {
        bool seal = id == "ending_seal";
        c = orbit(A, 1.0f, 1.5f, 30, 18, 12, 6, 2); c.dur = 8.f; c.tod = 22.f; c.weather = W_STORM; c.fogTint = vec3(0.6f, 0.25f, 0.85f); c.fogMul = 0.4f; c.music = "title";
        c.magic = seal ? GOLD : vec3(1.f, 0.4f, 0.2f); c.shake = seal ? 0.05f : 0.25f;
        c.text = seal ? "Aldric pose la Couronne-Mère sur sa tête. La lumière d'or emplit l'Abîme, et le sceau se referme autour de lui."
                      : "Aldric brise la Couronne-Mère sur la pierre de l'Abîme. Les six joyaux s'éteignent un à un. Il n'y a plus de clé.";
        s.push_back(c);
        c = orbit(A, 1.6f, 1.8f, 14, 10, 4, 3, 1.6f); c.dur = 7.5f; c.tod = 22.1f; c.magic = vec3(0.5f, 0.2f, 0.9f);
        c.speaker = "Frère Alaric"; c.speakerColor = ALARIC;
        c.text = seal ? "« Non… pas encore… pas après trois siècles… » Le sceau l'emporte avec son maître."
                      : "« Tu as tué ton propre royaume ! » Trois siècles le rattrapent d'un coup. Il n'en reste que de la poussière.";
        s.push_back(c);
        if (seal) {
            c = CineShot(); c.dur = 8.f; c.tod = 6.4f; c.weather = W_CLEAR; c.music = "greenhaven";
            c.from = at(beach, 30, -34, 10); c.to = at(beach, 12, -14, 4); c.lookFrom = at(beach, 0, 0, 0.5f); c.lookTo = at(beach, 0, 0, 1.f);
            c.text = "À Brindlemere, Maëlle trouve sur la grève une épée plantée dans le sable. Elle la reconnaît : c'est celle des tapisseries.";
            s.push_back(c);
            c = orbit(H, 3.8f, 4.3f, 120, 80, 55, 40, 18); c.dur = 8.f; c.tod = 7.f;
            c.text = "Les cloches de l'Ordre sonnent pour un roi qu'aucun vivant n'a connu. Sous Morneval, quelqu'un attend. Encore.";
            c.title = "ÉPILOGUE"; c.subtitle = "Le Roi du sceau";
            s.push_back(c);
        } else {
            vec2 home = B;
            if (const Poi* d = w.poiById("player_base")) home = d->pos;
            c = orbit(P("root_glade", {640, 1860}), 0.5f, 1.0f, 90, 60, 40, 26, 4); c.dur = 7.5f; c.tod = 6.8f; c.weather = W_CLEAR; c.music = "greenhaven";
            c.text = "Les Gardiens s'endorment pour de bon. Les créatures de la nuit, elles, ne partiront pas : le royaume devra se défendre seul.";
            s.push_back(c);
            c = orbit(home, 2.2f, 2.8f, 60, 40, 26, 16, 3); c.dur = 8.f; c.tod = 18.6f;
            c.text = "Un feu, un mur, des gens qui restent. C'est ainsi que commencent les couronnes. Celle-ci sera la tienne, et elle ne sera faite que de pierre.";
            c.title = "ÉPILOGUE"; c.subtitle = "Le Roi sans couronne";
            s.push_back(c);
        }
    }
    for (CineShot& k : s) { k.from = safe(k.from); k.to = safe(k.to); }
    return s;
}

// ------------------------------------------------------------------------------------------------ flow
void Game::queueChapter(const std::string& id) {
    pendingChapter = id;
    pendingChapterT = 1.2f;
    AGR_LOG("story: chapter '%s' queued", id.c_str());
}

void Game::startChapter(const std::string& id) {
    if (id == "ending_choice") { endingChoice = true; endingChoiceT = 0; play("boss_intro", 0.6f); return; }
    if (id == "prologue") { startPrologue(); return; }
    startShots(chapterShots(id), id);
}

// called by endCinematicShots
void Game::onChapterEnd(const std::string& id) {
    if (id.rfind("ch_", 0) == 0) storyFlags.insert("seen_" + id);
    if (id == "ch_guardians" && !questActive("m4") && !questsDone.count("m4")) startQuest("m4");
    if (id == "ch_scar") {
        // Alaric is gone: his NPC vanishes, the road to Morneval opens
        storyFlags.insert("alaric_gone");
        int ai = npcIndex("alaric");
        reg.each<Character>([&](Entity e, Character& c) { if (c.npcDef == ai && ai >= 0) { if (interactTarget == e) interactTarget = NullEntity; reg.destroyLater(e); } });
        if (!questActive("m6") && !questsDone.count("m6")) startQuest("m6");
    }
    if (id == "ch_legacy") { storyFlags.insert("act2_done"); notify("Acte II terminé : l'héritage du septième est entre les vagues.", vec3(0.7f, 0.95f, 1.f), "i_check"); }
    if (id == "ending_seal" || id == "ending_break") {
        storyFlags.insert("seen_ending");
        creditsT = 0;
        fadeTransition([this] { state = GameState::Credits; stateT = 0; creditsT = 0; });
    }
}

void Game::chooseEnding(bool seal) {
    endingChoice = false;
    storyFlags.insert(seal ? "ending_seal" : "ending_break");
    AGR_LOG("story: ending chosen: %s", seal ? "seal" : "break");
    startShots(chapterShots(seal ? "ending_seal" : "ending_break"), seal ? "ending_seal" : "ending_break");
}

void Game::updateStory(float dt) {
    // Alaric gone: a quest step that asks to talk to him is answered by what happened at the forge
    if (storyFlags.count("alaric_gone"))
        for (auto& q : questLog)
            if (!q.done && q.step < (int)quests()[q.idx].steps.size()) {
                const QuestStep& st = quests()[q.idx].steps[q.step];
                if (st.kind == StepKind::Talk && st.target == "alaric") { advanceQuests(StepKind::Talk, "alaric", 1); break; }
            }
    if (pendingChapter.empty()) return;
    bool busy = menu != Menu::None || dialog.open || cinematicT > 0 || cine.active || transitionT >= 0 || bossEnt != NullEntity || endingChoice || driving >= 0;
    if (busy) { pendingChapterT = std::max(pendingChapterT, 0.6f); return; }
    pendingChapterT -= dt;
    if (pendingChapterT > 0) return;
    std::string id = pendingChapter;
    pendingChapter.clear();
    startChapter(id);
}

Rect Game::endingChoiceRect(int i) const {
    float w = std::min(ui.W * 0.4f, 460.f), h = 170.f, gap = 30.f;
    float x0 = ui.W * 0.5f - w - gap * 0.5f;
    return {x0 + i * (w + gap), ui.H * 0.5f - 10.f, w, h};
}

void Game::drawEndingChoice(float dt) {
    if (!endingChoice) return;
    UI& u = ui;
    endingChoiceT += dt;
    float a = clampf(endingChoiceT / 1.2f, 0.f, 1.f);
    u.rect({0, 0, u.W, u.H}, vec4(0.03f, 0.01f, 0.06f, 0.94f * a));
    const vec4 GOLDT = rgba(0xf4d58a), PALE = rgba(0xf4e7c4), DIM = rgba(0xb0a48c);
    u.text("Le choix d'Aldric", u.W / 2, u.H * 0.16f, 46, vec4(GOLDT.x, GOLDT.y, GOLDT.z, a), F_TITLE, A_CENTER, 0, true);
    u.text("Malgrath est à terre, mais l'Abîme reste ouvert. La Couronne-Mère brûle dans ta main. Elle peut refermer le sceau, à condition qu'une âme le garde. Ou elle peut cesser d'exister.",
           u.W / 2, u.H * 0.16f + 64, 20, vec4(PALE.x, PALE.y, PALE.z, a), F_BODY, A_CENTER, std::min(u.W * 0.8f, 900.f), true);
    static const char* titles[2] = {"Reprendre ta place dans le sceau", "Briser la Couronne à jamais"};
    static const char* descs[2] = {"Tu redeviens le sceau. Le royaume vivra en paix. Toi, tu attendras, seul, sous Morneval.",
                                   "Plus de couronne, plus de clé. Le royaume devra se défendre seul. Toi, tu vivras parmi les tiens."};
    for (int i = 0; i < 2; i++) {
        Rect r = endingChoiceRect(i);
        u.nine("panel_plain", r, vec4(1, 1, 1, 0.9f * a), 0.5f);
        u.text(titles[i], r.x + r.w / 2, r.y + 16, 24, vec4(GOLDT.x, GOLDT.y, GOLDT.z, a), F_BOLD, A_CENTER, r.w - 24);
        u.text(descs[i], r.x + r.w / 2, r.y + 60, 18, vec4(PALE.x, PALE.y, PALE.z, a * 0.9f), F_BODY, A_CENTER, r.w - 30);
        if (endingChoiceT > 1.5f && u.button(r)) { play("ui_confirm", 0.8f); chooseEnding(i == 0); return; }
    }
}

// Objectives tab: the chapters seen so far
float Game::drawStoryPanel(Rect r) {
    UI& u = ui;
    const vec4 GOLDT = rgba(0xf4d58a), PALE = rgba(0xf4e7c4), DIM = rgba(0xb0a48c);
    float y = r.y;
    u.text("L'histoire", r.x, y, 22, GOLDT, F_SERIF); y += 34;
    for (const ChapterDef& ch : chapters()) {
        bool seen = ch.id == "prologue" ? storyFlags.count("prologue_seen") > 0 : (ch.id == "ending" ? storyFlags.count("seen_ending") > 0 : storyFlags.count("seen_" + ch.id) > 0);
        if (!seen) { u.text(std::string(ch.title).substr(0, std::string(ch.title).find(' ')) + " · ???", r.x, y, 16, DIM, F_BOLD); y += 26; continue; }
        u.text(ch.title, r.x, y, 17, PALE, F_BOLD);
        std::string sum = ch.summary;
        if (ch.id == "ending") sum += storyFlags.count("ending_seal") ? " Il a repris sa place dans le sceau." : " Il a brisé la Couronne, et choisi de vivre.";
        float th = u.textHeight(sum, 15, F_BODY, r.w - 120);
        u.text(sum, r.x, y + 24, 15, DIM, F_BODY, A_LEFT, r.w - 120);
        std::string cid = ch.id == "ending" ? (storyFlags.count("ending_seal") ? "ending_seal" : "ending_break") : ch.id;
        if (u.fancyButton({r.x + r.w - 104, y - 2, 100, 34}, "Revoir", true, "button_dark", 15)) { menu = Menu::None; replayingChapter = true; startChapter(cid); }
        y += 30 + th + 10;
    }
    return y - r.y;
}

}  // namespace lk
