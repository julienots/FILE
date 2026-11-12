#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>
#include <sstream>

namespace lk {

// ============================================================================================ inventory
bool Game::addItem(const std::string& id, int count, Rarity r, bool notifyUser) {
    const ItemDef* d = item(id);
    if (!d || count <= 0) return false;
    Rarity rr = r == Rarity::Count ? d->rarity : r;
    if (d->kind == ItemKind::MountToken) {
        if (std::find(ownedMounts.begin(), ownedMounts.end(), mounts()[d->mount].id) == ownedMounts.end()) ownedMounts.push_back(mounts()[d->mount].id);
        activeMount = d->mount;
        if (notifyUser) notify("Nouvelle monture : " + mounts()[d->mount].name, rarityColor(d->rarity), "i_mount");
        return true;
    }
    if (d->kind == ItemKind::Relic) relicsFound.insert(id);
    if (d->stack > 1) {
        for (auto& s : inventory)
            if (s.id == id && s.count < d->stack * 5) { s.count += count; goto added; }
    }
    if (inventory.size() >= 120) { notify("Inventaire plein !", vec3(1, 0.5f, 0.4f), "i_inventory"); return false; }
    {
        ItemStack s; s.id = id; s.count = count; s.rarity = rr; s.uid = (uint32_t)(gameTime * 1000) ^ (uint32_t)inventory.size() * 2654435761u;
        inventory.push_back(s);
    }
added:
    if (notifyUser) {
        char b[160];
        if (count > 1) snprintf(b, sizeof b, "%s ×%d", d->name.c_str(), count); else snprintf(b, sizeof b, "%s", d->name.c_str());
        notify(b, rarityColor(rr), d->icon);
    }
    advanceQuests(StepKind::Collect, id, 0);  // re-evaluate collect steps
    return true;
}

int Game::countItem(const std::string& id) const {
    int n = 0;
    for (auto& s : inventory) if (s.id == id) n += s.count;
    for (int i = 0; i < SL_COUNT; i++) if (equipment[i].id == id) n++;
    return n;
}

bool Game::removeItem(const std::string& id, int count) {
    int have = 0;
    for (auto& s : inventory) if (s.id == id) have += s.count;
    if (have < count) return false;
    for (size_t i = 0; i < inventory.size() && count > 0;) {
        if (inventory[i].id == id) {
            int take = std::min(count, inventory[i].count);
            inventory[i].count -= take; count -= take;
            if (inventory[i].count <= 0) { inventory.erase(inventory.begin() + i); continue; }
        }
        i++;
    }
    return true;
}

void Game::equip(int i) {
    if (i < 0 || i >= (int)inventory.size()) return;
    const ItemDef* d = item(inventory[i].id);
    if (!d) return;
    if (d->kind == ItemKind::Tool) {
        // tools are swapped into the weapon slot only while gathering; equipping keeps them handy
        notify("Les outils s'utilisent automatiquement lors de la récolte.", vec3(0.9f, 0.9f, 0.8f), d->icon);
        return;
    }
    if (d->kind == ItemKind::Cosmetic) { dye = d->id; rebuildPlayerVisual(); notify("Teinture appliquée : " + d->name, d->color, "i_star"); return; }
    int slot = slotForKind(d->kind);
    if (slot < 0) return;
    ItemStack it = inventory[i];
    inventory.erase(inventory.begin() + i);
    if (!equipment[slot].id.empty()) inventory.push_back(equipment[slot]);
    it.isNew = false;
    equipment[slot] = it;
    recomputeStats();
    rebuildPlayerVisual();
    play("ui_click", 0.7f);
    haptic(10, 60);
}

void Game::unequip(int slot) {
    if (slot < 0 || slot >= SL_COUNT || equipment[slot].id.empty()) return;
    inventory.push_back(equipment[slot]);
    equipment[slot] = ItemStack();
    recomputeStats();
    rebuildPlayerVisual();
}

void Game::useItem(int i) {
    if (i < 0 || i >= (int)inventory.size()) return;
    const ItemDef* d = item(inventory[i].id);
    if (!d) return;
    if (d->kind != ItemKind::Consumable) { equip(i); return; }
    Character& c = pc();
    float healMul = 1.f + skillBonus(skills, &SkillNode::heal);
    if (d->heal > 0) { c.hp = std::min(c.maxHp, c.hp + d->heal * healMul); floatText(c.pos + vec3(0, 2.3f, 0), "+" + std::to_string((int)(d->heal * healMul)), vec3(0.4f, 1, 0.5f)); }
    if (d->mana > 0) mp = std::min(maxMp, mp + d->mana);
    if (d->stam > 0) stamina = std::min(maxStamina, stamina + d->stam);
    if (d->buff == 1) c.status.wellFed = d->buffTime;
    if (d->buff == 2) c.status.strength = d->buffTime;
    if (d->buff == 3) c.status.frostRes = d->buffTime;
    if (d->buff == 4) c.status.wellFed = d->buffTime;
    particles.heal(c.pos, d->mana > 0 ? vec3(0.4f, 0.6f, 1.f) : vec3(0.4f, 1.f, 0.5f));
    if (!c.attackClip && !riding) c.vis.anim.play(&clip(C_DRINK), 1.3f, true);
    play("potion", 0.8f);
    if (--inventory[i].count <= 0) inventory.erase(inventory.begin() + i);
}

int Game::itemPrice(const std::string& id, bool selling) const {
    const ItemDef* d = item(id);
    if (!d) return 0;
    int fac = shopNpc >= 0 ? npcs()[shopNpc].faction : -1;
    float discount = fac >= 0 ? std::min(0.25f, reputation[fac] / 400.f) : 0.f;
    return selling ? std::max(1, (int)(d->value * 0.35f)) : std::max(1, (int)(d->value * (1.15f - discount)));
}

void Game::sellItem(int i) {
    if (i < 0 || i >= (int)inventory.size()) return;
    const ItemDef* d = item(inventory[i].id);
    if (!d || d->kind == ItemKind::Quest) { notify("Objet de quête : impossible à vendre.", vec3(1, 0.7f, 0.5f)); return; }
    int price = (int)(itemPrice(d->id, true) * rarityMult(inventory[i].rarity));
    gold += price;
    if (--inventory[i].count <= 0) inventory.erase(inventory.begin() + i);
    play("coin", 0.8f);
}

bool Game::buyItem(const std::string& id, int price) {
    if (gold < price) { notify("Pas assez d'or.", vec3(1, 0.6f, 0.4f), "i_coin"); return false; }
    const ItemDef* d = item(id);
    if (d && d->kind == ItemKind::MountToken && std::find(ownedMounts.begin(), ownedMounts.end(), mounts()[d->mount].id) != ownedMounts.end()) { notify("Vous possédez déjà cette monture.", vec3(1, 0.8f, 0.6f)); return false; }
    int realPrice = d && d->kind == ItemKind::MountToken ? mounts()[d->mount].price : price;
    if (gold < realPrice) { notify("Pas assez d'or.", vec3(1, 0.6f, 0.4f), "i_coin"); return false; }
    gold -= realPrice;
    addItem(id, 1);
    play("coin", 0.8f);
    return true;
}

// ============================================================================================ crafting
bool Game::canCraft(const Recipe& r) const {
    if (level < r.minLevel) return false;
    if (r.station != ST_ANY && r.station != craftStation) return false;
    for (auto& in : r.inputs) if (countItem(in.first) < in.second) return false;
    return true;
}

bool Game::craft(int ri) {
    const auto& R = recipes();
    if (ri < 0 || ri >= (int)R.size() || !canCraft(R[ri])) return false;
    for (auto& in : R[ri].inputs) removeItem(in.first, in.second);
    // crafted gear can roll a better rarity with skill & luck
    const ItemDef* d = item(R[ri].result);
    Rarity rr = d ? d->rarity : Rarity::Common;
    Rng r((uint64_t)(gameTime * 977));
    if (d && (d->kind == ItemKind::Weapon || d->kind == ItemKind::Head || d->kind == ItemKind::Chest || d->kind == ItemKind::Hands || d->kind == ItemKind::Feet)) {
        float roll = r.f();
        if (roll < 0.15f && (int)rr < (int)Rarity::Legendary) rr = (Rarity)((int)rr + 1);
    }
    addItem(R[ri].result, R[ri].count, rr);
    advanceQuests(StepKind::Craft, R[ri].result, R[ri].count);
    play(R[ri].station == ST_FORGE ? "forge" : "craft", 0.9f);
    particles.sparks(pc().pos + vec3(0, 1, 0), {0, 1, 0}, 14, vec3(1, 0.8f, 0.4f));
    gainXp(10 + R[ri].minLevel * 3);
    haptic(20, 100);
    return true;
}

bool Game::upgradeItem(int slot) {
    ItemStack& s = equipment[slot];
    if (s.id.empty() || s.level >= 5) return false;
    int bars = s.level + 1, cost = 60 * (s.level + 1) * (s.level + 1);
    std::string mat = s.level >= 3 ? "star_bar" : "iron_bar";
    if (countItem(mat) < bars || gold < cost) { notify("Il faut " + std::to_string(bars) + " × " + item(mat)->name + " et " + std::to_string(cost) + " or.", vec3(1, 0.7f, 0.5f), "i_anvil"); return false; }
    removeItem(mat, bars); gold -= cost;
    s.level++;
    recomputeStats();
    play("forge"); particles.sparks(pc().pos + vec3(0, 1, 0), {0, 1, 0}, 24, vec3(1, 0.8f, 0.4f));
    notify(item(s.id)->name + " amélioré en +" + std::to_string(s.level), rarityColor(s.rarity), "i_anvil");
    return true;
}

// ============================================================================================ quests
bool Game::questActive(const std::string& id) const {
    int qi = questIndex(id);
    for (auto& q : questLog) if (q.idx == qi && !q.done) return true;
    return false;
}

QuestState* Game::questState(const std::string& id) {
    int qi = questIndex(id);
    for (auto& q : questLog) if (q.idx == qi) return &q;
    return nullptr;
}

void Game::startQuest(const std::string& id) {
    int qi = questIndex(id);
    if (qi < 0 || questActive(id) || questsDone.count(id)) return;
    const QuestDef& q = quests()[qi];
    QuestState s; s.idx = qi; s.progress.assign(q.steps.size(), 0);
    questLog.push_back(s);
    trackedQuest = (int)questLog.size() - 1;
    notify((q.type == QuestType::Main ? "Quête principale : " : (q.type == QuestType::Hidden ? "Quête cachée : " : "Nouvelle quête : ")) + q.title, vec3(1.f, 0.85f, 0.4f), "i_quest");
    play("quest", 0.8f);
    if (id == "m3") addItem("letter_maelle", 1);
    if (id == "t_vael" || id == "t_bastion") addItem("treasure_map", 1, Rarity::Rare, false);
    // re-evaluate immediately (items already owned, places already discovered)
    advanceQuests(StepKind::Collect, "", 0);
    if (!q.steps.empty() && q.steps[0].kind == StepKind::Discover && discovered.count(q.steps[0].target)) advanceQuests(StepKind::Discover, q.steps[0].target);
    if (!q.steps.empty() && q.steps[0].kind == StepKind::Dungeon && dungeonsDone.count(q.steps[0].target)) advanceQuests(StepKind::Dungeon, q.steps[0].target);
    if (!q.steps.empty() && q.steps[0].kind == StepKind::KillBoss && defeatedBosses.count(q.steps[0].target)) advanceQuests(StepKind::KillBoss, q.steps[0].target);
}

void Game::completeQuest(QuestState& s) {
    const QuestDef& q = quests()[s.idx];
    s.done = true;
    questsDone.insert(q.id);
    gainXp(q.xp);
    gold += q.gold;
    for (auto& r : q.rewards) addItem(r, 1);
    if (q.faction >= 0) addReputation(q.faction, q.rep);
    notify("Quête terminée : " + q.title, vec3(0.6f, 1.f, 0.6f), "i_check");
    if (!q.done.empty()) floatText(pc().pos + vec3(0, 2.6f, 0), "Quête terminée !", vec3(1, 0.9f, 0.4f), 26);
    play("quest");
    haptic(60, 150);
    if (q.id == "m6") { startQuest("m7"); notify("L'Abîme s'est ouvert au sud de Morneval.", vec3(0.8f, 0.5f, 1.f), "i_waypoint"); }
    // pick the next active quest to track
    trackedQuest = -1;
    for (int i = (int)questLog.size() - 1; i >= 0; i--) if (!questLog[i].done) { trackedQuest = i; if (quests()[questLog[i].idx].type == QuestType::Main) break; }
}

void Game::advanceQuests(StepKind kind, const std::string& target, int amount) {
    bool changed = true;
    int guard = 0;
    while (changed && guard++ < 8) {
        changed = false;
        for (auto& s : questLog) {
            if (s.done) continue;
            const QuestDef& q = quests()[s.idx];
            if (s.step >= (int)q.steps.size()) continue;
            const QuestStep& st = q.steps[s.step];
            bool hit = false;
            if (st.kind == StepKind::Collect) {
                int have = countItem(st.target);
                if (have != s.progress[s.step]) { s.progress[s.step] = std::min(have, st.count); hit = true; }
            } else if (st.kind == kind && st.kind == StepKind::Discover && st.target == "waystones") {
                int n = 0; for (auto& p : world.pois) if (p.type == PoiType::Waystone && discovered.count(p.id)) n++;
                s.progress[s.step] = n; hit = true;
            } else if (st.kind == kind && (st.target == target || (st.kind == StepKind::Harvest && target == st.target))) {
                s.progress[s.step] += std::max(1, amount);
                hit = true;
            }
            if (hit && s.progress[s.step] >= st.count) {
                s.step++;
                changed = true;
                if (s.step >= (int)q.steps.size()) completeQuest(s);
                else { notify("Objectif : " + q.steps[s.step].desc, vec3(1.f, 0.9f, 0.6f), "i_quest_marker"); play("quest", 0.5f, 1.2f); }
            } else if (hit && kind == StepKind::Kill && amount > 0) {
                char b[160]; snprintf(b, sizeof b, "%s : %d/%d", st.desc.c_str(), s.progress[s.step], st.count);
                notify(b, vec3(0.9f, 0.9f, 0.8f), "i_quest");
            }
        }
    }
}

std::string Game::questObjectiveText() const {
    if (trackedQuest < 0 || trackedQuest >= (int)questLog.size()) return "";
    const QuestState& s = questLog[trackedQuest];
    if (s.done) return "";
    const QuestDef& q = quests()[s.idx];
    if (s.step >= (int)q.steps.size()) return q.title;
    const QuestStep& st = q.steps[s.step];
    char b[256];
    if (st.count > 1) snprintf(b, sizeof b, "%s (%d/%d)", st.desc.c_str(), s.progress[s.step], st.count);
    else snprintf(b, sizeof b, "%s", st.desc.c_str());
    return b;
}

vec3 Game::questMarker(bool* has) const {
    *has = false;
    if (trackedQuest < 0 || trackedQuest >= (int)questLog.size()) return {};
    const QuestState& s = questLog[trackedQuest];
    if (s.done) return {};
    const QuestDef& q = quests()[s.idx];
    if (s.step >= (int)q.steps.size()) return {};
    const QuestStep& st = q.steps[s.step];
    std::string poi = st.poi;
    if (poi.empty() && (st.kind == StepKind::Talk || st.kind == StepKind::Deliver)) { int ni = npcIndex(st.target); if (ni >= 0) poi = npcs()[ni].home; }
    if (poi.empty() && st.kind == StepKind::Collect && st.target == "crown_shard") {
        // nearest boss not yet defeated
        float best = 1e9f; const Poi* bp = nullptr;
        Character* p = reg.valid(player) ? const_cast<Registry&>(reg).get<Character>(player) : nullptr;
        for (auto& b : bosses()) {
            if (b.shard.empty() || defeatedBosses.count(b.id)) continue;
            const Poi* pp = world.poiById(b.arenaPoi);
            if (pp && p) { float d = length(pp->pos - p->pos.xz()); if (d < best) { best = d; bp = pp; } }
        }
        if (bp) { *has = true; return vec3(bp->pos.x, bp->height, bp->pos.y); }
    }
    if (poi.empty()) return {};
    const Poi* p = world.poiById(poi);
    if (!p) return {};
    *has = true;
    return vec3(p->pos.x, p->height, p->pos.y);
}

void Game::discover(const std::string& poi) {
    if (discovered.count(poi)) return;
    discovered.insert(poi);
    const Poi* p = world.poiById(poi);
    if (!p) return;
    notify("Lieu découvert : " + p->name, vec3(0.75f, 0.9f, 1.f), "i_compass");
    play("discover", 0.7f);
    gainXp(30);
    if (p->type == PoiType::Waystone) { respawnPos = vec3(p->pos.x + 3, p->height, p->pos.y + 3); notify("Point de réapparition enregistré.", vec3(0.6f, 0.9f, 1.f), "i_waypoint"); }
    // hidden quests
    if (poi == "elven_ruins") startQuest("x_harp");
    if (poi == "morneval") startQuest("x_ring");
    if (p->type == PoiType::Waystone) { if (!questActive("x_stones") && !questsDone.count("x_stones")) { int n = 0; for (auto& q : world.pois) if (q.type == PoiType::Waystone && discovered.count(q.id)) n++; if (n >= 2) startQuest("x_stones"); } }
    advanceQuests(StepKind::Discover, poi);
    advanceQuests(StepKind::Discover, "waystones");
    if (poi == "morneval" && questsDone.count("x_ring") == 0) {}
    // treasure: a chest appears at the treasure spot
    if ((poi == "vael_ruins" && questActive("t_vael")) || (poi == "ashen_ruins" && questActive("t_bastion"))) {
        NodeSpawn n; n.type = NodeType::Chest; n.pos = vec3(p->pos.x + 2, p->height, p->pos.y - 3); n.rot = 0.5f; n.uid = hash32((uint32_t)poi.size() * 7919 + 13); n.tier = 2;
        spawnNode(n);
    }
}

// ============================================================================================ dialogue
void Game::openDialog(Entity ent) {
    Interact* it = reg.get<Interact>(ent);
    Character* c = reg.get<Character>(ent);
    if (!it || !c) return;
    dialog = DialogState();
    dialog.open = true; dialog.ent = ent; dialog.npc = it->param; dialog.line = 0; dialog.reveal = 0;
    play("ui_open", 0.5f);
    if (it->param < 0) {
        // generic villager
        static const char* lines[] = {"Belle journée, n'est-ce pas ?", "Les routes ne sont plus sûres…", "On raconte que tu as vaincu des monstres !", "Le marché ouvre à l'aube.",
                                      "Ma grand-mère a connu le temps des rois.", "Fais attention la nuit, les morts se lèvent.", "Tu as l'air fatigué, voyageur.", "As-tu goûté le ragoût de l'auberge ?",
                                      "Les étoiles brillent plus fort ces temps-ci.", "Mon fils veut devenir chevalier, comme toi."};
        dialog.speaker = c->name; dialog.title = "Villageois";
        int li = (-1 - it->param + (int)(gameTime / 30)) % 10;
        std::string line = lines[li];
        if (env.night > 0.6f) line = "Il est tard… Je ferais mieux de rentrer.";
        if (rainAmt > 0.5f) line = "Quelle pluie ! Les champs vont être contents.";
        if (defeatedBosses.size() >= 3 && li % 3 == 0) line = "Les bardes chantent déjà tes exploits contre les gardiens !";
        dialog.lines = {line};
        dialog.choices = {{"Au revoir", "close"}};
        return;
    }
    const NpcDef& n = npcs()[it->param];
    dialog.speaker = n.name; dialog.title = n.title;
    bool first = !talkedTo.count(n.id);
    talkedTo.insert(n.id);
    dialog.lines.push_back(n.greetings[first ? 0 : (int)(gameTime / 20) % n.greetings.size()]);
    if (env.night > 0.7f && n.role != NpcRole::Innkeeper) dialog.lines[0] = "Il est tard, voyageur… mais je t'écoute.";
    // story beats: talking advances Talk steps
    advanceQuests(StepKind::Talk, n.id);
    // deliveries
    for (auto& s : questLog) {
        if (s.done) continue;
        const QuestDef& q = quests()[s.idx];
        if (s.step >= (int)q.steps.size()) continue;
        const QuestStep& st = q.steps[s.step];
        bool here = (st.kind == StepKind::Deliver) && (q.giver == n.id || (!st.poi.empty() && st.poi == n.home));
        if (here) {
            int have = countItem(st.target);
            const ItemDef* d = item(st.target);
            if (have >= st.count) dialog.choices.push_back({"Remettre : " + (d ? d->name : st.target) + " ×" + std::to_string(st.count), "deliver:" + q.id});
            else dialog.lines.push_back(q.progress.empty() ? ("Il me faut encore : " + (d ? d->name : st.target) + " (" + std::to_string(have) + "/" + std::to_string(st.count) + ")") : q.progress);
        }
    }
    // quest offers
    for (int qi = 0; qi < (int)quests().size(); qi++) {
        const QuestDef& q = quests()[qi];
        if (q.giver != n.id || questActive(q.id) || questsDone.count(q.id)) continue;
        if (!q.requires_.empty() && !questsDone.count(q.requires_)) continue;
        dialog.choices.push_back({std::string(q.type == QuestType::Main ? "★ " : (q.type == QuestType::Hunt ? "⚔ Contrat : " : "! ")) + q.title, "offer:" + q.id});
    }
    // services
    if (!n.shop.empty()) dialog.choices.push_back({"Commerce", "shop"});
    if (n.role == NpcRole::Blacksmith) { dialog.choices.push_back({"Utiliser la forge", "craft:1"}); dialog.choices.push_back({"Améliorer mon arme", "upgrade"}); }
    if (n.role == NpcRole::Alchemist) dialog.choices.push_back({"Table d'alchimie", "craft:2"});
    if (n.role == NpcRole::Hunter || n.role == NpcRole::Merchant || n.role == NpcRole::Stablemaster) dialog.choices.push_back({"Établi", "craft:4"});
    if (n.role == NpcRole::Innkeeper) dialog.choices.push_back({"Prendre une chambre (20 or) — sauvegarde", "rest"});
    if (n.role == NpcRole::Priest) dialog.choices.push_back({"Demander des soins", "heal"});
    if (n.role == NpcRole::Elder && n.faction >= 0) dialog.choices.push_back({"Réputation auprès de : " + std::string(factionName(n.faction)), "rep:" + std::to_string(n.faction)});
    if (!n.lore.empty()) dialog.choices.push_back({"Parle-moi de cet endroit", "lore"});
    dialog.choices.push_back({"Au revoir", "close"});
}

void Game::dialogAction(const std::string& a) {
    play("ui_click", 0.6f);
    auto setChoices = [&](std::vector<DialogState::Choice> c) { dialog.choices = c; };
    if (a == "close") { dialog.open = false; return; }
    if (a.rfind("offer:", 0) == 0) {
        const QuestDef& q = quests()[questIndex(a.substr(6))];
        dialog.lines = {q.offer.empty() ? q.desc : q.offer};
        dialog.line = 0; dialog.reveal = 0;
        setChoices({{"Accepter", "accept:" + q.id}, {"Pas maintenant", "back"}});
        return;
    }
    if (a.rfind("accept:", 0) == 0) {
        startQuest(a.substr(7));
        dialog.lines = {"Que les anciens rois veillent sur toi."};
        dialog.line = 0; dialog.reveal = 0;
        setChoices({{"Au revoir", "close"}});
        return;
    }
    if (a.rfind("deliver:", 0) == 0) {
        QuestState* s = questState(a.substr(8));
        if (!s) return;
        const QuestDef& q = quests()[s->idx];
        const QuestStep& st = q.steps[s->step];
        if (removeItem(st.target, st.count)) {
            s->progress[s->step] = st.count;
            s->step++;
            dialog.lines = {q.done.empty() ? "Merci, Errant." : q.done};
            dialog.line = 0; dialog.reveal = 0;
            if (s->step >= (int)q.steps.size()) completeQuest(*s);
            setChoices({{"Au revoir", "close"}});
            if (!dialog.ent || !reg.valid(dialog.ent)) return;
            Entity e = dialog.ent;
            openDialog(e);
            dialog.lines.insert(dialog.lines.begin(), q.done.empty() ? "Merci, Errant." : q.done);
        }
        return;
    }
    if (a == "shop") { shopNpc = dialog.npc; dialog.open = false; menu = Menu::Shop; selItem = -1; play("ui_open", 0.6f); return; }
    if (a.rfind("craft:", 0) == 0) { craftStation = std::atoi(a.c_str() + 6); dialog.open = false; menu = Menu::Crafting; selRecipe = -1; return; }
    if (a == "upgrade") { craftStation = ST_FORGE; dialog.open = false; menu = Menu::Equipment; return; }
    if (a == "rest") { if (gold >= 20) { gold -= 20; dialog.open = false; restAtInn(); } else { dialog.lines = {"Vingt pièces, pas une de moins."}; dialog.line = 0; dialog.reveal = 0; } return; }
    if (a == "heal") { Character& c = pc(); c.hp = c.maxHp; mp = maxMp; particles.heal(c.pos); play("heal"); dialog.lines = {"Voilà. Que la Lumière te garde."}; dialog.line = 0; dialog.reveal = 0; setChoices({{"Merci", "close"}}); return; }
    if (a == "lore") {
        const NpcDef& n = npcs()[dialog.npc];
        dialog.lines = n.lore; dialog.line = 0; dialog.reveal = 0;
        setChoices({{"Merci", "back"}});
        return;
    }
    if (a.rfind("rep:", 0) == 0) {
        int f = std::atoi(a.c_str() + 4);
        int r = reputation[f];
        const char* rank = r >= 150 ? "Héros" : r >= 80 ? "Champion" : r >= 40 ? "Allié" : r >= 10 ? "Ami" : "Inconnu";
        dialog.lines = {std::string("Ta réputation : ") + rank + " (" + std::to_string(r) + "). Nos marchands te font " + std::to_string(std::min(25, r / 4)) + " % de remise."};
        dialog.line = 0; dialog.reveal = 0;
        setChoices({{"Retour", "back"}});
        return;
    }
    if (a == "back" && reg.valid(dialog.ent)) { openDialog(dialog.ent); return; }
    dialog.open = false;
}

void Game::addReputation(int f, int amount) {
    if (f < 0 || f >= FA_COUNT) return;
    int before = reputation[f];
    reputation[f] += amount;
    notify(std::string(factionName(f)) + " : réputation +" + std::to_string(amount), vec3(0.8f, 1.f, 0.8f), "i_star");
    int ranks[] = {10, 40, 80, 150};
    const char* names[] = {"Ami", "Allié", "Champion", "Héros"};
    for (int i = 0; i < 4; i++) if (before < ranks[i] && reputation[f] >= ranks[i]) notify(std::string("Nouveau rang : ") + names[i] + " — " + factionName(f), vec3(1, 0.85f, 0.4f), "i_collection");
}

void Game::restAtInn() {
    fadeTransition([this] {
        timeOfDay = 7.f; day++;
        Character& c = pc();
        c.hp = c.maxHp; mp = maxMp; stamina = maxStamina;
        c.status.wellFed = 300;
        respawnPos = c.pos;
        saveGame(saveSlot);
        notify("Vous avez dormi jusqu'au matin. Partie sauvegardée.", vec3(0.8f, 0.9f, 1.f), "i_save");
    });
}

bool Game::unlockSkill(int idx) {
    const SkillNode& n = skillTree()[idx];
    if (skills.count(n.id) || skillPoints < n.cost) return false;
    if (!n.requires_.empty() && !skills.count(n.requires_)) return false;
    skillPoints -= n.cost;
    skills.insert(n.id);
    if (!n.spell.empty()) learnSpell(n.spell);
    recomputeStats();
    play("levelup", 0.6f, 1.3f);
    particles.magicBurst(pc().pos + vec3(0, 1, 0), vec3(1, 0.85f, 0.4f), 1.4f, 30);
    notify("Compétence acquise : " + n.name, vec3(1, 0.85f, 0.4f), "i_skills");
    return true;
}

void Game::learnSpell(const std::string& id) {
    if (std::find(knownSpells.begin(), knownSpells.end(), id) != knownSpells.end()) return;
    knownSpells.push_back(id);
    int si = spellIndex(id);
    for (int i = 0; i < 3; i++) if (equippedSpells[i] < 0) { equippedSpells[i] = si; break; }
    notify("Nouveau sort : " + spells()[si].name, elementColor(spells()[si].school), spells()[si].icon);
}

// ============================================================================================ save / load
static std::string slotPath(int slot) { return platform::writableDir() + "/save_" + std::to_string(slot) + ".lks"; }

bool Game::saveGame(int slot) {
    if (player == NullEntity) return false;
    Character& c = pc();
    std::ostringstream o;
    o.precision(9);
    o << "LOSTKINGDOM_SAVE 1\n";
    o << "player " << c.pos.x << " " << c.pos.y << " " << c.pos.z << " " << c.yaw << " " << c.hp << " " << mp << " " << stamina << "\n";
    o << "respawn " << respawnPos.x << " " << respawnPos.y << " " << respawnPos.z << "\n";
    o << "prog " << level << " " << xp << " " << gold << " " << skillPoints << " " << gameTime << " " << timeOfDay << " " << day << " " << playSeconds << "\n";
    for (auto& s : inventory) o << "inv " << s.id << " " << s.count << " " << (int)s.rarity << " " << s.level << "\n";
    for (int i = 0; i < SL_COUNT; i++) if (!equipment[i].id.empty()) o << "eq " << i << " " << equipment[i].id << " " << (int)equipment[i].rarity << " " << equipment[i].level << "\n";
    if (!dye.empty()) o << "dye " << dye << "\n";
    for (auto& s : skills) o << "skill " << s << "\n";
    for (auto& s : knownSpells) o << "spell " << s << "\n";
    o << "spellslots " << equippedSpells[0] << " " << equippedSpells[1] << " " << equippedSpells[2] << "\n";
    for (auto& m : ownedMounts) o << "mount " << m << "\n";
    o << "activemount " << activeMount << "\n";
    for (auto& q : questLog) { o << "quest " << quests()[q.idx].id << " " << q.step << " " << (q.done ? 1 : 0); for (int p : q.progress) o << " " << p; o << "\n"; }
    for (auto& q : questsDone) o << "qdone " << q << "\n";
    o << "track " << trackedQuest << "\n";
    for (auto& d : discovered) o << "disc " << d << "\n";
    for (auto& b : defeatedBosses) o << "boss " << b << "\n";
    for (auto& b : bestiary) o << "best " << b.first << " " << b.second << "\n";
    for (auto& l : loreFound) o << "lore " << l << "\n";
    for (auto& t : talkedTo) o << "talk " << t << "\n";
    for (auto& d : dungeonsDone) o << "dung " << d << "\n";
    for (auto u : killedUnique) o << "uniq " << u << "\n";
    for (auto u : openedChests) o << "chest " << u << "\n";
    for (auto& h : world.harvested) o << "harv " << h.first << " " << h.second << "\n";
    for (auto& cs : clearedSpawns) if (cs.second > gameTime) o << "clear " << cs.first << " " << cs.second << "\n";
    o << "rep"; for (int i = 0; i < FA_COUNT; i++) o << " " << reputation[i]; o << "\n";
    o << "fog ";
    // run-length encoded discovery map
    for (size_t i = 0; i < fog.size();) { uint8_t v = fog[i]; size_t j = i; while (j < fog.size() && fog[j] == v && j - i < 9999) j++; o << (v ? 1 : 0) << ":" << (j - i) << ","; i = j; }
    o << "\n";
    o << "weather " << weather << " " << weatherTimer << "\n";
    o << "end\n";
    bool ok = platform::writeFileAtomic(slotPath(slot), o.str());
    if (!ok) AGR_ERR("save failed (slot %d)", slot);
    return ok;
}

bool Game::loadGame(int slot) {
    std::string data;
    if (!platform::readFile(slotPath(slot), data)) return false;
    if (data.rfind("LOSTKINGDOM_SAVE", 0) != 0 || data.find("\nend\n") == std::string::npos) { AGR_ERR("corrupt save slot %d", slot); return false; }
    // reset world-dependent state
    reg.clear();
    player = mountEnt = bossEnt = lockTarget = interactTarget = NullEntity;
    riding = false;
    nodeEntities.clear(); spawnedChunks.clear(); chunkSpawns.clear(); villageNpcsSpawned.clear();
    inventory.clear(); for (auto& e : equipment) e = ItemStack();
    skills.clear(); knownSpells.clear(); ownedMounts.clear(); questLog.clear(); questsDone.clear(); discovered.clear(); defeatedBosses.clear();
    bestiary.clear(); loreFound.clear(); talkedTo.clear(); dungeonsDone.clear(); killedUnique.clear(); openedChests.clear(); world.harvested.clear(); clearedSpawns.clear();
    fog.assign(128 * 128, 0);
    dye.clear();
    dungeon = DungeonState();
    std::istringstream in(data);
    std::string line;
    vec3 pos; float yaw = 0, hp = -1;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        std::string k; l >> k;
        if (k == "player") { l >> pos.x >> pos.y >> pos.z >> yaw >> hp >> mp >> stamina; }
        else if (k == "respawn") l >> respawnPos.x >> respawnPos.y >> respawnPos.z;
        else if (k == "prog") l >> level >> xp >> gold >> skillPoints >> gameTime >> timeOfDay >> day >> playSeconds;
        else if (k == "inv") { ItemStack s; int r; l >> s.id >> s.count >> r >> s.level; s.rarity = (Rarity)r; s.isNew = false; if (item(s.id)) inventory.push_back(s); }
        else if (k == "eq") { int i, r; ItemStack s; l >> i >> s.id >> r >> s.level; s.rarity = (Rarity)r; if (i >= 0 && i < SL_COUNT && item(s.id)) equipment[i] = s; }
        else if (k == "dye") l >> dye;
        else if (k == "skill") { std::string s; l >> s; skills.insert(s); }
        else if (k == "spell") { std::string s; l >> s; knownSpells.push_back(s); }
        else if (k == "spellslots") l >> equippedSpells[0] >> equippedSpells[1] >> equippedSpells[2];
        else if (k == "mount") { std::string s; l >> s; ownedMounts.push_back(s); }
        else if (k == "activemount") l >> activeMount;
        else if (k == "quest") {
            std::string id; QuestState q; int d; l >> id >> q.step >> d; q.done = d != 0; q.idx = questIndex(id);
            if (q.idx < 0) continue;
            int p; while (l >> p) q.progress.push_back(p);
            q.progress.resize(quests()[q.idx].steps.size(), 0);
            questLog.push_back(q);
        }
        else if (k == "qdone") { std::string s; l >> s; questsDone.insert(s); }
        else if (k == "track") l >> trackedQuest;
        else if (k == "disc") { std::string s; l >> s; discovered.insert(s); }
        else if (k == "boss") { std::string s; l >> s; defeatedBosses.insert(s); }
        else if (k == "best") { std::string s; int n; l >> s >> n; bestiary[s] = n; }
        else if (k == "lore") { std::string s; l >> s; loreFound.insert(s); }
        else if (k == "talk") { std::string s; l >> s; talkedTo.insert(s); }
        else if (k == "dung") { std::string s; l >> s; dungeonsDone.insert(s); }
        else if (k == "uniq") { uint32_t u; l >> u; killedUnique.insert(u); }
        else if (k == "chest") { uint32_t u; l >> u; openedChests.insert(u); }
        else if (k == "harv") { uint32_t u; double t; l >> u >> t; world.harvested[u] = t; }
        else if (k == "clear") { std::string s; double t; l >> s >> t; clearedSpawns[s] = t; }
        else if (k == "rep") for (int i = 0; i < FA_COUNT; i++) l >> reputation[i];
        else if (k == "fog") {
            std::string rle; l >> rle;
            size_t idx = 0, p0 = 0;
            while (p0 < rle.size() && idx < fog.size()) {
                size_t c = rle.find(':', p0), e = rle.find(',', p0);
                if (c == std::string::npos || e == std::string::npos) break;
                int v = std::atoi(rle.substr(p0, c - p0).c_str()), n = std::atoi(rle.substr(c + 1, e - c - 1).c_str());
                for (int i = 0; i < n && idx < fog.size(); i++) fog[idx++] = v ? 255 : 0;
                p0 = e + 1;
            }
        }
        else if (k == "weather") l >> weather >> weatherTimer;
    }
    if (trackedQuest >= (int)questLog.size()) trackedQuest = -1;
    if (pos.x > WORLD_SIZE) pos = respawnPos;  // saved inside a dungeon: resume at its entrance
    pos.y = world.groundHeight(pos.x, pos.z);
    spawnPlayer(pos);
    Character& c = pc();
    c.yaw = yaw;
    recomputeStats();
    rebuildPlayerVisual();
    if (hp > 0) c.hp = std::min(hp, c.maxHp);
    saveSlot = slot == 99 ? saveSlot : slot;
    camFocus = pos;
    weatherNext = weather;
    return true;
}

