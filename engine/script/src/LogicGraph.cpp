// Visual logic graphs: catalog of node types, editing operations, JSON format and the Lua code
// generator. The generated script is an ordinary AGR script class (properties = variables).
#include "agr/script/LogicGraph.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <functional>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

namespace agr::logic {

using json = nlohmann::json;

const char* enumName(PinKind kind) {
    switch (kind) {
        case PinKind::Exec: return "exec";
        case PinKind::Number: return "number";
        case PinKind::Text: return "text";
        case PinKind::Bool: return "bool";
        case PinKind::Entity: return "entity";
        case PinKind::Any: return "any";
    }
    return "any";
}

namespace {

PinKind parseKind(const std::string& s) {
    for (PinKind k : {PinKind::Exec, PinKind::Number, PinKind::Text, PinKind::Bool, PinKind::Entity, PinKind::Any})
        if (s == enumName(k)) return k;
    return PinKind::Any;
}

// ---------------------------------------------------------------------------- catalog
PinDef exec(const char* id = "next", const char* label = "") { return {id, label, PinKind::Exec, {}, {}}; }
PinDef num(const char* id, const char* label, const char* def = "0") { return {id, label, PinKind::Number, def, {}}; }
PinDef text(const char* id, const char* label, const char* def = "") { return {id, label, PinKind::Text, def, {}}; }
PinDef boolean(const char* id, const char* label, const char* def = "true") { return {id, label, PinKind::Bool, def, {}}; }
PinDef entity(const char* id = "entity", const char* label = "Entité") { return {id, label, PinKind::Entity, "", {}}; }
PinDef any(const char* id, const char* label, const char* def = "") { return {id, label, PinKind::Any, def, {}}; }
PinDef choice(const char* id, const char* label, std::vector<std::string> values) {
    PinDef p{id, label, PinKind::Text, values.front(), {}};
    p.choices = std::move(values);
    return p;
}

struct Gen;
struct Emit {
    std::string& code;
    int indent;
    void line(const std::string& s) const { code += std::string(static_cast<usize>(indent) * 4, ' ') + s + "\n"; }
};

/// How a node turns into Lua: statements (exec nodes) and / or expressions per output.
struct Codegen {
    std::function<void(Gen&, const Node&, const Emit&)> stmt;                  ///< exec nodes: body, then follow outputs
    std::function<std::string(Gen&, const Node&, const std::string&)> value;  ///< pure nodes: expression of an output
};

std::map<std::string, Codegen>& codegens() {
    static std::map<std::string, Codegen> c;
    return c;
}

std::vector<NodeDef>& catalogStorage() {
    static std::vector<NodeDef> c;
    return c;
}

void define(NodeDef def, Codegen gen) {
    codegens()[def.type] = std::move(gen);
    catalogStorage().push_back(std::move(def));
}

std::string luaString(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': break;
            default: out += c;
        }
    }
    return out + "\"";
}

bool isNumber(const std::string& s, double* out = nullptr) {
    if (s.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size()) return false;
    if (out) *out = v;
    return true;
}

bool isIdentifier(const std::string& s) {
    if (s.empty() || std::isdigit(static_cast<unsigned char>(s[0]))) return false;
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
}

// ---------------------------------------------------------------------------- generator
struct Gen {
    const Graph& g;
    std::vector<std::string> errors, warnings;
    std::set<int> stack;  // exec nodes being generated (cycle detection)
    bool usesTimers = false, usesOnce = false;

    std::string where(const Node& n) const {
        const NodeDef* d = findNodeDef(n.type);
        return "« " + (d ? d->title : n.type) + " » (n° " + std::to_string(n.id) + ")";
    }

    std::string value(const Node& n, const std::string& field) const {
        auto it = n.values.find(field);
        if (it != n.values.end()) return it->second;
        if (const NodeDef* d = findNodeDef(n.type)) {
            for (const PinDef& p : d->fields)
                if (p.id == field) return p.defaultValue;
            for (const PinDef& p : d->inputs)
                if (p.id == field) return p.defaultValue;
        }
        return {};
    }

    /// Lua literal of an inline value of the given kind.
    std::string literal(const Node& n, const std::string& v, PinKind kind) {
        switch (kind) {
            case PinKind::Number: {
                double d = 0.0;
                if (v.empty()) return "0";
                if (!isNumber(v, &d)) {
                    errors.push_back(where(n) + " : « " + v + " » n'est pas un nombre");
                    return "0";
                }
                return v;
            }
            case PinKind::Bool: return (v == "true" || v == "1" || v == "vrai") ? "true" : "false";
            case PinKind::Entity: return v.empty() ? "self.entity" : "agr.find(" + luaString(v) + ")";
            case PinKind::Text: return luaString(v);
            case PinKind::Any: {
                if (v == "true" || v == "false") return v;
                return isNumber(v) ? v : luaString(v);
            }
            case PinKind::Exec: break;
        }
        return "nil";
    }

    /// Expression feeding an input: the linked output, or the inline value.
    std::string in(const Node& n, const std::string& pin) {
        const NodeDef* d = findNodeDef(n.type);
        PinKind kind = PinKind::Any;
        for (const PinDef& p : d->inputs)
            if (p.id == pin) kind = p.kind;
        if (const Link* l = g.linkTo(n.id, pin)) {
            const Node* src = g.node(l->fromNode);
            if (!src) return "nil";
            std::string e = out(*src, l->fromPin);
            if (kind == PinKind::Text) {
                const NodeDef* sd = findNodeDef(src->type);
                for (const PinDef& p : sd->outputs)
                    if (p.id == l->fromPin && p.kind != PinKind::Text) e = "tostring(" + e + ")";
            }
            return e;
        }
        return literal(n, value(n, pin), kind);
    }

