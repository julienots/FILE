#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

extern const char* CTL_ICON[CTL_COUNT];
Rect ctlRect(const Game& g, int id);

static const vec4 INK = rgba(0x3a2a1a), GOLDT = rgba(0xf6dc9a), PALE = rgba(0xe8dcc0), DIM = rgba(0xb0a48c);

// ============================================================================================ helpers
static bool slider(UI& u, Rect r, const std::string& label, float& v, float lo, float hi, const char* fmt = "%.0f %%", float mul = 100.f) {
    u.text(label, r.x, r.y, 19, PALE, F_BODY);
    Rect bar{r.x, r.y + 30, r.w - 70, 14};
    u.rect(bar, vec4(0, 0, 0, 0.45f));
    float f = saturate((v - lo) / (hi - lo));
    u.rect({bar.x, bar.y, bar.w * f, bar.h}, rgba(0xc9a45c));
    u.circle({bar.x + bar.w * f, bar.y + 7}, 13, rgba(0xf6dc9a), 16);
    char b[32]; snprintf(b, sizeof b, fmt, v * mul);
    u.text(b, r.x + r.w - 60, r.y + 24, 18, GOLDT, F_BOLD);
    bool changed = false;
    for (auto& fg : u.fingers) {
        if (fg.down && Rect{bar.x - 10, bar.y - 18, bar.w + 20, 50}.contains(fg.sx, fg.sy)) {
            fg.consumed = true;
            float nv = lo + saturate((fg.x - bar.x) / bar.w) * (hi - lo);
            if (std::fabs(nv - v) > 1e-4f) { v = nv; changed = true; }
        }
    }
    return changed;
}

static bool toggle(UI& u, Rect r, const std::string& label, bool& v) {
    u.text(label, r.x, r.y + 8, 19, PALE, F_BODY);
    Rect sw{r.x + r.w - 76, r.y + 4, 66, 32};
    u.nine("bar_frame", sw, vec4(1, 1, 1, 1), 1.f);
    u.circle({v ? sw.x + sw.w - 16 : sw.x + 16, sw.y + 16}, 12, v ? rgba(0x7ac85a) : rgba(0x8a7a6a), 16);
    if (u.button({r.x, r.y, r.w, 40})) { v = !v; return true; }
    return false;
}

static Rect journalFrame(UI& u) { return {44, 56, u.W - 88, u.H - 84}; }

bool Game::itemIcon(const std::string& id, Rect r, float a) {
    const ItemDef* d = item(id);
    if (!d) return false;
    auto it = iconUV.find(id);
    if (iconsReady && it != iconUV.end()) { ui.image(iconRT.color.id, r, it->second, vec4(1, 1, 1, a)); return true; }
    std::string ic = d->icon.empty() ? "i_star" : d->icon;
    vec4 tint{1, 1, 1, a};
    if (d->kind == ItemKind::Head || d->kind == ItemKind::Chest || d->kind == ItemKind::Hands || d->kind == ItemKind::Feet) tint = vec4(lerp(vec3(1), d->color * 1.4f, 0.5f), a);
    ui.sprite(ic, r.inset(r.w * 0.06f), tint);
    return true;
}