std::string Game::saveSummary(int slot) const {
    std::string data;
    if (!platform::readFile(slotPath(slot), data)) return "";
    std::istringstream in(data);
    std::string line;
    int lvl = 1, g = 0, d = 1; double gt = 0, ps = 0; float tod = 0; int x = 0, sp = 0;
    vec3 pos;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        std::string k; l >> k;
        if (k == "prog") l >> lvl >> x >> g >> sp >> gt >> tod >> d >> ps;
        if (k == "player") l >> pos.x >> pos.y >> pos.z;
    }
    int reg_ = pos.x > WORLD_SIZE ? -1 : world.regionId(pos.x, pos.z);
    char b[160];
    snprintf(b, sizeof b, "Niveau %d · %s · Jour %d · %dh%02d de jeu", lvl, reg_ >= 0 ? regionName(reg_) : "Donjon", d, (int)(ps / 3600), (int)std::fmod(ps / 60, 60));
    return b;
}

void Game::saveSettings() {
    std::ostringstream o;
    o << "profile " << (int)settings.profile << "\n";
    o << "volumes " << settings.master << " " << settings.music << " " << settings.sfx << " " << settings.ambience << "\n";
    o << "camera " << settings.camSensitivity << " " << settings.joystickDeadzone << " " << (settings.invertCamera ? 1 : 0) << " " << (settings.cameraShake ? 1 : 0) << "\n";
    o << "flags " << settings.haptics << " " << settings.showFps << " " << settings.autoTarget << " " << settings.chromatic << "\n";
    o << "layoutver " << LAYOUT_VERSION << "\n";
    o << "layout " << settings.layout.scale << " " << settings.layout.opacity << " " << settings.layout.leftHanded;
    for (int i = 0; i < CTL_COUNT; i++) o << " " << settings.layout.pos[i].x << " " << settings.layout.pos[i].y;
    o << "\n";
    platform::writeFileAtomic(platform::writableDir() + "/settings.cfg", o.str());
}