    /// Expression of a data output.
    std::string out(const Node& src, const std::string& pin) {
        const NodeDef* d = findNodeDef(src.type);
        if (d->event) {  // event parameters are the function's arguments
            if (pin == "dt") return "dt";
            if (pin == "other") return "other";
            if (pin == "sender") return "sender";
            if (pin == "value") return "value";
            return "nil";
        }
        if (d->pure) {
            if (stack.count(-src.id)) {
                errors.push_back(where(src) + " : les valeurs forment une boucle");
                return "nil";
            }
            stack.insert(-src.id);
            std::string e = codegens()[src.type].value(*this, src, pin);
            stack.erase(-src.id);
            return e;
        }
        return "v" + std::to_string(src.id) + "_" + pin;  // value produced by an action (local)
    }

    /// Statements of the chain leaving `n` through the exec output `pin`.
    void follow(const Node& n, const std::string& pin, const Emit& e) {
        const Link* l = g.linkFrom(n.id, pin);
        if (!l) return;
        const Node* next = g.node(l->toNode);
        if (!next) return;
        if (stack.count(next->id)) {
            errors.push_back(where(*next) + " : l'exécution revient en boucle sur ce nœud (utilisez « Répéter »)");
            return;
        }
        stack.insert(next->id);
        const auto& cg = codegens()[next->type];
        if (cg.stmt) cg.stmt(*this, *next, e);
        stack.erase(next->id);
    }
};

/// Simple exec node: one line of Lua, then the "next" output.
Codegen action(std::function<std::string(Gen&, const Node&)> line) {
    return {[line](Gen& g, const Node& n, const Emit& e) {
                e.line(line(g, n));
                g.follow(n, "next", e);
            },
            {}};
}

Codegen pure(std::function<std::string(Gen&, const Node&, const std::string&)> v) { return {{}, std::move(v)}; }

NodeDef node(const char* type, const char* title, const char* category, const char* tooltip, std::vector<PinDef> fields, std::vector<PinDef> inputs,
             std::vector<PinDef> outputs) {
    NodeDef d;
    d.type = type;
    d.title = title;
    d.category = category;
    d.tooltip = tooltip;
    d.fields = std::move(fields);
    d.inputs = std::move(inputs);
    d.outputs = std::move(outputs);
    return d;
}

NodeDef eventNode(const char* type, const char* title, const char* tooltip, std::vector<PinDef> fields, std::vector<PinDef> outputs) {
    NodeDef d = node(type, title, "Événements", tooltip, std::move(fields), {}, std::move(outputs));
    d.event = true;
    return d;
}

NodeDef actionNode(const char* type, const char* title, const char* category, const char* tooltip, std::vector<PinDef> fields, std::vector<PinDef> inputs,
                   std::vector<PinDef> extraOutputs = {}) {
    inputs.insert(inputs.begin(), exec("in"));
    extraOutputs.insert(extraOutputs.begin(), exec("next"));
    return node(type, title, category, tooltip, std::move(fields), std::move(inputs), std::move(extraOutputs));
}

NodeDef valueNode(const char* type, const char* title, const char* category, const char* tooltip, std::vector<PinDef> fields, std::vector<PinDef> inputs,
                  std::vector<PinDef> outputs) {
    NodeDef d = node(type, title, category, tooltip, std::move(fields), std::move(inputs), std::move(outputs));
    d.pure = true;
    return d;
}

std::string varName(Gen& g, const Node& n) {
    const std::string name = g.value(n, "name");
    if (!isIdentifier(name)) g.errors.push_back(g.where(n) + " : nom de variable invalide « " + name + " » (lettres, chiffres, _)");
    return name;
}