// Item icons for weapons & shields are rendered from their actual 3D models.
void Game::renderItemIcons() {
    if (iconsReady) return;
    const int cell = 128, cols = 8;
    if (!iconRT.fbo) iconRT.create(cell * cols, cell * cols, GL_RGBA8, true);
    std::vector<const ItemDef*> list;
    for (auto& d : allItems()) if (d.kind == ItemKind::Weapon || d.kind == ItemKind::Shield || d.kind == ItemKind::Tool) list.push_back(&d);
    Environment e; e.sunDir = normalize(vec3(-0.4f, 0.8f, 0.6f)); e.sunIntensity = 3.2f; e.sunColor = srgb(0xfff2dc);
    e.skyAmbient = srgb(0x9ab0d0) * 1.1f; e.groundAmbient = srgb(0x5a4a3a) * 0.8f; e.fogDensity = 0; e.exposure = 1.1f; e.saturation = 1.1f; e.contrast = 1.05f;
    RenderTarget cellRT; cellRT.create(cell, cell, GL_RGBA8, false);
    // render each icon into its own small target then copy into the atlas
    for (size_t i = 0; i < list.size() && i < (size_t)(cols * cols); i++) {
        const ItemDef* d = list[i];
        float tip = 0.8f;
        auto mesh = weaponMesh(d->weapon, d->tier, d->weapon == WeaponType::Shield ? d->color : elementColor(d->element), &tip);
        Camera c;
        float len = d->weapon == WeaponType::Shield ? 0.7f : std::max(0.5f, tip + 0.3f);
        vec3 center = d->weapon == WeaponType::Shield ? vec3(0.08f, 0, 0) : vec3(0, 0, tip * 0.42f);
        quat q = d->weapon == WeaponType::Shield ? quat::axisAngle({0, 1, 0}, -PI / 2 + 0.35f) : quat::axisAngle({1, 0, 0}, -PI / 2) * quat::axisAngle({0, 1, 0}, 0.3f);
        c.target = vec3(0); c.pos = vec3(0.f, 0.1f, len * 1.35f); c.fovY = 40 * DEG; c.zNear = 0.05f; c.zFar = 20;
        c.update(1.f);
        DrawItem di; di.mesh = mesh.get();
        di.model = mat4::rotate(quat::axisAngle({0, 0, 1}, -PI / 4) * q) * mat4::translate(-center);
        di.outline = true; di.outlineColor = vec4(0.05f, 0.03f, 0.02f, 1); di.outlineWidth = 0.004f * len;
        renderer.renderToTarget(cellRT, c, e, {di}, vec4(0, 0, 0, 0));
        int cx = (int)(i % cols), cy = (int)(i / cols);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, cellRT.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, iconRT.fbo);
        glBlitFramebuffer(0, 0, cell, cell, cx * cell, cy * cell, (cx + 1) * cell, (cy + 1) * cell, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        float s = 1.f / cols;
        iconUV[d->id] = vec4(cx * s, (cy + 1) * s, (cx + 1) * s, cy * s);  // flipped (GL origin bottom-left)
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    cellRT.destroy();
    iconsReady = true;
}

// Close-up portrait of an NPC (rendered with the dialogue depth of field look).
void Game::renderPortrait(int npc) {
    if (portraitNpc == npc && portraitRT.fbo) return;
    if (!portraitRT.fbo) portraitRT.create(256, 256, GL_RGBA8, true);
    portraitNpc = npc;
    Visual v;
    HumanoidLook L;
    if (npc >= 0) L = npcs()[npc].look;
    else { L.c1 = hexColor(0x6a4a3a); L.seed = (uint32_t)(-npc) * 31; L.hairCol = hexColor(0x5a3a22); }
    setupVisualHumanoid(v, L, WeaponType::None, WeaponType::None, 1.f);
    v.anim.update(0.016f);
    computeWorld(*v.anim.skeleton, v.anim.pose, v.pw);
    Camera c;
    vec3 head = v.pw.wpos[H_HEAD] + vec3(0, 0.08f * L.headSize, 0);
    c.target = head; c.pos = head + vec3(0.18f, 0.03f, 0.62f); c.fovY = 30 * DEG; c.zNear = 0.05f; c.zFar = 10; c.update(1.f);
    Environment e; e.sunDir = normalize(vec3(0.5f, 0.6f, 0.7f)); e.sunIntensity = 2.6f; e.sunColor = srgb(0xffe8c8);
    e.skyAmbient = srgb(0x8a9ac0); e.groundAmbient = srgb(0x5a4030) * 0.7f; e.fogDensity = 0; e.exposure = 1.05f; e.saturation = 1.05f; e.contrast = 1.05f;
    DrawItem d; d.mesh = v.mesh.get(); d.flags = LIT_SKINNED | (v.albedo ? LIT_TEXTURED : 0); d.albedo = v.albedo; d.bones = v.pw.skin[0].m; d.boneCount = v.anim.skeleton->size(); d.fx.z = 0.8f;
    renderer.renderToTarget(portraitRT, c, e, {d}, vec4(0.16f, 0.12f, 0.1f, 1));
}

// ============================================================================================ dialogue box
void Game::drawDialog() {
    UI& u = ui;
    u.consumeAll();
    float W_ = u.W, H_ = u.H;
    Rect box{W_ * 0.08f, H_ - 250, W_ * 0.84f, 230};
    u.panel(box, "", true);
    renderPortrait(dialog.npc);
    Rect pr{box.x + 22, box.y + 22, 150, 150};
    u.nine("slot_sel", {pr.x - 6, pr.y - 6, pr.w + 12, pr.h + 12}, vec4(1, 1, 1, 1), 0.6f);
    if (portraitRT.fbo) u.image(portraitRT.color.id, pr, vec4(0, 1, 1, 0));
    u.text(dialog.speaker, box.x + 196, box.y + 18, 30, rgba(0x5a2a14), F_SERIF, A_LEFT);
    u.text(dialog.title, box.x + 196, box.y + 54, 18, rgba(0x7a5a3a), F_BODY, A_LEFT);
    std::string line = dialog.line < (int)dialog.lines.size() ? dialog.lines[dialog.line] : "";
    dialog.reveal += u.dt * 55.f;
    size_t n = std::min(line.size(), (size_t)dialog.reveal);
    while (n < line.size() && (line[n] & 0xC0) == 0x80) n++;  // keep UTF-8 intact
    float textW = box.w - 196 - (dialog.line + 1 >= (int)dialog.lines.size() ? 330 : 40);
    u.text(line.substr(0, n), box.x + 196, box.y + 86, 22, INK, F_BODY, A_LEFT, textW);
    bool lastLine = dialog.line + 1 >= (int)dialog.lines.size();
    // tap to advance / skip typewriter
    for (auto& f : u.fingers) if (f.released && box.contains(f.x, f.y)) {
        if (n < line.size()) dialog.reveal = 1e9f;
        else if (!lastLine) { dialog.line++; dialog.reveal = 0; }
    }
    if (!lastLine) { u.text("▸", box.x + box.w - 40, box.y + box.h - 44, 28, rgba(0x7a5a3a, 0.5f + 0.5f * std::sin(realTime * 5)), F_BOLD); return; }
    // choices on the right
    float y = box.y + 18;
    float cw = 310;
    float ch = std::min(44.f, (box.h - 30) / std::max(1, (int)dialog.choices.size()) - 6);
    std::vector<DialogState::Choice> ch2 = dialog.choices;
    for (auto& c : ch2) {
        Rect br{box.x + box.w - cw - 20, y, cw, ch};
        bool special = c.action.rfind("offer:", 0) == 0 || c.action.rfind("deliver:", 0) == 0 || c.action.rfind("accept:", 0) == 0;
        if (u.fancyButton(br, c.text, true, special ? "button" : "button_dark", std::min(19.f, ch * 0.45f))) { dialogAction(c.action); return; }
        y += ch + 6;
    }
}

// ============================================================================================ splash / title / credits
void Game::drawSplash(float dt) {
    UI& u = ui;
    float t = stateT, W_ = u.W, H_ = u.H;
    if (t < 0.05f) audio.playMusic("studio", 0.2f);
    u.rect({0, 0, W_, H_}, vec4(0.02f, 0.015f, 0.02f, t < 6.2f ? 1.f : saturate(1 - (t - 6.2f) * 1.5f)));
    vec2 c{W_ / 2, H_ * 0.42f};
    // emblem: a faceted essence drop inside two rings, drawn with particles of light
    float a1 = saturate((t - 0.3f) * 1.2f) * saturate((5.8f - t) * 1.5f);
    float spin = t * 0.6f;
    u.ring(c, 92, 96, -PI / 2, -PI / 2 + TAU * saturate(t * 0.6f), rgba(0xc9a45c, a1), 64);
    u.ring(c, 78, 80, PI / 2, PI / 2 + TAU * saturate((t - 0.4f) * 0.6f), rgba(0xf6dc9a, a1 * 0.7f), 64);
    for (int i = 0; i < 6; i++) {
        float a = spin + i * TAU / 6;
        u.circle(c + vec2(std::cos(a), std::sin(a)) * 87.f, 4, rgba(0xf6dc9a, a1), 8);
    }
    // the drop
    float s = 1.f + 0.03f * std::sin(t * 3);
    std::vector<vec2> drop;
    for (int i = 0; i < 24; i++) { float a = i * TAU / 24; float rr = 34 + 18 * std::max(0.f, -std::sin(a)) * std::max(0.f, -std::sin(a)); drop.push_back(c + vec2(std::cos(a) * 30, std::sin(a) * rr - 6) * s); }
    for (int i = 0; i < 24; i++) { vec2 p0 = drop[i]; (void)p0; }
    u.circle(c + vec2(0, 6), 30 * s, rgba(0x3fe0c8, a1 * 0.9f), 24);
    u.ring(c + vec2(0, 6), 30 * s, 33 * s, 0, TAU, rgba(0xf6dc9a, a1), 24);
    u.circle(c + vec2(0, -26), 14 * s, rgba(0x3fe0c8, a1 * 0.9f), 16);
    u.circle(c + vec2(-9, -2), 7, rgba(0xffffff, a1 * 0.7f), 10);
    u.sprite("glow", {c.x - 160, c.y - 160, 320, 320}, rgba(0x3fe0c8, a1 * 0.25f));
    // name, letter by letter, with a gleam sweeping across
    std::string name = "SUPERESSENCE";
    float tw = u.textWidth(name, 64, F_SERIF);
    float x = W_ / 2 - tw / 2;
    size_t i = 0; int k = 0;
    while (i < name.size()) {
        size_t s0 = i; utf8Next(name, i);
        std::string ch = name.substr(s0, i - s0);
        float la = saturate((t - 1.0f - k * 0.09f) * 2.5f) * saturate((5.8f - t) * 1.5f);
        float gleam = std::exp(-std::pow((t - 2.6f) * 6.f - k * 0.5f, 2.f));
        vec4 col = lerp(rgba(0xc9a45c, la), rgba(0xfff6dc, la), gleam);
        u.text(ch, x, H_ * 0.62f + (1 - la) * 20, 64, col, F_SERIF, A_LEFT, 0, true);
        x += u.textWidth(ch, 64, F_SERIF);
        k++;
    }
    float a2 = saturate((t - 2.4f) * 1.5f) * saturate((5.8f - t) * 1.5f);
    u.text("S T U D I O", W_ / 2, H_ * 0.62f + 80, 24, rgba(0xe8dcc0, a2), F_SERIF, A_CENTER, 0, true);
    u.text("présente", W_ / 2, H_ * 0.62f + 120, 20, rgba(0xb0a48c, a2), F_BODY, A_CENTER);
    float a3 = saturate((t - 3.6f) * 2.f) * saturate((5.8f - t) * 1.5f);
    u.text("Propulsé par AGR ENGINE", W_ / 2, H_ - 50, 16, rgba(0x8a8070, a3), F_BOLD, A_CENTER);
    bool skip = false;
    for (auto& f : u.fingers) if (f.released) skip = true;
    if (t > 7.f || (skip && t > 0.8f)) { state = GameState::Title; stateT = 0; audio.playMusic("title", 2.f); }
    (void)dt;
}

void Game::newGame() {
    reg.clear();
    player = mountEnt = bossEnt = lockTarget = interactTarget = NullEntity;
    riding = false;
    level = 1; xp = 0; gold = 60; skillPoints = 1;
    inventory.clear(); for (auto& e : equipment) e = ItemStack();
    skills.clear(); knownSpells.clear(); ownedMounts.clear(); activeMount = -1;
    questLog.clear(); questsDone.clear(); discovered.clear(); defeatedBosses.clear(); bestiary.clear(); loreFound.clear(); relicsFound.clear(); talkedTo.clear();
    killedUnique.clear(); openedChests.clear(); clearedSpawns.clear(); dungeonsDone.clear(); world.harvested.clear();
    for (auto& r : reputation) r = 0;
    fog.assign(128 * 128, 0);
    nodeEntities.clear(); spawnedChunks.clear(); chunkSpawns.clear(); villageNpcsSpawned.clear();
    dungeon = DungeonState();
    gameTime = 0; playSeconds = 0; timeOfDay = 7.5f; day = 1;
    equippedSpells[0] = equippedSpells[1] = equippedSpells[2] = -1;
    // washed ashore south of Brindlemere, near the river mouth
    const Poi* b = world.poiById("brindlemere");
    vec3 start{b->pos.x + 20.f, 0, b->pos.y + 60.f};
    start.y = world.groundHeight(start.x, start.z);
    spawnPlayer(start);
    respawnPos = start;
    equipment[SL_WEAPON] = {"sword_rusty", 1, Rarity::Common, 0, 1, false};
    equipment[SL_CHEST] = {"leather_chest", 1, Rarity::Common, 0, 2, false};
    addItem("potion_minor", 3, Rarity::Count, false);
    addItem("bread", 2, Rarity::Count, false);
    addItem("tool_pickaxe", 1, Rarity::Count, false);
    addItem("tool_woodaxe", 1, Rarity::Count, false);
    addItem("shield_wood", 1, Rarity::Count, false);
    learnSpell("fireball");
    recomputeStats();
    rebuildPlayerVisual();
    pc().hp = pc().maxHp; mp = maxMp; stamina = maxStamina;
    pc().yaw = PI;
    startQuest("m1");
    camFocus = start; camYaw = 0.6f;
    saveSlot = 0;
}

void Game::startPlaying() {
    state = GameState::Playing; stateT = 0; menu = Menu::None;
    lastRegion = -1;
    renderItemIcons();
    toastHint = "Joystick à gauche pour marcher · Bouton épée : attaquer (maintenir pour charger) · Glisser à droite pour tourner la caméra.";
    hintT = 0;
}

void Game::drawTitle(float dt) {
    UI& u = ui;
    float W_ = u.W, H_ = u.H, t = stateT;
    if (audio.currentMusic().empty()) audio.playMusic("title", 2.f);
    float a = saturate(t * 0.8f);
    u.rectGrad({0, 0, W_, H_ * 0.45f}, vec4(0, 0, 0, 0.55f * a), vec4(0, 0, 0, 0));
    u.rectGrad({0, H_ * 0.6f, W_, H_ * 0.4f}, vec4(0, 0, 0, 0), vec4(0, 0, 0, 0.7f * a));
    // logo
    float ly = H_ * 0.13f + (1 - easeOut(saturate(t * 0.7f))) * 30;
    u.sprite("glow", {W_ / 2 - 360, ly - 160, 720, 380}, rgba(0xf6dc9a, 0.12f * a));
    u.text("LOST KINGDOM", W_ / 2 + 3, ly + 4, 104, rgba(0x000000, 0.5f * a), F_TITLE, A_CENTER);
    u.text("LOST KINGDOM", W_ / 2, ly, 104, rgba(0xf6dc9a, a), F_TITLE, A_CENTER, 0, true);
    u.sprite("divider", {W_ / 2 - 260, ly + 128, 520, 26}, vec4(1, 1, 1, a));
    u.text("A world waiting to be discovered.", W_ / 2, ly + 160, 26, rgba(0xe8dcc0, a), F_SERIF, A_CENTER, 0, true);
    // menu
    bool hasSave = platform::fileExists(platform::writableDir() + "/save_99.lks") || platform::fileExists(platform::writableDir() + "/save_0.lks");
    float bw = 360, bh = 62, by = H_ * 0.52f;
    float ba = saturate((t - 0.8f) * 1.5f);
    auto btn = [&](const std::string& label, bool en) { bool r = u.fancyButton({W_ / 2 - bw / 2, by, bw, bh}, label, en, "button", 26) && ba > 0.5f; by += bh + 12; return r; };
    if (menu == Menu::Settings || menu == Menu::Saves) { drawMenus(dt); return; }
    if (hasSave && btn("Continuer", true)) {
        int slot = platform::fileExists(platform::writableDir() + "/save_99.lks") ? 99 : 0;
        if (loadGame(slot)) { play("ui_open"); startPlaying(); }
    }
    if (btn("Nouvelle partie", true)) { play("ui_open"); newGame(); startPlaying(); regionBanner = "Greenhaven"; regionBannerSub = "Les plaines verdoyantes"; regionBannerT = 5; }
    if (btn("Charger", hasSave)) { menu = Menu::Saves; play("ui_open"); }
    if (btn("Paramètres", true)) { menu = Menu::Settings; play("ui_open"); }
    if (btn("Crédits", true)) { state = GameState::Credits; stateT = 0; creditsT = 0; play("ui_open"); }
    u.text("Created by SUPERESSENCE STUDIO", W_ / 2, H_ - 44, 18, rgba(0xc8bca0, ba), F_SERIF, A_CENTER, 0, true);
    u.text("v1.6.0 · AGR Engine", W_ - 16, H_ - 30, 14, rgba(0x8a8070, ba), F_BOLD, A_RIGHT);
}

void Game::drawCredits(float dt) {
    UI& u = ui;
    float W_ = u.W, H_ = u.H;
    if (stateT < 0.05f) audio.playMusic("credits", 2.f);
    creditsT += dt;
    u.rect({0, 0, W_, H_}, vec4(0, 0, 0, 0.72f));
    static const char* lines[] = {
        "#LOST KINGDOM", "", "A world waiting to be discovered.", "", "", "@Une production", "#SUPERESSENCE STUDIO", "", "",
        "@Direction artistique & monde", "Superessence Studio", "", "@Moteur", "AGR Engine — rendu OpenGL ES 3, animation, audio, ECS", "",
        "@Programmation", "Systèmes de jeu, IA, combat, quêtes, streaming du monde", "", "@Musique & sons", "Compositions et effets synthétisés pour Lost Kingdom", "",
        "@Polices", "Cinzel, Cinzel Decorative, Alegreya Sans — SIL Open Font License", "", "@Bibliothèques", "stb_image, stb_vorbis, stb_image_write — domaine public (Sean Barrett)", "", "",
        "@Merci d'avoir joué", "Le royaume se souviendra de l'Errant.", "", "", "#LOST KINGDOM — A WORLD WITHOUT BOUNDARIES", "", "Created by SUPERESSENCE STUDIO",
    };
    float y = H_ + 20 - creditsT * 48.f;
    for (const char* l : lines) {
        std::string s = l;
        if (s.empty()) { y += 34; continue; }
        if (s[0] == '#') { u.text(s.substr(1), W_ / 2, y, 52, GOLDT, F_TITLE, A_CENTER, W_ * 0.9f, true); y += 74; }
        else if (s[0] == '@') { u.text(s.substr(1), W_ / 2, y, 22, rgba(0xc9a45c), F_SERIF, A_CENTER, 0, true); y += 36; }
        else { u.text(s, W_ / 2, y, 22, PALE, F_BODY, A_CENTER, W_ * 0.8f, true); y += 34; }
    }
    bool tap = false;
    for (auto& f : u.fingers) if (f.released) tap = true;
    if (y < -50 || (tap && creditsT > 1.5f)) { state = GameState::Title; stateT = 0; audio.playMusic("title", 2.f); if (player != NullEntity && questsDone.count("m7")) { state = GameState::Playing; } }
}

// ============================================================================================ journal tabs
static const char* TABS[] = {"Inventaire", "Équipement", "Compétences", "Artisanat", "Carte", "Quêtes", "Bestiaire", "Collection", "Paramètres", "Sauvegardes"};
static const Menu TAB_MENU[] = {Menu::Inventory, Menu::Equipment, Menu::Skills, Menu::Crafting, Menu::Map, Menu::Quests, Menu::Bestiary, Menu::Collection, Menu::Settings, Menu::Saves};
static const char* TAB_ICON[] = {"i_inventory", "i_character", "i_skills", "i_craft", "i_map", "i_quest", "i_bestiary", "i_collection", "i_settings", "i_save"};

static void itemTooltip(Game& g, Rect r, const ItemStack& s, bool equipped) {
    UI& u = g.ui;
    const ItemDef* d = item(s.id);
    if (!d) return;
    u.nine("panel_plain", r, vec4(1, 1, 1, 0.95f), 0.7f);
    vec3 rc = rarityColor(s.rarity);
    float y = r.y + 18;
    g.itemIcon(s.id, {r.x + 18, y, 80, 80});
    std::string nm = d->name + (s.level > 0 ? " +" + std::to_string(s.level) : "");
    u.text(nm, r.x + 112, y, 24, vec4(rc, 1), F_SERIF, A_LEFT, r.w - 130);
    y += u.textHeight(nm, 24, F_SERIF, r.w - 130) + 2;
    static const char* kinds[] = {"Arme", "Bouclier", "Tête", "Torse", "Mains", "Pieds", "Anneau", "Amulette", "Consommable", "Matériau", "Objet de quête", "Outil", "Monture", "Relique", "Apparence"};
    u.text(std::string(rarityName(s.rarity)) + " · " + kinds[(int)d->kind], r.x + 112, y, 17, vec4(rc * 0.85f, 1), F_BODY);
    y = std::max(y + 30, r.y + 110);
    float m = rarityMult(s.rarity) * (1.f + 0.12f * s.level);
    auto stat = [&](const char* label, float v, const char* fmt) { if (v == 0) return; char b[64]; snprintf(b, sizeof b, fmt, v); u.text(label, r.x + 22, y, 19, DIM, F_BODY); u.text(b, r.x + r.w - 22, y, 19, GOLDT, F_BOLD, A_RIGHT); y += 26; };
    stat("Attaque", d->atk * m, "+%.0f");
    stat("Défense", d->def * m, "+%.0f");
    stat("Points de vie", d->hp * m, "+%.0f");
    stat("Mana", d->mp * m, "+%.0f");
    stat("Critique", d->crit * 100, "+%.0f %%");
    stat("Vitesse", d->speed * 100, "%+.0f %%");
    if (d->elemDmg > 0) { char b[64]; snprintf(b, sizeof b, "+%.0f %s", d->elemDmg * m, elementName(d->element)); u.text("Dégâts élémentaires", r.x + 22, y, 19, DIM, F_BODY); u.text(b, r.x + r.w - 22, y, 19, vec4(elementColor(d->element), 1), F_BOLD, A_RIGHT); y += 26; }
    if (d->heal > 0) stat("Soin", d->heal, "+%.0f PV");
    if (d->mana > 0) stat("Mana rendu", d->mana, "+%.0f");
    if (d->set >= 0 && d->set < (int)sets().size()) {
        const SetDef& sd = sets()[d->set];
        int n = 0; for (int i = 0; i < SL_COUNT; i++) { const ItemDef* e = item(g.equipment[i].id); if (e && e->set == d->set) n++; }
        u.text(sd.name + " (" + std::to_string(n) + "/4)", r.x + 22, y + 4, 18, rgba(0x7ad06a), F_BOLD); y += 28;
        u.text("2 : " + sd.bonus2, r.x + 30, y, 17, n >= 2 ? rgba(0x9ae08a) : DIM, F_BODY, A_LEFT, r.w - 50); y += 24;
        u.text("4 : " + sd.bonus4, r.x + 30, y, 17, n >= 4 ? rgba(0x9ae08a) : DIM, F_BODY, A_LEFT, r.w - 50); y += 26;
    }
    u.text(d->desc, r.x + 22, y + 6, 18, rgba(0xc8bca4), F_BODY, A_LEFT, r.w - 44);
    if (equipped) u.text("Équipé", r.x + r.w - 22, r.y + 18, 16, rgba(0x9ae08a), F_BOLD, A_RIGHT);
}

void Game::drawMenus(float dt) {
    UI& u = ui;
    float W_ = u.W, H_ = u.H;
    if (menu == Menu::None) return;
    if (layoutEdit) { drawLayoutEditor(); return; }
    if (menu != Menu::Death) u.rect({0, 0, W_, H_}, vec4(0.02f, 0.015f, 0.01f, 0.55f));
    // ---------------------------------------------------------------- pause & death
    if (menu == Menu::Pause) {
        u.consumeAll();
        Rect p{W_ / 2 - 230, H_ / 2 - 250, 460, 500};
        u.panel(p, "Pause");
        float y = p.y + 70;
        auto b = [&](const char* l) { bool r = u.fancyButton({p.x + 50, y, p.w - 100, 60}, l, true, "button", 25); y += 72; return r; };
        if (b("Reprendre")) { menu = Menu::None; play("ui_close", 0.5f); }
        if (b("Journal")) { menu = Menu::Inventory; }
        if (b("Sauvegarder")) { menu = Menu::Saves; }
        if (b("Paramètres")) { menu = Menu::Settings; }
        if (b("Menu principal")) { saveGame(99); state = GameState::Title; stateT = 0; menu = Menu::None; audio.playMusic("title", 2.f); dungeon.active = false; }
        return;
    }
    if (menu == Menu::Death) {
        Rect p{W_ / 2 - 240, H_ * 0.52f, 480, 200};
        u.consumeAll();
        if (u.fancyButton({p.x + 40, p.y, p.w - 80, 62}, "Réapparaître (-10 % d'or)", true, "button", 24)) {
            Character& c = pc();
            gold = (int)(gold * 0.9f);
            c.dead = false; c.hp = c.maxHp * 0.6f; c.deathT = 0; c.vis.anim.setState(BodyState::Locomotion, 0.1f); c.vis.anim.stopAction(0.05f);
            c.status = Status();
            if (dungeon.active) { c.pos = dungeon.entrance; }
            else { c.pos = respawnPos; c.pos.y = groundAt(c.pos.x, c.pos.z); }
            camFocus = c.pos;
            if (bossEnt != NullEntity) { reg.destroyLater(bossEnt); bossEnt = NullEntity; }
            menu = Menu::None; deathT = 0;
            play("heal");
        }
        if (u.fancyButton({p.x + 40, p.y + 76, p.w - 80, 62}, "Charger la dernière sauvegarde", true, "button_dark", 22)) {
            if (loadGame(99) || loadGame(saveSlot)) { menu = Menu::None; startPlaying(); }
        }
        return;
    }
    if (menu == Menu::Shop) {
        u.consumeAll();
        Rect f = journalFrame(u);
        const NpcDef& n = npcs()[shopNpc];
        u.panel(f, n.name + " — Commerce");
        if (u.iconButton({f.x + f.w - 60, f.y - 10, 64, 64}, "close")) { menu = Menu::None; play("ui_close", 0.5f); return; }
        u.text("Or : " + std::to_string(gold), f.x + 50, f.y + 40, 22, GOLDT, F_BOLD);
        Rect left{f.x + 40, f.y + 80, f.w * 0.5f - 60, f.h - 110}, right{f.x + f.w * 0.5f + 10, f.y + 80, f.w * 0.5f - 50, f.h - 110};
        u.text("À vendre", left.x, left.y - 30, 20, PALE, F_SERIF);
        u.text("Votre sac (toucher pour vendre)", right.x, right.y - 30, 20, PALE, F_SERIF);
        u.pushClip(left);
        float so = u.scrollArea("shopL", left, n.shop.size() * 76.f);
        float y = left.y - so;
        for (auto& id : n.shop) {
            const ItemDef* d = item(id);
            if (!d) continue;
            int price = d->kind == ItemKind::MountToken ? mounts()[d->mount].price : itemPrice(id, false);
            Rect row{left.x, y, left.w - 12, 70};
            u.nine("slot", row, vec4(1, 1, 1, 0.9f), 0.5f);
            itemIcon(id, {row.x + 6, row.y + 5, 60, 60});
            u.text(d->name, row.x + 78, row.y + 8, 20, vec4(rarityColor(d->rarity), 1), F_BODY);
            u.text(std::to_string(price) + " or", row.x + 78, row.y + 38, 17, gold >= price ? GOLDT : rgba(0xd06050), F_BOLD);
            if (u.button(row)) buyItem(id, price);
            y += 76;
        }
        u.popClip();
        u.pushClip(right);
        int cols = std::max(1, (int)(right.w / 86));
        float so2 = u.scrollArea("shopR", right, ((inventory.size() + cols - 1) / cols) * 86.f);
        for (int i = 0; i < (int)inventory.size(); i++) {
            Rect s{right.x + (i % cols) * 86, right.y + (i / cols) * 86 - so2, 80, 80};
            u.nine("slot", s, vec4(1, 1, 1, 0.9f), 0.4f);
            itemIcon(inventory[i].id, s.inset(6));
            u.rect({s.x + 6, s.y + s.h - 8, s.w - 12, 3}, vec4(rarityColor(inventory[i].rarity), 0.9f));
            if (inventory[i].count > 1) u.text(std::to_string(inventory[i].count), s.x + s.w - 8, s.y + s.h - 28, 16, PALE, F_BOLD, A_RIGHT, 0, true);
            if (u.button(s)) { sellItem(i); break; }
        }
        u.popClip();
        return;
    }
    // ---------------------------------------------------------------- journal frame with tabs
    u.consumeAll();
    Rect f = journalFrame(u);
    u.panel(f);
    float tw = (f.w - 120) / 10.f;
    for (int i = 0; i < 10; i++) {
        Rect t{f.x + 30 + i * tw, f.y - 36, tw - 6, 58};
        bool active = TAB_MENU[i] == menu;
        u.nine(active ? "button" : "button_dark", t, vec4(1, 1, 1, active ? 1.f : 0.85f), 0.8f);
        u.sprite(TAB_ICON[i], {t.x + 8, t.y + 12, 32, 32}, vec4(1, 1, 1, active ? 1 : 0.7f));
        if (tw > 120) u.text(TABS[i], t.x + 44, t.y + 17, 17, active ? GOLDT : DIM, F_SERIF);
        if (u.button(t)) { menu = TAB_MENU[i]; selItem = selSlot = selRecipe = selSkill = selQuest = selEnemy = -1; play("ui_click", 0.5f); }
    }
    if (u.iconButton({f.x + f.w - 56, f.y - 30, 60, 60}, "close")) { menu = state == GameState::Playing ? Menu::None : Menu::None; play("ui_close", 0.5f); return; }
    Rect c{f.x + 36, f.y + 40, f.w - 72, f.h - 64};
    switch (menu) {
    // ---------------------------------------------------------------- inventory
    case Menu::Inventory: {
        Rect grid{c.x, c.y + 40, c.w * 0.58f, c.h - 40};
        u.text("Sac · " + std::to_string(inventory.size()) + "/120", c.x, c.y, 22, PALE, F_SERIF);
        u.sprite("i_coin", {c.x + c.w * 0.58f - 140, c.y - 4, 30, 30});
        u.text(std::to_string(gold), c.x + c.w * 0.58f - 104, c.y, 22, GOLDT, F_BOLD);
        if (u.fancyButton({c.x + 250, c.y - 8, 150, 42}, "Trier", true, "button_dark", 18)) {
            std::stable_sort(inventory.begin(), inventory.end(), [](const ItemStack& a, const ItemStack& b) {
                const ItemDef *da = item(a.id), *db = item(b.id);
                if (da->kind != db->kind) return (int)da->kind < (int)db->kind;
                if (a.rarity != b.rarity) return (int)a.rarity > (int)b.rarity;
                return da->name < db->name;
            });
            selItem = -1;
        }
        int cols = std::max(4, (int)(grid.w / 92));
        u.pushClip(grid);
        float so = u.scrollArea("inv", grid, ((inventory.size() + cols - 1) / cols + 1) * 92.f);
        for (int i = 0; i < (int)inventory.size(); i++) {
            Rect s{grid.x + (i % cols) * 92, grid.y + (i / cols) * 92 - so, 86, 86};
            if (s.y > grid.y + grid.h || s.y + s.h < grid.y) continue;
            u.nine(i == selItem ? "slot_sel" : "slot", s, vec4(1, 1, 1, 1), 0.45f);
            vec3 rc = rarityColor(inventory[i].rarity);
            if ((int)inventory[i].rarity >= 2) u.sprite("glow", s.inset(-6), vec4(rc, 0.25f));
            itemIcon(inventory[i].id, s.inset(8));
            u.rect({s.x + 8, s.y + s.h - 9, s.w - 16, 3}, vec4(rc, 0.95f));
            if (inventory[i].count > 1) u.text(std::to_string(inventory[i].count), s.x + s.w - 8, s.y + s.h - 30, 17, PALE, F_BOLD, A_RIGHT, 0, true);
            if (inventory[i].isNew) u.circle({s.x + 12, s.y + 12}, 5, rgba(0xf8d050), 8);
            if (u.button(s)) { selItem = i; inventory[i].isNew = false; play("ui_click", 0.4f); }
        }
        u.popClip();
        Rect info{c.x + c.w * 0.6f, c.y, c.w * 0.4f, c.h - 90};
        if (selItem >= 0 && selItem < (int)inventory.size()) {
            ItemStack s = inventory[selItem];
            itemTooltip(*this, info, s, false);
            const ItemDef* d = item(s.id);
            float bx = info.x, by = info.y + info.h + 14, bw = (info.w - 12) / 2;
            bool equipable = slotForKind(d->kind) >= 0 || d->kind == ItemKind::Cosmetic;
            if (d->kind == ItemKind::Consumable && u.fancyButton({bx, by, bw, 56}, "Utiliser", true, "button", 22)) { useItem(selItem); if (selItem >= (int)inventory.size()) selItem = -1; }
            if (equipable && u.fancyButton({bx, by, bw, 56}, "Équiper", true, "button", 22)) { equip(selItem); selItem = -1; }
            if (d->kind != ItemKind::Quest && u.fancyButton({bx + bw + 12, by, bw, 56}, "Jeter", true, "button_dark", 22)) {
                Character& p = pc();
                spawnPickup(s.id, 1, s.rarity, p.pos + vec3(std::sin(p.yaw), 0, std::cos(p.yaw)) * 1.5f);
                if (--inventory[selItem].count <= 0) { inventory.erase(inventory.begin() + selItem); selItem = -1; }
            }
        } else u.text("Touchez un objet pour voir ses détails.", info.x + info.w / 2, info.y + 120, 20, DIM, F_BODY, A_CENTER, info.w - 40);
        break;
    }
    // ---------------------------------------------------------------- equipment (live 3D preview)
    case Menu::Equipment: {
        Rect prev{c.x + c.w * 0.32f, c.y, c.w * 0.3f, c.h};
        // render the hero into the portrait target with a slow turntable
        {
            static RenderTarget heroRT;
            if (!heroRT.fbo) heroRT.create(384, 512, GL_RGBA8, true);
            Character& p = pc();
            Camera cm; cm.target = vec3(0, 1.0f, 0); cm.pos = vec3(std::sin(realTime * 0.4f) * 3.6f, 1.35f, std::cos(realTime * 0.4f) * 3.6f); cm.fovY = 36 * DEG; cm.zNear = 0.1f; cm.zFar = 20; cm.update(384.f / 512.f);
            Environment e; e.sunDir = normalize(vec3(0.5f, 0.7f, 0.6f)); e.sunIntensity = 2.8f; e.sunColor = srgb(0xfff0dc); e.skyAmbient = srgb(0x8a9ac0); e.groundAmbient = srgb(0x5a4030) * 0.8f; e.fogDensity = 0; e.exposure = 1.05f; e.saturation = 1.08f; e.contrast = 1.05f;
            Visual& v = p.vis;
            Pose save = v.anim.pose; PoseWorld pw;
            Pose idle; idle.reset();
            for (int b = 0; b < H_COUNT; b++) idle.rot[b] = save.rot[b];
            computeWorld(*v.anim.skeleton, save, pw);
            std::vector<DrawItem> items;
            DrawItem d; d.mesh = v.mesh.get(); d.flags = LIT_SKINNED | (v.albedo ? LIT_TEXTURED : 0); d.albedo = v.albedo; d.bones = pw.skin[0].m; d.boneCount = v.anim.skeleton->size(); d.fx.z = 0.7f; d.outline = true; d.outlineWidth = 0.006f;
            items.push_back(d);
            if (v.weapon) { DrawItem w; w.mesh = v.weapon.get(); w.model = pw.world[v.wt == WeaponType::Bow ? H_PROPL : H_PROPR]; items.push_back(w); }
            if (v.offhand) { DrawItem w; w.mesh = v.offhand.get(); w.model = pw.world[H_PROPL]; items.push_back(w); }
            renderer.renderToTarget(heroRT, cm, e, items, vec4(0, 0, 0, 0));
            float ph = prev.h, pwid = ph * 0.75f;
            u.sprite("glow", {prev.x + prev.w / 2 - pwid * 0.6f, prev.y + ph * 0.1f, pwid * 1.2f, ph * 0.9f}, rgba(0xf6dc9a, 0.15f));
            u.image(heroRT.color.id, {prev.x + prev.w / 2 - pwid / 2, prev.y, pwid, ph}, vec4(0, 1, 1, 0));
        }
        // slots on both sides
        int order[SL_COUNT] = {SL_HEAD, SL_CHEST, SL_HANDS, SL_FEET, SL_WEAPON, SL_OFFHAND, SL_RING, SL_AMULET};
        for (int k = 0; k < SL_COUNT; k++) {
            int sl = order[k];
            bool leftSide = k < 4;
            Rect s{leftSide ? c.x + c.w * 0.32f - 104 : c.x + c.w * 0.62f + 14, c.y + 20 + (k % 4) * 118.f, 96, 96};
            u.nine(sl == selSlot ? "slot_sel" : "slot", s, vec4(1, 1, 1, 1), 0.45f);
            if (!equipment[sl].id.empty()) { itemIcon(equipment[sl].id, s.inset(8)); u.rect({s.x + 8, s.y + s.h - 9, s.w - 16, 3}, vec4(rarityColor(equipment[sl].rarity), 1)); if (equipment[sl].level) u.text("+" + std::to_string(equipment[sl].level), s.x + s.w - 8, s.y + 4, 16, GOLDT, F_BOLD, A_RIGHT, 0, true); }
            u.text(slotName(sl), s.x + s.w / 2, s.y + s.h + 2, 15, DIM, F_BODY, A_CENTER);
            if (u.button(s)) { selSlot = sl; play("ui_click", 0.4f); }
        }
        // stats
        Rect st{c.x, c.y, c.w * 0.32f - 130, c.h};
        Character& p = pc();
        float y = st.y;
        u.text("Caractéristiques", st.x, y, 22, GOLDT, F_SERIF); y += 40;
        auto row = [&](const char* l, const std::string& v) { u.text(l, st.x, y, 18, DIM, F_BODY); u.text(v, st.x + st.w, y, 18, PALE, F_BOLD, A_RIGHT); y += 28; };
        char b[64];
        row("Niveau", std::to_string(level));
        snprintf(b, sizeof b, "%.0f", p.maxHp); row("Vie", b);
        snprintf(b, sizeof b, "%.0f", maxMp); row("Mana", b);
        snprintf(b, sizeof b, "%.0f", maxStamina); row("Endurance", b);
        snprintf(b, sizeof b, "%.0f", p.atk); row("Attaque", b);
        snprintf(b, sizeof b, "%.0f", p.def); row("Défense", b);
        snprintf(b, sizeof b, "%.0f %%", critChance * 100); row("Critique", b);
        snprintf(b, sizeof b, "x%.2f", critDmg); row("Dégâts critiques", b);
        if (p.elemDmg > 0) { snprintf(b, sizeof b, "+%.0f %s", p.elemDmg, elementName(p.element)); row("Élément", b); }
        snprintf(b, sizeof b, "%+.0f %%", (moveSpeedMul - 1) * 100); row("Vitesse", b);
        if (!dye.empty()) row("Teinture", item(dye)->name);
        if (selSlot >= 0 && !equipment[selSlot].id.empty()) {
            Rect info{c.x + c.w * 0.62f + 130, c.y, c.w * 0.38f - 130, c.h - 80};
            itemTooltip(*this, info, equipment[selSlot], true);
            float bw = (info.w - 12) / 2;
            if (u.fancyButton({info.x, info.y + info.h + 12, bw, 56}, "Retirer", true, "button_dark", 21)) { unequip(selSlot); selSlot = -1; }
            if (craftStation == ST_FORGE && selSlot >= 0 && u.fancyButton({info.x + bw + 12, info.y + info.h + 12, bw, 56}, "Améliorer", equipment[selSlot].level < 5, "button", 21)) upgradeItem(selSlot);
        } else if (craftStation == ST_FORGE) u.text("Forge : choisissez un objet équipé à améliorer.", c.x + c.w * 0.81f, c.y + 60, 18, DIM, F_BODY, A_CENTER, c.w * 0.3f);
        break;
    }
    // ---------------------------------------------------------------- skill tree
    case Menu::Skills: {
        u.text("Points disponibles : " + std::to_string(skillPoints), c.x, c.y, 22, skillPoints ? GOLDT : PALE, F_SERIF);
        static const char* emb[] = {"i_cl_warrior", "i_cl_archer", "i_cl_assassin", "i_cl_mage", "i_cl_paladin", "i_cl_hunter"};
        float colW = (c.w * 0.68f) / BR_COUNT;
        const auto& T = skillTree();
        for (int b = 0; b < BR_COUNT; b++) {
            float x = c.x + b * colW + colW / 2;
            u.sprite(emb[b], {x - 34, c.y + 36, 68, 68});
            u.text(branchName(b), x, c.y + 108, 18, PALE, F_SERIF, A_CENTER);
            for (int ti = 0; ti < 5; ti++) {
                int idx = -1;
                for (int i = 0; i < (int)T.size(); i++) if (T[i].branch == b && T[i].tier == ti) idx = i;
                if (idx < 0) continue;
                float y = c.y + 150 + ti * ((c.h - 170) / 5.f);
                bool have = skills.count(T[idx].id) > 0;
                bool avail = !have && (T[idx].requires_.empty() || skills.count(T[idx].requires_));
                if (ti < 4) u.rect({x - 2, y + 30, 4, (c.h - 170) / 5.f - 60}, have ? rgba(0xc9a45c) : rgba(0x4a3a2a));
                Rect n{x - 30, y, 60, 60};
                u.sprite(idx == selSkill ? "round_hi" : (have ? "round_red" : "round"), n, vec4(1, 1, 1, avail || have ? 1.f : 0.5f));
                std::string ic = T[idx].spell.empty() ? std::string(emb[b]) : spells()[spellIndex(T[idx].spell)].icon;
                u.sprite(ic, n.inset(10), vec4(1, 1, 1, have ? 1.f : (avail ? 0.8f : 0.35f)));
                if (u.button(n)) { selSkill = idx; play("ui_click", 0.4f); }
            }
        }
        Rect info{c.x + c.w * 0.7f, c.y, c.w * 0.3f, c.h * 0.48f};
        u.nine("panel_plain", info, vec4(1, 1, 1, 0.95f), 0.6f);
        if (selSkill >= 0) {
            const SkillNode& n = T[selSkill];
            u.text(n.name, info.x + 20, info.y + 18, 24, GOLDT, F_SERIF, A_LEFT, info.w - 40);
            u.text(std::string(branchName(n.branch)) + " · coût " + std::to_string(n.cost), info.x + 20, info.y + 54, 17, DIM, F_BODY);
            u.text(n.desc, info.x + 20, info.y + 86, 19, PALE, F_BODY, A_LEFT, info.w - 40);
            bool have = skills.count(n.id) > 0;
            bool can = !have && skillPoints >= n.cost && (n.requires_.empty() || skills.count(n.requires_));
            if (!have && u.fancyButton({info.x + 20, info.y + info.h - 70, info.w - 40, 54}, "Apprendre", can, "button", 22)) unlockSkill(selSkill);
            if (have) u.text("Acquise", info.x + info.w / 2, info.y + info.h - 52, 20, rgba(0x9ae08a), F_BOLD, A_CENTER);
        }
        // spell slots
        Rect sp{c.x + c.w * 0.7f, c.y + c.h * 0.5f, c.w * 0.3f, c.h * 0.5f};
        u.text("Sorts (toucher pour équiper)", sp.x, sp.y, 19, PALE, F_SERIF);
        for (int s = 0; s < 3; s++) {
            Rect r{sp.x + s * 92, sp.y + 34, 84, 84};
            u.nine(s == selSlot - 100 ? "slot_sel" : "slot", r, vec4(1, 1, 1, 1), 0.45f);
            if (equippedSpells[s] >= 0) u.sprite(spells()[equippedSpells[s]].icon, r.inset(8));
            u.text(std::to_string(s + 1), r.x + 8, r.y + 4, 16, GOLDT, F_BOLD, A_LEFT, 0, true);
            if (u.button(r)) selSlot = 100 + s;
        }
        int cols = std::max(1, (int)(sp.w / 76));
        for (int k = 0; k < (int)knownSpells.size(); k++) {
            int si = spellIndex(knownSpells[k]);
            Rect r{sp.x + (k % cols) * 76, sp.y + 134 + (k / cols) * 76, 70, 70};
            u.sprite(spells()[si].icon, r);
            if (u.button(r)) {
                int slot = selSlot >= 100 ? selSlot - 100 : 0;
                for (int s = 0; s < 3; s++) if (equippedSpells[s] == si) equippedSpells[s] = -1;
                equippedSpells[slot] = si;
                notify(spells()[si].name + " → emplacement " + std::to_string(slot + 1) + " · " + std::to_string((int)spells()[si].mana) + " mana", elementColor(spells()[si].school), spells()[si].icon);
            }
        }
        break;
    }
    // ---------------------------------------------------------------- crafting
    case Menu::Crafting: {
        // a campfire nearby enables cooking
        if (craftStation == ST_ANY || craftStation == ST_CAMPFIRE) {
            craftStation = ST_ANY;
            world.forEachVisibleChunk([&](Chunk& ch) { for (auto& e : ch.emitters) if ((int)e.w == EM_FIRE && distXZ(vec3(e.x, e.y, e.z), pc().pos) < 6) craftStation = ST_CAMPFIRE; });
        }
        u.text(std::string("Poste : ") + (craftStation == ST_ANY ? "Sac (recettes simples)" : stationName(craftStation)), c.x, c.y, 22, GOLDT, F_SERIF);
        if (craftStation == ST_ANY) u.text("Parlez à un forgeron, un alchimiste ou un marchand, ou approchez d'un feu de camp, pour plus de recettes.", c.x, c.y + 34, 17, DIM, F_BODY, A_LEFT, c.w * 0.55f);
        const auto& R = recipes();
        std::vector<int> list;
        for (int i = 0; i < (int)R.size(); i++) if (R[i].station == ST_ANY || R[i].station == craftStation) list.push_back(i);
        Rect lr{c.x, c.y + 80, c.w * 0.55f, c.h - 80};
        u.pushClip(lr);
        float so = u.scrollArea("craft", lr, list.size() * 80.f);
        for (int k = 0; k < (int)list.size(); k++) {
            const Recipe& r = R[list[k]];
            const ItemDef* d = item(r.result);
            Rect row{lr.x, lr.y + k * 80 - so, lr.w - 12, 74};
            bool can = canCraft(r);
            u.nine(list[k] == selRecipe ? "slot_sel" : "slot", row, vec4(1, 1, 1, can ? 1.f : 0.7f), 0.45f);
            itemIcon(r.result, {row.x + 6, row.y + 6, 62, 62}, can ? 1.f : 0.5f);
            u.text(d ? d->name : r.result, row.x + 80, row.y + 10, 20, can ? vec4(rarityColor(d ? d->rarity : Rarity::Common), 1) : DIM, F_BODY);
            std::string req;
            for (auto& in : r.inputs) req += std::to_string(countItem(in.first)) + "/" + std::to_string(in.second) + " " + item(in.first)->name + "  ";
            if (level < r.minLevel) req = "Niveau " + std::to_string(r.minLevel) + " requis";
            u.text(req, row.x + 80, row.y + 40, 15, can ? PALE : rgba(0xc07060), F_BODY, A_LEFT, row.w - 90);
            if (u.button(row)) selRecipe = list[k];
        }
        u.popClip();
        if (selRecipe >= 0) {
            Rect info{c.x + c.w * 0.58f, c.y + 80, c.w * 0.42f, c.h - 170};
            ItemStack s; s.id = R[selRecipe].result; s.rarity = item(s.id) ? item(s.id)->rarity : Rarity::Common;
            itemTooltip(*this, info, s, false);
            if (u.fancyButton({info.x, info.y + info.h + 14, info.w, 60}, "Fabriquer", canCraft(R[selRecipe]), "button", 24)) craft(selRecipe);
        }
        break;
    }
    // ---------------------------------------------------------------- world map
    case Menu::Map: {
        static GLuint fogMap = 0;
        static float rebuildT = 0;
        rebuildT -= dt;
        if (world.mapReady && (rebuildT <= 0 || !fogMap)) {
            rebuildT = 2.f;
            std::vector<uint8_t> img = world.mapImage;
            int R = world.mapRes;
            for (int j = 0; j < R; j++) for (int i = 0; i < R; i++) {
                uint8_t f2 = fog[(j * 128 / R) * 128 + i * 128 / R];
                if (!f2) { uint8_t* p = &img[((size_t)j * R + i) * 4]; uint8_t l = (uint8_t)((p[0] + p[1] + p[2]) / 3); p[0] = (uint8_t)(l * 0.35f + 40); p[1] = (uint8_t)(l * 0.32f + 34); p[2] = (uint8_t)(l * 0.28f + 26); }
            }
            if (!fogMap) glGenTextures(1, &fogMap);
            glBindTexture(GL_TEXTURE_2D, fogMap);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, R, R, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        float side = std::min(c.w * 0.72f, c.h) * mapZoom;
        Rect mr{c.x + (std::min(c.w * 0.72f, c.h) - side) / 2 + mapPan.x, c.y + (c.h - side) / 2 + mapPan.y, side, side};
        u.pushClip(c);
        if (fogMap) u.image(fogMap, mr);
        else u.text("Cartographie en cours…", c.x + c.w * 0.36f, c.y + c.h / 2, 22, PALE, F_BODY, A_CENTER);
        auto toMap = [&](vec2 w) { return vec2(mr.x + w.x / WORLD_SIZE * mr.w, mr.y + w.y / WORLD_SIZE * mr.h); };
        for (auto& P : world.pois) {
            if (!discovered.count(P.id)) continue;
            vec2 m = toMap(P.pos);
            const char* ic = P.type == PoiType::Village || P.type == PoiType::Mill ? "i_village" : P.type == PoiType::Castle ? "i_castle" : (P.type == PoiType::Cave || P.type == PoiType::Mine || P.type == PoiType::Graveyard) ? "i_cave" : P.type == PoiType::Waystone ? "i_waypoint" : P.type == PoiType::BossArena ? "i_boss" : P.type == PoiType::Monastery || P.type == PoiType::Tower ? "i_tower" : "i_star";
            u.sprite(ic, {m.x - 15, m.y - 15, 30, 30});
            if (mapZoom > 1.3f || P.type == PoiType::Village || P.type == PoiType::Castle || P.type == PoiType::Monastery) u.text(P.name, m.x, m.y + 15, 15, rgba(0x2a1a10), F_BOLD, A_CENTER);
        }
        // region names
        static const vec2 rc[R_COUNT] = {{1500, 1500}, {900, 430}, {2520, 2150}, {430, 1450}, {1150, 2650}, {2600, 620}};
        for (int r = 0; r < R_COUNT; r++) { vec2 m = toMap(rc[r]); u.text(regionName(r), m.x, m.y - 40, 20, rgba(0x3a2410, 0.75f), F_SERIF, A_CENTER); }
        bool has; vec3 qm = questMarker(&has);
        if (has) { vec2 m = toMap(qm.xz()); u.sprite("i_quest_marker", {m.x - 14, m.y - 38, 28, 38}); }
        vec2 pm = toMap(pc().pos.xz());
        u.spriteRot("i_player_arrow", pm, vec2(30, 30), minimapArrowAngle(pc().yaw, 0.f), vec4(1, 1, 1, 1));
        u.popClip();
        // pan with a finger
        static std::map<int, vec2> last;
        for (auto& fg : u.fingers) {
            if (!fg.down || !c.contains(fg.sx, fg.sy)) continue;
            auto it = last.find(fg.id);
            if (it != last.end()) mapPan += vec2(fg.x, fg.y) - it->second;
            last[fg.id] = vec2(fg.x, fg.y);
        }
        for (auto it = last.begin(); it != last.end();) { bool alive = false; for (auto& fg : u.fingers) if (fg.id == it->first && fg.down) alive = true; if (!alive) it = last.erase(it); else ++it; }
        Rect side2{c.x + c.w * 0.75f, c.y, c.w * 0.25f, c.h};
        if (u.fancyButton({side2.x, side2.y, side2.w, 54}, "Zoom +", mapZoom < 3, "button_dark", 20)) mapZoom = std::min(3.f, mapZoom * 1.4f);
        if (u.fancyButton({side2.x, side2.y + 64, side2.w, 54}, "Zoom −", mapZoom > 1, "button_dark", 20)) { mapZoom = std::max(1.f, mapZoom / 1.4f); if (mapZoom == 1) mapPan = vec2(0, 0); }
        if (u.fancyButton({side2.x, side2.y + 128, side2.w, 54}, "Centrer", true, "button_dark", 20)) mapPan = vec2(0, 0);
        int found = 0; for (int i = 0; i < 128 * 128; i++) if (fog[i]) found++;
        char b[64]; snprintf(b, sizeof b, "Royaume exploré : %.1f %%", found * 100.f / (128 * 128));
        u.text(b, side2.x, side2.y + 210, 18, PALE, F_BODY, A_LEFT, side2.w);
        u.text(std::string("Région : ") + regionName(world.regionId(pc().pos.x, pc().pos.z)), side2.x, side2.y + 246, 18, GOLDT, F_BODY, A_LEFT, side2.w);
        u.text("Lieux découverts : " + std::to_string(discovered.size()) + "/" + std::to_string(world.pois.size()), side2.x, side2.y + 282, 18, PALE, F_BODY, A_LEFT, side2.w);
        break;
    }
    // ---------------------------------------------------------------- quests
    case Menu::Quests: {
        Rect lr{c.x, c.y, c.w * 0.4f, c.h};
        u.pushClip(lr);
        float so = u.scrollArea("quests", lr, questLog.size() * 70.f + 40);
        float y = lr.y - so;
        std::vector<int> order;
        for (int i = 0; i < (int)questLog.size(); i++) if (!questLog[i].done) order.push_back(i);
        for (int i = 0; i < (int)questLog.size(); i++) if (questLog[i].done) order.push_back(i);
        for (int i : order) {
            const QuestDef& q = quests()[questLog[i].idx];
            Rect row{lr.x, y, lr.w - 14, 62};
            u.nine(i == selQuest ? "slot_sel" : "slot", row, vec4(1, 1, 1, questLog[i].done ? 0.6f : 1.f), 0.4f);
            static const char* typ[] = {"Principale", "Secondaire", "Cachée", "Contrat", "Trésor", "Faction"};
            u.text(q.title, row.x + 16, row.y + 8, 20, questLog[i].done ? DIM : (q.type == QuestType::Main ? GOLDT : PALE), F_BODY, A_LEFT, row.w - 140);
            u.text(questLog[i].done ? "Terminée" : typ[(int)q.type], row.x + 16, row.y + 36, 15, questLog[i].done ? rgba(0x9ae08a) : DIM, F_BODY);
            if (i == trackedQuest) u.sprite("i_quest_marker", {row.x + row.w - 34, row.y + 14, 22, 30});
            if (u.button(row)) selQuest = i;
            y += 70;
        }
        u.popClip();
        if (selQuest >= 0 && selQuest < (int)questLog.size()) {
            const QuestState& s = questLog[selQuest];
            const QuestDef& q = quests()[s.idx];
            Rect info{c.x + c.w * 0.43f, c.y, c.w * 0.57f, c.h};
            u.nine("parchment", info, vec4(1, 1, 1, 1), 0.7f);
            float y2 = info.y + 30;
            u.text(q.title, info.x + 34, y2, 28, rgba(0x5a2a14), F_SERIF, A_LEFT, info.w - 68); y2 += 46;
            y2 += u.text(q.desc, info.x + 34, y2, 19, INK, F_BODY, A_LEFT, info.w - 68) + 18;
            for (int k = 0; k < (int)q.steps.size(); k++) {
                bool done = k < s.step || s.done;
                bool cur = k == s.step && !s.done;
                if (!done && !cur) continue;
                u.sprite(done ? "i_check" : "i_quest_marker", {info.x + 34, y2, 24, 24});
                std::string t = q.steps[k].desc;
                if (cur && q.steps[k].count > 1) t += " (" + std::to_string(s.progress[k]) + "/" + std::to_string(q.steps[k].count) + ")";
                u.text(t, info.x + 66, y2, 19, done ? rgba(0x6a5a40) : INK, F_BODY, A_LEFT, info.w - 100);
                y2 += 32;
            }
            y2 += 10;
            std::string rw = "Récompenses : " + std::to_string(q.xp) + " XP · " + std::to_string(q.gold) + " or";
            for (auto& r : q.rewards) if (item(r)) rw += " · " + item(r)->name;
            u.text(rw, info.x + 34, y2, 17, rgba(0x6a4a20), F_BODY, A_LEFT, info.w - 68);
            if (!s.done && u.fancyButton({info.x + info.w - 230, info.y + info.h - 76, 200, 54}, selQuest == trackedQuest ? "Suivie" : "Suivre", selQuest != trackedQuest, "button", 21)) trackedQuest = selQuest;
        }
        if (questLog.empty()) u.text("Aucune quête.", lr.x + lr.w / 2, lr.y + 60, 20, DIM, F_BODY, A_CENTER);
        break;
    }
    // ---------------------------------------------------------------- bestiary
    case Menu::Bestiary: {
        const auto& E = enemies();
        Rect lr{c.x, c.y, c.w * 0.42f, c.h};
        u.pushClip(lr);
        float so = u.scrollArea("best", lr, E.size() * 58.f);
        for (int i = 0; i < (int)E.size(); i++) {
            Rect row{lr.x, lr.y + i * 58 - so, lr.w - 14, 52};
            auto it = bestiary.find(E[i].id);
            int kills = it == bestiary.end() ? 0 : it->second;
            u.nine(i == selEnemy ? "slot_sel" : "slot", row, vec4(1, 1, 1, kills ? 1.f : 0.6f), 0.4f);
            u.text(kills ? E[i].name : "???", row.x + 14, row.y + 12, 19, kills ? (E[i].behavior == Behavior::Boss ? GOLDT : PALE) : DIM, F_BODY);
            u.text(kills ? std::to_string(kills) : "", row.x + row.w - 14, row.y + 12, 18, DIM, F_BOLD, A_RIGHT);
            if (u.button(row) && kills) { selEnemy = i; portraitNpc = -2; }
        }
        u.popClip();
        if (selEnemy >= 0) {
            const EnemyDef& e = E[selEnemy];
            Rect info{c.x + c.w * 0.45f, c.y, c.w * 0.55f, c.h};
            u.nine("parchment", info, vec4(1, 1, 1, 1), 0.7f);
            // 3D preview of the creature
            static RenderTarget prevRT; static int prevFor = -1; static Visual pv;
            if (!prevRT.fbo) prevRT.create(320, 320, GL_RGBA8, true);
            if (prevFor != selEnemy) {
                prevFor = selEnemy;
                if (e.arch == Arch::Humanoid) setupVisualHumanoid(pv, e.hlook, e.weapon, e.offhand, 1.f); else setupVisualCreature(pv, e.arch, e.clook, 1.f);
            }
            pv.anim.speed = 0; pv.anim.update(dt);
            computeWorld(*pv.anim.skeleton, pv.anim.pose, pv.pw);
            Camera cm; float hgt = e.arch == Arch::Humanoid ? 1.0f : 0.6f;
            cm.target = vec3(0, hgt, 0); cm.pos = vec3(std::sin(realTime * 0.5f) * 3.f, hgt + 0.9f, std::cos(realTime * 0.5f) * 3.f); cm.fovY = 40 * DEG; cm.zNear = 0.1f; cm.zFar = 30; cm.update(1);
            Environment en; en.sunDir = normalize(vec3(0.4f, 0.8f, 0.5f)); en.sunIntensity = 2.8f; en.sunColor = srgb(0xfff0dc); en.skyAmbient = srgb(0x8a9ac0); en.groundAmbient = srgb(0x5a4030); en.fogDensity = 0; en.exposure = 1.05f; en.saturation = 1.05f; en.contrast = 1.05f;
            DrawItem d; d.mesh = pv.mesh.get(); d.flags = LIT_SKINNED | (pv.albedo ? LIT_TEXTURED : 0); d.albedo = pv.albedo; d.bones = pv.pw.skin[0].m; d.boneCount = pv.anim.skeleton->size(); d.fx.z = 0.6f;
            std::vector<DrawItem> list{d};
            if (pv.weapon) { DrawItem w; w.mesh = pv.weapon.get(); w.model = pv.pw.world[pv.wt == WeaponType::Bow ? H_PROPL : H_PROPR]; list.push_back(w); }
            renderer.renderToTarget(prevRT, cm, en, list, vec4(0, 0, 0, 0));
            u.image(prevRT.color.id, {info.x + info.w - 300, info.y + 20, 280, 280}, vec4(0, 1, 1, 0));
            float y2 = info.y + 30;
            u.text(e.name, info.x + 34, y2, 28, rgba(0x5a2a14), F_SERIF, A_LEFT, info.w - 340); y2 += 44;
            u.text(e.family + " · " + regionName(e.region) + " · niveau " + std::to_string(e.level), info.x + 34, y2, 17, rgba(0x6a4a2a), F_BODY, A_LEFT, info.w - 340); y2 += 34;
            y2 += u.text(e.desc, info.x + 34, y2, 19, INK, F_BODY, A_LEFT, info.w - 340) + 14;
            int kills = bestiary[e.id];
            char b[96];
            snprintf(b, sizeof b, "Vaincus : %d", kills); u.text(b, info.x + 34, y2, 18, INK, F_BOLD); y2 += 30;
            if (kills >= 3) {
                if (e.weak != Element::Count) { u.text(std::string("Faiblesse : ") + elementName(e.weak), info.x + 34, y2, 18, vec4(elementColor(e.weak) * 0.7f, 1), F_BOLD); y2 += 28; }
                if (e.resist != Element::Count) { u.text(std::string("Résiste : ") + elementName(e.resist), info.x + 34, y2, 18, rgba(0x5a4a3a), F_BOLD); y2 += 28; }
                snprintf(b, sizeof b, "PV ~%.0f · Attaque ~%.0f", e.hp, e.atk); u.text(b, info.x + 34, y2, 17, rgba(0x5a4a3a), F_BODY);
            } else u.text("Vainquez-en 3 pour révéler ses faiblesses.", info.x + 34, y2, 17, rgba(0x7a6a5a), F_BODY, A_LEFT, info.w - 68);
        }
        break;
    }
    // ---------------------------------------------------------------- collection
    case Menu::Collection: {
        float y = c.y;
        float colW = c.w / 2 - 20;
        u.text("Fragments de Couronne", c.x, y, 22, GOLDT, F_SERIF);
        int shards = countItem("crown_shard") + (questsDone.count("m5") ? 6 : 0);
        for (int i = 0; i < 6; i++) u.sprite(i < shards ? "i_collection" : "i_lock", {c.x + i * 62, y + 34, 54, 54}, vec4(1, 1, 1, i < shards ? 1.f : 0.4f));
        y += 110;
        u.text("Gardiens vaincus : " + std::to_string(defeatedBosses.size()) + "/8", c.x, y, 20, PALE, F_BODY); y += 34;
        for (auto& b : bosses()) { bool d = defeatedBosses.count(b.id) > 0; u.text(std::string(d ? "✓ " : "· ") + (d ? b.name : "???"), c.x + 10, y, 18, d ? rgba(0x9ae08a) : DIM, F_BODY); y += 26; }
        y += 10;
        u.text("Montures", c.x, y, 22, GOLDT, F_SERIF); y += 36;
        for (int i = 0; i < (int)mounts().size(); i++) {
            bool own = std::find(ownedMounts.begin(), ownedMounts.end(), mounts()[i].id) != ownedMounts.end();
            Rect r{c.x, y, colW, 40};
            u.text(std::string(own ? (activeMount == i ? "★ " : "· ") : "  ") + (own ? mounts()[i].name : "???"), r.x + 10, r.y + 8, 18, own ? (activeMount == i ? GOLDT : PALE) : DIM, F_BODY);
            if (own && u.button(r)) { activeMount = i; if (reg.valid(mountEnt) && !riding) { reg.destroyLater(mountEnt); mountEnt = NullEntity; } notify(mounts()[i].name + " sera appelé·e.", vec3(1, 0.9f, 0.7f), "i_mount"); }
            y += 34;
        }
        float x2 = c.x + c.w / 2 + 20;
        y = c.y;
        u.text("Reliques", x2, y, 22, GOLDT, F_SERIF); y += 36;
        for (auto& d : allItems()) if (d.kind == ItemKind::Relic) { bool f2 = relicsFound.count(d.id) > 0; u.text(std::string(f2 ? "✓ " : "· ") + (f2 ? d.name : "???"), x2 + 10, y, 18, f2 ? PALE : DIM, F_BODY); y += 26; }
        y += 14;
        u.text("Chroniques", x2, y, 22, GOLDT, F_SERIF); y += 36;
        for (auto& l : lore()) {
            bool f2 = loreFound.count(l.id) > 0;
            if (f2) { u.text(l.title, x2 + 10, y, 18, rgba(0xd8c090), F_BOLD); y += 24; y += u.text(l.text, x2 + 10, y, 16, PALE, F_BODY, A_LEFT, colW - 20) + 8; }
            else { u.text("· ???", x2 + 10, y, 18, DIM, F_BODY); y += 26; }
        }
        y += 10;
        u.text("Réputation", x2, y, 22, GOLDT, F_SERIF); y += 34;
        for (int fa = 0; fa < FA_COUNT; fa++) { u.text(factionName(fa), x2 + 10, y, 17, PALE, F_BODY); u.bar({x2 + colW - 170, y, 160, 20}, std::min(1.f, reputation[fa] / 150.f), vec4(0.85f, 0.7f, 0.3f, 1)); y += 28; }
        break;
    }
    // ---------------------------------------------------------------- settings
    case Menu::Settings: {
        float y = c.y, colW = c.w / 2 - 30;
        u.text("Qualité graphique", c.x, y, 22, GOLDT, F_SERIF); y += 36;
        for (int p = 0; p < 5; p++) {
            Rect b{c.x + p * (colW / 5), y, colW / 5 - 6, 50};
            bool on = (int)settings.profile == p;
            if (u.fancyButton(b, profileName((QualityProfile)p), true, on ? "button" : "button_dark", 15)) { applyGraphicsProfile((QualityProfile)p); notify(std::string("Profil graphique : ") + profileName((QualityProfile)p), vec3(0.8f, 0.9f, 1.f), "i_settings"); }
        }
        y += 60;
        const GraphicsSettings& gs = renderer.settings;
        char b[200];
        snprintf(b, sizeof b, "Résolution %d %% · ombres %s · occlusion %s · bloom %s · %d FPS · herbe %d %% · feuillage %s", (int)(gs.renderScale * 100), gs.shadowRes ? (gs.softShadows ? "douces" : "oui") : "non", gs.ssao ? "oui" : "non", gs.bloom ? "oui" : "non", gs.fpsCap, (int)(gs.grassDensity * 100), settings.profile <= QualityProfile::Medium ? "détaillé" : "simple");
        u.text(b, c.x, y, 16, DIM, F_BODY, A_LEFT, colW); y += 44;
        if (toggle(u, {c.x, y, colW, 40}, "Aberration chromatique", settings.chromatic)) applyGraphicsProfile(settings.profile); y += 46;
        toggle(u, {c.x, y, colW, 40}, "Afficher les FPS", settings.showFps); y += 46;
        toggle(u, {c.x, y, colW, 40}, "Tremblements de caméra", settings.cameraShake); y += 46;
        toggle(u, {c.x, y, colW, 40}, "Inverser la caméra (horizontal)", settings.invertCamera); y += 46;
        toggle(u, {c.x, y, colW, 40}, "Inverser l'axe vertical", settings.invertCameraY); y += 46;
        toggle(u, {c.x, y, colW, 40}, "Visée automatique", settings.autoTarget); y += 46;
        toggle(u, {c.x, y, colW, 40}, "Vibrations (retour haptique)", settings.haptics); y += 46;
        if (importedModel("hero") && toggle(u, {c.x, y, colW, 40}, "Héros 3D détaillé (armure fixe)", settings.importedHero)) { rebuildPlayerVisual(); saveSettings(); }
        if (importedModel("hero")) y += 46;
        float x2 = c.x + colW + 60; y = c.y;
        u.text("Audio", x2, y, 22, GOLDT, F_SERIF); y += 36;
        bool a = false;
        a |= slider(u, {x2, y, colW, 60}, "Volume général", settings.master, 0, 1); y += 64;
        a |= slider(u, {x2, y, colW, 60}, "Musique", settings.music, 0, 1); y += 64;
        a |= slider(u, {x2, y, colW, 60}, "Effets", settings.sfx, 0, 1); y += 64;
        a |= slider(u, {x2, y, colW, 60}, "Ambiances", settings.ambience, 0, 1); y += 70;
        if (a) audio.setVolumes(settings.master, settings.music, settings.sfx, settings.ambience);
        u.text("Commandes", x2, y, 22, GOLDT, F_SERIF); y += 36;
        slider(u, {x2, y, colW, 60}, "Sensibilité caméra", settings.camSensitivity, 0.3f, 2.5f, "x%.2f", 1.f); y += 64;
        slider(u, {x2, y, colW, 60}, "Zone morte du joystick", settings.joystickDeadzone, 0.f, 0.4f); y += 64;
        if (u.fancyButton({x2, y, colW, 54}, "Personnaliser les commandes", state == GameState::Playing, "button", 20)) { layoutEdit = true; }
        y += 64;
        if (toggle(u, {x2, y, colW, 40}, "Mode gaucher", settings.layout.leftHanded)) {}
        static float lastSave = 0;
        lastSave += dt;
        if (lastSave > 1.f) { saveSettings(); lastSave = 0; }
        break;
    }
    // ---------------------------------------------------------------- saves
    case Menu::Saves: {
        float y = c.y;
        int slots[4] = {99, 0, 1, 2};
        for (int k = 0; k < 4; k++) {
            int s = slots[k];
            Rect r{c.x, y, c.w, 110};
            u.nine("slot", r, vec4(1, 1, 1, 1), 0.5f);
            std::string sum = saveSummary(s);
            u.text(s == 99 ? "Sauvegarde automatique" : "Emplacement " + std::to_string(s + 1), r.x + 24, r.y + 16, 24, GOLDT, F_SERIF);
            u.text(sum.empty() ? "Vide" : sum, r.x + 24, r.y + 56, 19, sum.empty() ? DIM : PALE, F_BODY);
            if (state == GameState::Playing && s != 99 && u.fancyButton({r.x + r.w - 380, r.y + 26, 170, 56}, "Sauvegarder", true, "button", 21)) { if (saveGame(s)) { saveSlot = s; notify("Partie sauvegardée (emplacement " + std::to_string(s + 1) + ")", vec3(0.7f, 0.9f, 1.f), "i_save"); } }
            if (u.fancyButton({r.x + r.w - 196, r.y + 26, 170, 56}, "Charger", !sum.empty(), "button_dark", 21)) { if (loadGame(s)) { menu = Menu::None; startPlaying(); notify("Partie chargée", vec3(0.7f, 0.9f, 1.f), "i_save"); return; } }
            y += 124;
        }
        break;
    }
    default: break;
    }
}

// Drag & drop editor for the on-screen controls (positions, size, opacity).
void Game::drawLayoutEditor() {
    UI& u = ui;
    u.rect({0, 0, u.W, u.H}, vec4(0, 0, 0, 0.35f));
    u.text("Faites glisser les boutons · Touchez « Terminer » pour enregistrer", u.W / 2, 90, 22, GOLDT, F_SERIF, A_CENTER, 0, true);
    static const char* names[CTL_COUNT] = {"Attaque", "Esquive", "Saut", "Parade", "Sort 1", "Sort 2", "Sort 3", "Potion", "Interagir", "Monture"};
    for (int i = 0; i < CTL_COUNT; i++) {
        Rect r = ctlRect(*this, i);
        u.sprite(i == layoutDrag ? "round_hi" : "round", r, vec4(1, 1, 1, settings.layout.opacity));
        if (CTL_ICON[i][0]) u.sprite(std::string("i_") + CTL_ICON[i], r.inset(r.w * 0.25f));
        u.text(names[i], r.x + r.w / 2, r.y + r.h + 2, 15, PALE, F_BOLD, A_CENTER, 0, true);
    }
    for (auto& f : u.fingers) {
        if (f.pressed) for (int i = 0; i < CTL_COUNT; i++) if (ctlRect(*this, i).contains(f.x, f.y)) layoutDrag = i;
        if (f.down && layoutDrag >= 0) {
            vec2 p{f.x - u.W, f.y - u.H};
            if (settings.layout.leftHanded) p.x = -f.x;
            settings.layout.pos[layoutDrag] = vec2(clampf(p.x, -u.W + 60, -40), clampf(p.y, -u.H + 120, -40));
        }
        if (f.released) layoutDrag = -1;
    }
    Rect panel{u.W / 2 - 300, u.H / 2 - 120, 600, 240};
    u.nine("panel_plain", panel, vec4(1, 1, 1, 0.85f), 0.6f);
    slider(u, {panel.x + 30, panel.y + 20, 540, 60}, "Taille des boutons", settings.layout.scale, 0.7f, 1.4f, "x%.2f", 1.f);
    slider(u, {panel.x + 30, panel.y + 84, 540, 60}, "Opacité", settings.layout.opacity, 0.3f, 1.f);
    if (u.fancyButton({panel.x + 30, panel.y + 166, 250, 54}, "Réinitialiser", true, "button_dark", 20)) { ControlLayout L; settings.layout = L; settings.layout.leftHanded = false; loadSettings(); for (int i = 0; i < CTL_COUNT; i++) settings.layout.pos[i] = defaultLayoutPos()[i]; settings.layout.scale = 1; settings.layout.opacity = 0.85f; }
    if (u.fancyButton({panel.x + 320, panel.y + 166, 250, 54}, "Terminer", true, "button", 20)) { layoutEdit = false; saveSettings(); }
    u.consumeAll();
}

}  // namespace lk