void Game::loadSettings() {
    // default control layout (bottom-right cluster), in UI units relative to the bottom-right corner
    for (int i = 0; i < CTL_COUNT; i++) settings.layout.pos[i] = defaultLayoutPos()[i];
    int layoutVer = 1;
    std::string data;
    if (!platform::readFile(platform::writableDir() + "/settings.cfg", data)) return;
    std::istringstream in(data);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        std::string k; l >> k;
        if (k == "profile") { int p; l >> p; settings.profile = (QualityProfile)clampf((float)p, 0, 4); }
        else if (k == "volumes") l >> settings.master >> settings.music >> settings.sfx >> settings.ambience;
        else if (k == "camera") { int inv, sh; l >> settings.camSensitivity >> settings.joystickDeadzone >> inv >> sh; settings.invertCamera = inv; settings.cameraShake = sh; }
        else if (k == "flags") l >> settings.haptics >> settings.showFps >> settings.autoTarget >> settings.chromatic;
        else if (k == "layoutver") l >> layoutVer;
        else if (k == "layout") {
            vec2 pos[CTL_COUNT];
            l >> settings.layout.scale >> settings.layout.opacity >> settings.layout.leftHanded;
            for (int i = 0; i < CTL_COUNT; i++) l >> pos[i].x >> pos[i].y;
            if (layoutVer >= LAYOUT_VERSION && l) for (int i = 0; i < CTL_COUNT; i++) settings.layout.pos[i] = pos[i];
        }
    }
    // layouts saved by older versions (overlapping buttons) are replaced by the new default positions
    settings.layout.scale = clampf(settings.layout.scale, 0.7f, 1.4f);
    settings.layout.opacity = clampf(settings.layout.opacity, 0.3f, 1.f);
}

void Game::applyGraphicsProfile(QualityProfile p) {
    settings.profile = p;
    GraphicsSettings gs = GraphicsSettings::forProfile(p);
    gs.chromatic = settings.chromatic;
    renderer.applySettings(gs);
    particles.setBudget(gs.particleScale);
    world.grassDensity = gs.grassDensity;
    world.detailedFoliage = settings.profile <= QualityProfile::Medium;
    world.unloadAll();   // rebuild chunks with the new grass density
    saveSettings();
}

}  // namespace lk