void buildCatalog() {
    if (!catalogStorage().empty()) return;
    // ------------------------------------------------------------------ events
    const auto chainOnly = Codegen{};
    define(eventNode("event.start", "Au démarrage", "Une fois, quand l'entité apparaît (onStart).", {}, {exec("next")}), chainOnly);
    define(eventNode("event.update", "À chaque image", "À chaque image (onUpdate) ; dt = durée de l'image en secondes.", {},
                     {exec("next"), num("dt", "dt")}),
           chainOnly);
    define(eventNode("event.every", "Toutes les N secondes", "Minuteur répété.", {num("seconds", "Secondes", "1")}, {exec("next")}), chainOnly);
    define(eventNode("event.keyPressed", "Touche appuyée", "Au moment où la touche est enfoncée (Space, A, Left, Return...).", {text("key", "Touche", "Space")},
                     {exec("next")}),
           chainOnly);
    define(eventNode("event.keyReleased", "Touche relâchée", "Au moment où la touche est relâchée.", {text("key", "Touche", "Space")}, {exec("next")}),
           chainOnly);
    define(eventNode("event.action", "Action appuyée", "Action d'entrée du projet (clavier, manette, tactile) : jump, fire...",
                     {text("action", "Action", "jump")}, {exec("next")}),
           chainOnly);
    define(eventNode("event.collision", "Collision", "Quand un corps physique touche cette entité.", {}, {exec("next"), entity("other", "Autre")}),
           chainOnly);
    define(eventNode("event.triggerEnter", "Entrée dans la zone", "Quand une entité entre dans ce déclencheur.", {},
                     {exec("next"), entity("other", "Autre")}),
           chainOnly);
    define(eventNode("event.triggerExit", "Sortie de la zone", "Quand une entité sort de ce déclencheur.", {}, {exec("next"), entity("other", "Autre")}),
           chainOnly);
    define(eventNode("event.click", "Clic sur le bouton", "Quand ce bouton d'interface est cliqué ou touché.", {}, {exec("next")}), chainOnly);
    define(eventNode("event.message", "Message reçu", "Appelé par « Envoyer un message » ou par un bouton (champ message).",
                     {text("name", "Nom", "onMessage")}, {exec("next"), entity("sender", "Expéditeur"), any("value", "Valeur")}),
           chainOnly);

    // ------------------------------------------------------------------ flow
    define(node("flow.branch", "Si", "Flux", "Suit « Vrai » ou « Faux » selon la condition.", {}, {exec("in"), boolean("condition", "Condition")},
                {exec("true", "Vrai"), exec("false", "Faux")}),
           {[](Gen& g, const Node& n, const Emit& e) {
                e.line("if " + g.in(n, "condition") + " then");
                g.follow(n, "true", {e.code, e.indent + 1});
                if (g.g.linkFrom(n.id, "false")) {
                    e.line("else");
                    g.follow(n, "false", {e.code, e.indent + 1});
                }
                e.line("end");
            },
            {}});
    define(node("flow.sequence", "Séquence", "Flux", "Exécute les sorties dans l'ordre.", {}, {exec("in")},
                {exec("then1", "Puis 1"), exec("then2", "Puis 2"), exec("then3", "Puis 3")}),
           {[](Gen& g, const Node& n, const Emit& e) {
                for (const char* p : {"then1", "then2", "then3"}) g.follow(n, p, e);
            },
            {}});
    define(node("flow.repeat", "Répéter", "Flux", "Exécute « Corps » N fois (index de 1 à N), puis « Fini ».", {}, {exec("in"), num("count", "Fois", "3")},
                {exec("body", "Corps"), num("index", "Index"), exec("done", "Fini")}),
           {[](Gen& g, const Node& n, const Emit& e) {
                e.line("for v" + std::to_string(n.id) + "_index = 1, " + g.in(n, "count") + " do");
                g.follow(n, "body", {e.code, e.indent + 1});
                e.line("end");
                g.follow(n, "done", e);
            },
            {}});
    define(node("flow.forEachChild", "Pour chaque enfant", "Flux", "Exécute « Corps » pour chaque enfant de l'entité.", {}, {exec("in"), entity("parent", "Parent")},
                {exec("body", "Corps"), entity("child", "Enfant"), exec("done", "Fini")}),
           {[](Gen& g, const Node& n, const Emit& e) {
                e.line("for _, v" + std::to_string(n.id) + "_child in ipairs(" + g.in(n, "parent") + ":children()) do");
                g.follow(n, "body", {e.code, e.indent + 1});
                e.line("end");
                g.follow(n, "done", e);
            },
            {}});
    define(node("flow.once", "Une seule fois", "Flux", "Ne laisse passer qu'une fois (par entité).", {}, {exec("in")}, {exec("next")}),
           {[](Gen& g, const Node& n, const Emit& e) {
                const std::string flag = "self.__once" + std::to_string(n.id);
                e.line("if not " + flag + " then");
                e.line("    " + flag + " = true");
                g.follow(n, "next", {e.code, e.indent + 1});
                e.line("end");
            },
            {}});
    define(node("flow.delay", "Après un délai", "Flux", "Continue après N secondes (sans bloquer le jeu).", {}, {exec("in"), num("seconds", "Secondes", "1")},
                {exec("next")}),
           {[](Gen& g, const Node& n, const Emit& e) {
                g.usesTimers = true;
                e.line("self.__timers = self.__timers or {}");
                e.line("table.insert(self.__timers, { t = " + g.in(n, "seconds") + ", f = function()");
                g.follow(n, "next", {e.code, e.indent + 1});
                e.line("end })");
            },
            {}});

    // ------------------------------------------------------------------ actions
    const auto xyz = [](const char* def = "0") { return std::vector<PinDef>{num("x", "X", def), num("y", "Y", def), num("z", "Z", def)}; };
    auto withEntity = [](std::vector<PinDef> v) {
        v.insert(v.begin(), entity());
        return v;
    };
    define(actionNode("act.setPosition", "Placer", "Actions", "Position (locale) de l'entité.", {}, withEntity(xyz())),
           action([](Gen& g, const Node& n) { return g.in(n, "entity") + ":setPosition(" + g.in(n, "x") + ", " + g.in(n, "y") + ", " + g.in(n, "z") + ")"; }));
    define(actionNode("act.move", "Déplacer", "Actions", "Ajoute (x, y, z) à la position ; « par seconde » multiplie par la durée de l'image.", {},
                      [&] {
                          auto v = withEntity(xyz());
                          v.push_back(boolean("perSecond", "Par seconde", "true"));
                          return v;
                      }()),
           action([](Gen& g, const Node& n) {
               const std::string k = "(" + g.in(n, "perSecond") + " and agr.time.delta() or 1)";
               return g.in(n, "entity") + ":translate(" + g.in(n, "x") + " * " + k + ", " + g.in(n, "y") + " * " + k + ", " + g.in(n, "z") + " * " + k + ")";
           }));
    define(actionNode("act.setRotation", "Orienter", "Actions", "Rotation en degrés (X, Y, Z).", {}, withEntity(xyz())),
           action([](Gen& g, const Node& n) { return g.in(n, "entity") + ":setRotation(" + g.in(n, "x") + ", " + g.in(n, "y") + ", " + g.in(n, "z") + ")"; }));
    define(actionNode("act.rotate", "Tourner", "Actions", "Ajoute une rotation en degrés par seconde.", {}, withEntity(xyz())),
           action([](Gen& g, const Node& n) {
               const std::string k = " * agr.time.delta()";
               return g.in(n, "entity") + ":rotate(" + g.in(n, "x") + k + ", " + g.in(n, "y") + k + ", " + g.in(n, "z") + k + ")";
           }));
    define(actionNode("act.setScale", "Taille", "Actions", "Échelle de l'entité.", {}, withEntity(xyz("1"))),
           action([](Gen& g, const Node& n) { return g.in(n, "entity") + ":setScale(" + g.in(n, "x") + ", " + g.in(n, "y") + ", " + g.in(n, "z") + ")"; }));
    define(actionNode("act.setVelocity", "Vitesse", "Actions", "Vitesse du corps physique (2D : x, y).", {}, withEntity(xyz())),
           action([](Gen& g, const Node& n) { return g.in(n, "entity") + ":setVelocity(" + g.in(n, "x") + ", " + g.in(n, "y") + ", " + g.in(n, "z") + ")"; }));
    define(actionNode("act.impulse", "Impulsion", "Actions", "Pousse le corps physique d'un coup.", {}, withEntity(xyz())),
           action([](Gen& g, const Node& n) { return g.in(n, "entity") + ":applyImpulse(" + g.in(n, "x") + ", " + g.in(n, "y") + ", " + g.in(n, "z") + ")"; }));
    define(actionNode("act.setActive", "Afficher / cacher", "Actions", "Active ou désactive l'entité (et ses enfants).", {},
                      {entity(), boolean("active", "Active", "false")}),
           action([](Gen& g, const Node& n) { return g.in(n, "entity") + ":setActive(" + g.in(n, "active") + ")"; }));
    define(actionNode("act.destroy", "Détruire", "Actions", "Supprime l'entité à la fin de l'image.", {}, {entity()}),
           action([](Gen& g, const Node& n) { return g.in(n, "entity") + ":destroy()"; }));
    define(actionNode("act.clone", "Cloner", "Actions", "Copie une entité (prototype caché) à une position ; sortie : la copie.", {},
                      [&] {
                          auto v = std::vector<PinDef>{entity("entity", "Modèle")};
                          for (PinDef p : xyz()) v.push_back(p);
                          return v;
                      }(),
                      {entity("copy", "Copie")}),
           {[](Gen& g, const Node& n, const Emit& e) {
                const std::string v = "v" + std::to_string(n.id) + "_copy";
                e.line("local " + v + " = agr.clone(" + g.in(n, "entity") + ", false)");
                e.line(v + ":setPosition(" + g.in(n, "x") + ", " + g.in(n, "y") + ", " + g.in(n, "z") + ")");
                g.follow(n, "next", e);
            },
            {}});
    define(actionNode("act.setText", "Changer le texte", "Actions", "Texte d'un élément d'interface (UIText).", {}, {entity(), text("text", "Texte", "")}),
           action([](Gen& g, const Node& n) { return g.in(n, "entity") + ":setText(" + g.in(n, "text") + ")"; }));
    define(actionNode("act.sound", "Jouer un son", "Actions", "Son ponctuel (bus Effets).", {},
                      {text("sound", "Son", "assets://audio/coin.wav"), num("volume", "Volume", "1")}),
           action([](Gen& g, const Node& n) { return "agr.audio.play(" + g.in(n, "sound") + ", { volume = " + g.in(n, "volume") + " })"; }));
    define(actionNode("act.log", "Écrire dans la console", "Actions", "Message dans la console de l'éditeur.", {}, {any("text", "Texte", "Bonjour")}),
           action([](Gen& g, const Node& n) { return "agr.log(tostring(" + g.in(n, "text") + "))"; }));
    define(actionNode("act.setVar", "Changer une variable", "Actions", "Donne une valeur à une variable du graphe.", {text("name", "Variable", "score")},
                      {any("value", "Valeur", "0")}),
           action([](Gen& g, const Node& n) { return "self." + varName(g, n) + " = " + g.in(n, "value"); }));
    define(actionNode("act.addVar", "Ajouter à une variable", "Actions", "Ajoute un nombre à une variable (score, vies...).", {text("name", "Variable", "score")},
                      {num("amount", "Quantité", "1")}),
           action([](Gen& g, const Node& n) {
               const std::string v = "self." + varName(g, n);
               return v + " = (" + v + " or 0) + " + g.in(n, "amount");
           }));
    define(actionNode("act.loadScene", "Changer de scène", "Actions", "Charge une autre scène à la fin de l'image.", {},
                      {text("scene", "Scène", "game://scenes/main.agrscene")}),
           action([](Gen& g, const Node& n) { return "agr.loadScene(" + g.in(n, "scene") + ")"; }));
    define(actionNode("act.send", "Envoyer un message", "Actions", "Appelle la fonction « Nom » du script de la cible (nœud « Message reçu »).",
                      {text("name", "Nom", "onMessage")}, {entity("target", "Cible"), any("value", "Valeur", "")}),
           {[](Gen& g, const Node& n, const Emit& e) {
                const std::string name = g.value(n, "name");
                if (!isIdentifier(name)) g.errors.push_back(g.where(n) + " : nom de message invalide « " + name + " »");
                const std::string s = "v" + std::to_string(n.id) + "_script";
                e.line("local " + s + " = " + g.in(n, "target") + ":script()");
                e.line("if " + s + " and " + s + "." + name + " then " + s + ":" + name + "(self.entity, " + g.in(n, "value") + ") end");
                g.follow(n, "next", e);
            },
            {}});
    define(actionNode("act.animParam", "Paramètre d'animation", "Actions", "Change un paramètre de l'Animator (machine à états).", {},
                      {entity(), text("param", "Paramètre", "speed"), any("value", "Valeur", "1")}),
           action([](Gen& g, const Node& n) { return g.in(n, "entity") + ":setParameter(" + g.in(n, "param") + ", " + g.in(n, "value") + ")"; }));
    define(actionNode("act.animTrigger", "Déclencher une animation", "Actions", "Active un déclencheur de l'Animator.", {},
                      {entity(), text("param", "Déclencheur", "attack")}),
           action([](Gen& g, const Node& n) { return g.in(n, "entity") + ":setTrigger(" + g.in(n, "param") + ")"; }));
    define(actionNode("act.save", "Sauvegarder", "Actions", "Enregistre une valeur dans la sauvegarde locale.", {}, {text("key", "Clé", "record"), any("value", "Valeur", "0")}),
           action([](Gen& g, const Node& n) { return "agr.save.set(" + g.in(n, "key") + ", " + g.in(n, "value") + ")"; }));
    define(actionNode("act.vibrate", "Vibrer", "Actions", "Vibreur du téléphone (sans effet ailleurs).", {}, {num("seconds", "Secondes", "0.1")}),
           action([](Gen& g, const Node& n) { return "agr.device.vibrate(" + g.in(n, "seconds") + ")"; }));

    // ------------------------------------------------------------------ effects
    define(actionNode("fx.tween", "Animer", "Effets", "Animation interpolée d'une propriété (agr.tween).",
                      {choice("property", "Propriété", {"position", "scale", "rotation", "alpha"}),
                       choice("ease", "Courbe", {"outQuad", "linear", "inOutSine", "outBack", "outElastic", "outBounce", "inQuad"})},
                      [&] {
                          auto v = withEntity(xyz("1"));
                          v.push_back(num("duration", "Durée", "0.5"));
                          return v;
                      }()),
           action([](Gen& g, const Node& n) {
               const std::string prop = g.value(n, "property");
               const std::string target = prop == "alpha" ? g.in(n, "x") : "{ " + g.in(n, "x") + ", " + g.in(n, "y") + ", " + g.in(n, "z") + " }";
               return "agr.tween(" + g.in(n, "entity") + ", { " + prop + " = " + target + " }, " + g.in(n, "duration") + ", { ease = " +
                      luaString(g.value(n, "ease")) + " })";
           }));
    define(actionNode("fx.burst", "Gerbe de particules", "Effets", "Particules lancées d'un point (impacts, ramassages).", {},
                      {num("x", "X"), num("y", "Y"), num("z", "Z"), num("count", "Nombre", "20")}),
           action([](Gen& g, const Node& n) {
               return "agr.fx.burst(" + g.in(n, "x") + ", " + g.in(n, "y") + ", " + g.in(n, "z") + ", { count = " + g.in(n, "count") + " })";
           }));
    define(actionNode("fx.shake", "Secouer la caméra", "Effets", "Tremblement de la caméra (force 0 à 1).", {}, {num("strength", "Force", "0.4")}),
           action([](Gen& g, const Node& n) { return "agr.fx.shake(" + g.in(n, "strength") + ")"; }));
    define(actionNode("fx.popup", "Texte flottant", "Effets", "Texte qui monte d'un point du monde (« +1 », dégâts).", {},
                      {num("x", "X"), num("y", "Y"), num("z", "Z"), any("text", "Texte", "+1")}),
           action([](Gen& g, const Node& n) {
               return "agr.fx.popup(" + g.in(n, "x") + ", " + g.in(n, "y") + ", " + g.in(n, "z") + ", tostring(" + g.in(n, "text") + "))";
           }));
    define(actionNode("fx.flash", "Flash d'écran", "Effets", "L'écran clignote d'une couleur.", {}, {num("r", "Rouge", "1"), num("g", "Vert", "1"), num("b", "Bleu", "1")}),
           action([](Gen& g, const Node& n) { return "agr.fx.flash(" + g.in(n, "r") + ", " + g.in(n, "g") + ", " + g.in(n, "b") + ")"; }));

    // ------------------------------------------------------------------ values
    define(valueNode("val.self", "Moi", "Valeurs", "L'entité qui porte ce graphe.", {}, {}, {entity("entity", "Entité")}),
           pure([](Gen&, const Node&, const std::string&) { return std::string("self.entity"); }));
    define(valueNode("val.find", "Entité nommée", "Valeurs", "Première entité de la scène avec ce nom.", {}, {text("name", "Nom", "Hero")}, {entity("entity", "Entité")}),
           pure([](Gen& g, const Node& n, const std::string&) { return "agr.find(" + g.in(n, "name") + ")"; }));
    define(valueNode("val.number", "Nombre", "Valeurs", "Une valeur fixe.", {}, {num("value", "Valeur", "1")}, {num("out", "Nombre")}),
           pure([](Gen& g, const Node& n, const std::string&) { return g.in(n, "value"); }));
    define(valueNode("val.text", "Texte", "Valeurs", "Un texte fixe.", {}, {text("value", "Texte", "Bonjour")}, {text("out", "Texte")}),
           pure([](Gen& g, const Node& n, const std::string&) { return g.in(n, "value"); }));
    define(valueNode("val.bool", "Vrai / faux", "Valeurs", "Un booléen fixe.", {}, {boolean("value", "Valeur", "true")}, {boolean("out", "Valeur")}),
           pure([](Gen& g, const Node& n, const std::string&) { return g.in(n, "value"); }));
    define(valueNode("val.var", "Variable", "Valeurs", "Valeur d'une variable du graphe.", {text("name", "Variable", "score")}, {}, {any("out", "Valeur")}),
           pure([](Gen& g, const Node& n, const std::string&) { return "self." + varName(g, n); }));
    define(valueNode("val.position", "Position", "Valeurs", "Position (locale) d'une entité.", {}, {entity()}, {num("x", "X"), num("y", "Y"), num("z", "Z")}),
           pure([](Gen& g, const Node& n, const std::string& pin) {
               const int i = pin == "x" ? 1 : pin == "y" ? 2 : 3;
               return "(select(" + std::to_string(i) + ", " + g.in(n, "entity") + ":position()))";
           }));
    define(valueNode("val.velocity", "Vitesse actuelle", "Valeurs", "Vitesse du corps physique.", {}, {entity()}, {num("x", "X"), num("y", "Y"), num("z", "Z")}),
           pure([](Gen& g, const Node& n, const std::string& pin) {
               const int i = pin == "x" ? 1 : pin == "y" ? 2 : 3;
               return "(select(" + std::to_string(i) + ", " + g.in(n, "entity") + ":velocity()) or 0)";
           }));
    define(valueNode("val.name", "Nom de l'entité", "Valeurs", "Nom d'une entité.", {}, {entity()}, {text("out", "Nom")}),
           pure([](Gen& g, const Node& n, const std::string&) { return g.in(n, "entity") + ":name()"; }));
    define(valueNode("val.active", "Est active", "Valeurs", "L'entité est-elle active ?", {}, {entity()}, {boolean("out", "Active")}),
           pure([](Gen& g, const Node& n, const std::string&) { return g.in(n, "entity") + ":isActive()"; }));
    define(valueNode("val.keyDown", "Touche enfoncée", "Valeurs", "Vrai tant que la touche est enfoncée.", {text("key", "Touche", "Right")}, {},
                     {boolean("out", "Enfoncée")}),
           pure([](Gen& g, const Node& n, const std::string&) { return "agr.input.keyDown(" + luaString(g.value(n, "key")) + ")"; }));
    define(valueNode("val.axis", "Axe d'entrée", "Valeurs", "Valeur d'une action (move_x, move_y : -1 à 1).", {text("action", "Action", "move_x")}, {},
                     {num("out", "Valeur")}),
           pure([](Gen& g, const Node& n, const std::string&) { return "agr.input.value(" + luaString(g.value(n, "action")) + ")"; }));
    define(valueNode("val.time", "Temps", "Valeurs", "Secondes depuis le début, durée de l'image.", {}, {}, {num("total", "Total"), num("delta", "Image")}),
           pure([](Gen&, const Node&, const std::string& pin) { return std::string(pin == "total" ? "agr.time.total()" : "agr.time.delta()"); }));
    define(valueNode("val.random", "Hasard", "Valeurs", "Nombre au hasard entre Min et Max.", {}, {num("min", "Min", "0"), num("max", "Max", "1")}, {num("out", "Nombre")}),
           pure([](Gen& g, const Node& n, const std::string&) {
               return "(" + g.in(n, "min") + " + math.random() * (" + g.in(n, "max") + " - " + g.in(n, "min") + "))";
           }));
    define(valueNode("val.load", "Valeur sauvegardée", "Valeurs", "Valeur de la sauvegarde locale (ou la valeur par défaut).", {},
                     {text("key", "Clé", "record"), any("default", "Défaut", "0")}, {any("out", "Valeur")}),
           pure([](Gen& g, const Node& n, const std::string&) { return "agr.save.get(" + g.in(n, "key") + ", " + g.in(n, "default") + ")"; }));

    // ------------------------------------------------------------------ maths and logic
    auto binary = [&](const char* type, const char* title, const char* op, const char* tip, PinKind kind) {
        PinDef a = kind == PinKind::Number ? num("a", "A", "0") : boolean("a", "A", "true");
        PinDef b = kind == PinKind::Number ? num("b", "B", "0") : boolean("b", "B", "true");
        PinDef o = kind == PinKind::Number ? num("out", "Résultat") : boolean("out", "Résultat");
        const std::string opStr = op;
        define(valueNode(type, title, "Maths", tip, {}, {a, b}, {o}),
               pure([opStr](Gen& g, const Node& n, const std::string&) { return "(" + g.in(n, "a") + " " + opStr + " " + g.in(n, "b") + ")"; }));
    };
    binary("math.add", "Additionner", "+", "A + B", PinKind::Number);
    binary("math.sub", "Soustraire", "-", "A - B", PinKind::Number);
    binary("math.mul", "Multiplier", "*", "A × B", PinKind::Number);
    binary("math.div", "Diviser", "/", "A ÷ B", PinKind::Number);
    binary("math.and", "Et", "and", "Vrai si A et B sont vrais.", PinKind::Bool);
    binary("math.or", "Ou", "or", "Vrai si A ou B est vrai.", PinKind::Bool);
    define(valueNode("math.not", "Non", "Maths", "Inverse vrai / faux.", {}, {boolean("a", "A", "true")}, {boolean("out", "Résultat")}),
           pure([](Gen& g, const Node& n, const std::string&) { return "(not " + g.in(n, "a") + ")"; }));
    define(valueNode("math.compare", "Comparer", "Maths", "Compare deux valeurs.", {choice("op", "Opération", {"==", "~=", "<", "<=", ">", ">="})},
                     {any("a", "A", "0"), any("b", "B", "0")}, {boolean("out", "Résultat")}),
           pure([](Gen& g, const Node& n, const std::string&) {
               std::string op = g.value(n, "op");
               static const std::set<std::string> ops{"==", "~=", "<", "<=", ">", ">="};
               if (!ops.count(op)) {
                   g.errors.push_back(g.where(n) + " : opération inconnue « " + op + " »");
                   op = "==";
               }
               return "(" + g.in(n, "a") + " " + op + " " + g.in(n, "b") + ")";
           }));
    define(valueNode("math.concat", "Assembler du texte", "Maths", "A suivi de B.", {}, {any("a", "A", "Score : "), any("b", "B", "")}, {text("out", "Texte")}),
           pure([](Gen& g, const Node& n, const std::string&) { return "(tostring(" + g.in(n, "a") + ") .. tostring(" + g.in(n, "b") + "))"; }));
    define(valueNode("math.distance", "Distance", "Maths", "Distance entre deux entités.", {}, {entity("a", "A"), entity("b", "B")}, {num("out", "Distance")}),
           pure([](Gen& g, const Node& n, const std::string&) {
               return "(function(a, b) local ax, ay, az = a:worldPosition(); local bx, by, bz = b:worldPosition(); return math.sqrt((ax - bx) ^ 2 + (ay - by) ^ 2 + (az - bz) ^ 2) end)(" +
                      g.in(n, "a") + ", " + g.in(n, "b") + ")";
           }));
    define(valueNode("math.clamp", "Limiter", "Maths", "Garde la valeur entre Min et Max.", {}, {num("value", "Valeur"), num("min", "Min", "0"), num("max", "Max", "1")},
                     {num("out", "Résultat")}),
           pure([](Gen& g, const Node& n, const std::string&) {
               return "math.max(" + g.in(n, "min") + ", math.min(" + g.in(n, "max") + ", " + g.in(n, "value") + "))";
           }));
}

} // namespace

const std::vector<NodeDef>& nodeCatalog() {
    buildCatalog();
    return catalogStorage();
}

const NodeDef* findNodeDef(std::string_view type) {
    for (const NodeDef& d : nodeCatalog())
        if (d.type == type) return &d;
    return nullptr;
}

bool compatible(PinKind from, PinKind to) {
    if (from == PinKind::Exec || to == PinKind::Exec) return from == to;
    if (from == to || from == PinKind::Any || to == PinKind::Any) return true;
    return to == PinKind::Text;  // numbers and booleans are written as text
}

// ---------------------------------------------------------------------------- graph editing
Node* Graph::node(int id) {
    for (Node& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}
const Node* Graph::node(int id) const {
    for (const Node& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}

int Graph::addNode(const std::string& type, float x, float y) {
    const NodeDef* d = findNodeDef(type);
    if (!d) return 0;
    Node n;
    n.id = nextId++;
    n.type = type;
    n.x = x;
    n.y = y;
    for (const PinDef& p : d->fields) n.values[p.id] = p.defaultValue;
    for (const PinDef& p : d->inputs)
        if (p.kind != PinKind::Exec) n.values[p.id] = p.defaultValue;
    nodes.push_back(std::move(n));
    return nodes.back().id;
}

void Graph::removeNode(int id) {
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [id](const Node& n) { return n.id == id; }), nodes.end());
    links.erase(std::remove_if(links.begin(), links.end(), [id](const Link& l) { return l.fromNode == id || l.toNode == id; }), links.end());
}

namespace {
const PinDef* findPin(const std::vector<PinDef>& pins, const std::string& id) {
    for (const PinDef& p : pins)
        if (p.id == id) return &p;
    return nullptr;
}
} // namespace

Status Graph::connect(int fromNode, const std::string& fromPin, int toNode, const std::string& toPin) {
    const Node* a = node(fromNode);
    const Node* b = node(toNode);
    if (!a || !b) return makeError(Error::Code::NotFound, "nœud introuvable");
    if (fromNode == toNode) return makeError(Error::Code::InvalidArgument, "un nœud ne peut pas se relier à lui-même");
    const PinDef* out = findPin(findNodeDef(a->type)->outputs, fromPin);
    const PinDef* in = findPin(findNodeDef(b->type)->inputs, toPin);
    if (!out || !in) return makeError(Error::Code::InvalidArgument, "une liaison va d'une sortie vers une entrée");
    if (!compatible(out->kind, in->kind))
        return makeError(Error::Code::InvalidArgument, std::string("types incompatibles : ") + enumName(out->kind) + " vers " + enumName(in->kind));
    if (out->kind == PinKind::Exec) {  // one next step per exec output
        links.erase(std::remove_if(links.begin(), links.end(), [&](const Link& l) { return l.fromNode == fromNode && l.fromPin == fromPin; }), links.end());
    } else {  // one source per data input
        disconnect(toNode, toPin);
    }
    links.push_back({fromNode, fromPin, toNode, toPin});
    return {};
}

void Graph::disconnect(int toNode, const std::string& toPin) {
    links.erase(std::remove_if(links.begin(), links.end(), [&](const Link& l) { return l.toNode == toNode && l.toPin == toPin; }), links.end());
}

const Link* Graph::linkTo(int toNode, const std::string& toPin) const {
    for (const Link& l : links)
        if (l.toNode == toNode && l.toPin == toPin) return &l;
    return nullptr;
}
const Link* Graph::linkFrom(int fromNode, const std::string& fromPin) const {
    for (const Link& l : links)
        if (l.fromNode == fromNode && l.fromPin == fromPin) return &l;
    return nullptr;
}

std::string Graph::toJson() const {
    json j;
    j["format"] = "agr.logic";
    j["version"] = 1;
    j["nextId"] = nextId;
    j["variables"] = json::array();
    for (const Variable& v : variables) j["variables"].push_back({{"name", v.name}, {"type", enumName(v.type)}, {"value", v.value}});
    j["nodes"] = json::array();
    for (const Node& n : nodes) j["nodes"].push_back({{"id", n.id}, {"type", n.type}, {"x", n.x}, {"y", n.y}, {"values", n.values}});
    j["links"] = json::array();
    for (const Link& l : links) j["links"].push_back({{"from", l.fromNode}, {"fromPin", l.fromPin}, {"to", l.toNode}, {"toPin", l.toPin}});
    if (!comments.empty()) {
        j["comments"] = json::array();
        for (const Comment& c : comments) j["comments"].push_back({{"x", c.x}, {"y", c.y}, {"w", c.w}, {"h", c.h}, {"text", c.text}, {"color", c.color}});
    }
    return j.dump(2);
}

Result<Graph> Graph::fromJson(std::string_view text) {
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return makeError(Error::Code::ParseError, "graphe de logique : JSON invalide");
    if (j.value("format", "") != "agr.logic") return makeError(Error::Code::ParseError, "graphe de logique : format inconnu");
    if (j.value("version", 0) > 1) return makeError(Error::Code::VersionMismatch, "graphe de logique : version plus récente que le moteur");
    Graph g;
    try {
        for (const auto& v : j.value("variables", json::array()))
            g.variables.push_back({v.value("name", ""), parseKind(v.value("type", "number")), v.value("value", "")});
        for (const auto& n : j.value("nodes", json::array())) {
            Node node;
            node.id = n.value("id", 0);
            node.type = n.value("type", "");
            node.x = n.value("x", 0.0f);
            node.y = n.value("y", 0.0f);
            if (n.contains("values") && n["values"].is_object())
                for (auto it = n["values"].begin(); it != n["values"].end(); ++it)
                    node.values[it.key()] = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
            g.nodes.push_back(std::move(node));
        }
        for (const auto& l : j.value("links", json::array()))
            g.links.push_back({l.value("from", 0), l.value("fromPin", ""), l.value("to", 0), l.value("toPin", "")});
        for (const auto& c : j.value("comments", json::array()))
            g.comments.push_back({c.value("x", 0.0f), c.value("y", 0.0f), std::max(c.value("w", 320.0f), 40.0f), std::max(c.value("h", 200.0f), 30.0f),
                                  c.value("text", std::string()), c.value("color", 0x3C6E9Bu)});
        int maxId = 0;
        for (const Node& n : g.nodes) maxId = std::max(maxId, n.id);
        g.nextId = std::max(j.value("nextId", 1), maxId + 1);
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("graphe de logique : ") + e.what());
    }
    return g;
}

// ---------------------------------------------------------------------------- compiler
CompileResult compile(const Graph& graph, const std::string& sourceName) {
    Gen gen{graph, {}, {}, {}};
    // Unknown node types and dangling links.
    for (const Node& n : graph.nodes)
        if (!findNodeDef(n.type)) gen.errors.push_back("nœud de type inconnu « " + n.type + " » (n° " + std::to_string(n.id) + ")");
    for (const Link& l : graph.links)
        if (!graph.node(l.fromNode) || !graph.node(l.toNode)) gen.errors.push_back("liaison vers un nœud supprimé");
    if (!gen.errors.empty()) return {std::string(), gen.errors, gen.warnings};

    std::ostringstream os;
    os << "-- Script generated by AGR ENGINE from the visual logic graph " << (sourceName.empty() ? std::string("(unsaved)") : sourceName) << ".\n";
    os << "-- Do not edit: open the graph in the editor (Logique visuelle) instead.\n";
    os << "local L = { properties = {";
    bool first = true;
    std::set<std::string> varNames;
    for (const Variable& v : graph.variables) {
        if (!isIdentifier(v.name)) {
            gen.errors.push_back("variable au nom invalide « " + v.name + " »");
            continue;
        }
        if (!varNames.insert(v.name).second) gen.errors.push_back("variable « " + v.name + " » déclarée deux fois");
        std::string value;
        switch (v.type) {
            case PinKind::Number: value = isNumber(v.value) ? v.value : "0"; break;
            case PinKind::Bool: value = (v.value == "true" || v.value == "1") ? "true" : "false"; break;
            default: value = luaString(v.value);
        }
        os << (first ? " " : ", ") << v.name << " = " << value;
        first = false;
    }
    os << (first ? "" : " ") << "} }\n\n";

    // Group the events by Lua callback, in node order.
    std::map<std::string, std::vector<const Node*>> byCallback;
    std::vector<std::string> order;
    auto callbackOf = [](const Node& n, const Gen& g) -> std::string {
        if (n.type == "event.start") return "onStart";
        if (n.type == "event.collision") return "onCollision";
        if (n.type == "event.triggerEnter" || n.type == "event.triggerExit") return "onTrigger";
        if (n.type == "event.click") return "onClick";
        if (n.type == "event.message") return "msg:" + g.value(n, "name");
        return "onUpdate";  // update, every, key / action events
    };
    for (const Node& n : graph.nodes) {
        const NodeDef* d = findNodeDef(n.type);
        if (!d->event) continue;
        if (!graph.linkFrom(n.id, "next")) {
            gen.warnings.push_back(gen.where(n) + " : rien n'est relié à cet événement");
            continue;
        }
        const std::string cb = callbackOf(n, gen);
        if (!byCallback.count(cb)) order.push_back(cb);
        byCallback[cb].push_back(&n);
    }
    // Pre-generate the bodies (to know whether timers are used before writing onUpdate).
    std::map<std::string, std::string> bodies;
    for (const std::string& cb : order) {
        std::string body;
        for (const Node* n : byCallback[cb]) {
            const Emit top{body, 1};
            const std::string id = std::to_string(n->id);
            if (n->type == "event.every") {
                double s = 1.0;
                if (!isNumber(gen.value(*n, "seconds"), &s) || s <= 0.0) gen.errors.push_back(gen.where(*n) + " : durée invalide");
                top.line("self.__every" + id + " = (self.__every" + id + " or 0) + dt");
                top.line("if self.__every" + id + " >= " + gen.value(*n, "seconds") + " then");
                top.line("    self.__every" + id + " = self.__every" + id + " - " + gen.value(*n, "seconds"));
                gen.follow(*n, "next", {body, 2});
                top.line("end");
            } else if (n->type == "event.keyPressed" || n->type == "event.keyReleased") {
                top.line(std::string("if agr.input.") + (n->type == "event.keyPressed" ? "keyPressed(" : "keyReleased(") + luaString(gen.value(*n, "key")) + ") then");
                gen.follow(*n, "next", {body, 2});
                top.line("end");
            } else if (n->type == "event.action") {
                top.line("if agr.input.pressed(" + luaString(gen.value(*n, "action")) + ") then");
                gen.follow(*n, "next", {body, 2});
                top.line("end");
            } else if (n->type == "event.collision" || n->type == "event.triggerEnter") {
                top.line("if began then");
                gen.follow(*n, "next", {body, 2});
                top.line("end");
            } else if (n->type == "event.triggerExit") {
                top.line("if not began then");
                gen.follow(*n, "next", {body, 2});
                top.line("end");
            } else {
                gen.follow(*n, "next", top);
            }
        }
        bodies[cb] = body;
    }
    if (gen.usesTimers && !bodies.count("onUpdate")) {
        order.push_back("onUpdate");
        bodies["onUpdate"] = "";
    }
    for (const std::string& cb : order) {
        std::string sig;
        if (cb == "onStart") sig = "function L:onStart()";
        else if (cb == "onUpdate") sig = "function L:onUpdate(dt)";
        else if (cb == "onCollision") sig = "function L:onCollision(other, began)";
        else if (cb == "onTrigger") sig = "function L:onTrigger(other, began)";
        else if (cb == "onClick") sig = "function L:onClick()";
        else {
            const std::string name = cb.substr(4);
            if (!isIdentifier(name)) gen.errors.push_back("nom de message invalide « " + name + " »");
            sig = "function L:" + name + "(sender, value)";
        }
        os << sig << "\n";
        if (cb == "onUpdate" && gen.usesTimers) {  // "Après un délai": pending continuations
            os << "    if self.__timers then\n"
                  "        for i = #self.__timers, 1, -1 do\n"
                  "            local t = self.__timers[i]\n"
                  "            t.t = t.t - dt\n"
                  "            if t.t <= 0 then table.remove(self.__timers, i); t.f() end\n"
                  "        end\n"
                  "    end\n";
        }
        os << bodies[cb] << "end\n\n";
    }
    // Nodes never reached from an event.
    std::set<int> reached;
    std::function<void(int)> mark = [&](int id) {
        if (!reached.insert(id).second) return;
        for (const Link& l : graph.links) {
            if (l.fromNode == id) mark(l.toNode);
            if (l.toNode == id) mark(l.fromNode);  // values feeding reached nodes
        }
    };
    for (const Node& n : graph.nodes)
        if (findNodeDef(n.type)->event) mark(n.id);
    for (const Node& n : graph.nodes)
        if (!reached.count(n.id)) gen.warnings.push_back(gen.where(n) + " : relié à aucun événement (ignoré)");
    os << "return L\n";
    CompileResult r;
    r.lua = os.str();
    r.errors = std::move(gen.errors);
    r.warnings = std::move(gen.warnings);
    return r;
}

} // namespace agr::logic
