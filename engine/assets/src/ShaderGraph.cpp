#include "agr/assets/ShaderGraph.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

namespace agr {

using json = nlohmann::json;
using PT = ShaderPortType;

namespace {

constexpr u8 kSurface = 1, kSprite = 2, kBoth = 3;

ShaderPortDef in(std::string id, std::string label, PT t, std::string def = "") { return {std::move(id), std::move(label), t, std::move(def)}; }
ShaderPortDef out(std::string id, std::string label, PT t) { return {std::move(id), std::move(label), t, ""}; }

std::vector<ShaderNodeDef> buildDefs() {
    std::vector<ShaderNodeDef> d;
    auto add = [&](std::string type, std::string title, std::string cat, std::string tip, std::vector<ShaderPortDef> ins,
                   std::vector<ShaderPortDef> outs, u8 targets = kBoth, std::vector<ShaderPortDef> fields = {}) {
        d.push_back({std::move(type), std::move(title), std::move(cat), std::move(tip), std::move(ins), std::move(outs), std::move(fields), targets});
    };
    // ---- outputs
    add("SurfaceOutput", "Sortie surface", "Sortie", "Ce que le graphe change de la surface (entrées non reliées : valeur du matériau)",
        {in("baseColor", "Couleur", PT::Vec3), in("alpha", "Opacité", PT::Float), in("metallic", "Métal", PT::Float),
         in("roughness", "Rugosité", PT::Float), in("emissive", "Émission", PT::Vec3), in("normal", "Normale", PT::Vec3),
         in("occlusion", "Occlusion", PT::Float)},
        {}, kSurface);
    add("VertexOffset", "Déplacement des sommets", "Sortie", "Ajouté à la position monde des sommets (aussi dans la passe d'ombre)",
        {in("offset", "Décalage", PT::Vec3, "0,0,0")}, {}, kSurface);
    add("SpriteOutput", "Sortie sprite", "Sortie", "Couleur finale du pixel du sprite", {in("color", "Couleur", PT::Vec4)}, {}, kSprite);
    // ---- inputs
    add("UV", "UV", "Entrée", "Coordonnées de texture", {}, {out("uv", "UV", PT::Vec2)});
    add("Time", "Temps", "Entrée", "Secondes (TIME)", {}, {out("t", "Temps", PT::Float)});
    add("Position", "Position monde", "Entrée", "", {}, {out("p", "Position", PT::Vec3)});
    add("Normal", "Normale", "Entrée", "Normale monde", {}, {out("n", "Normale", PT::Vec3)}, kSurface);
    add("ViewDir", "Direction de vue", "Entrée", "Vers la caméra (fragment)", {}, {out("v", "Vue", PT::Vec3)}, kSurface);
    add("VertexColor", "Couleur de l'objet", "Entrée", "", {}, {out("c", "Couleur", PT::Vec4)});
    add("BaseColor", "Couleur du matériau", "Entrée", "Surface : couleur déjà calculée ; sprite : texture × teinte",
        {}, {out("rgba", "RGBA", PT::Vec4), out("rgb", "RGB", PT::Vec3), out("a", "A", PT::Float)});
    add("SpriteTexture", "Texture du sprite", "Entrée", "La texture du sprite (pour l'échantillonner ailleurs)", {}, {out("tex", "Texture", PT::Sampler)},
        kSprite);
    // ---- constants and parameters
    add("Float", "Nombre", "Constante", "", {}, {out("v", "Valeur", PT::Float)}, kBoth, {in("value", "Valeur", PT::Float, "1")});
    add("Vector", "Vecteur", "Constante", "", {}, {out("v", "XYZW", PT::Vec4), out("xyz", "XYZ", PT::Vec3), out("xy", "XY", PT::Vec2)}, kBoth,
        {in("value", "Valeur", PT::Vec4, "0,0,0,0")});
    add("Color", "Couleur", "Constante", "Couleur sRGB (convertie en linéaire)", {}, {out("rgba", "RGBA", PT::Vec4), out("rgb", "RGB", PT::Vec3)},
        kBoth, {in("value", "Couleur", PT::Vec4, "1,1,1,1")});
    add("ParamFloat", "Paramètre nombre", "Paramètre", "Réglable dans chaque matériau", {}, {out("v", "Valeur", PT::Float)}, kBoth,
        {in("name", "Nom", PT::Float, "amount"), in("value", "Défaut", PT::Float, "1"), in("min", "Min", PT::Float, "0"), in("max", "Max", PT::Float, "1")});
    add("ParamColor", "Paramètre couleur", "Paramètre", "Réglable dans chaque matériau", {},
        {out("rgba", "RGBA", PT::Vec4), out("rgb", "RGB", PT::Vec3), out("a", "A", PT::Float)}, kBoth,
        {in("name", "Nom", PT::Float, "tint"), in("value", "Défaut", PT::Vec4, "1,1,1,1")});
    add("ParamTexture", "Paramètre texture", "Paramètre", "Image réglable dans chaque matériau", {}, {out("tex", "Texture", PT::Sampler)}, kSurface,
        {in("name", "Nom", PT::Float, "mask"), in("path", "Image", PT::Float, ""), in("srgb", "Couleur (sRGB)", PT::Float, "true")});
    // ---- textures
    add("SampleTexture", "Lire une texture", "Texture", "Couleur d'une texture aux coordonnées données",
        {in("tex", "Texture", PT::Sampler), in("uv", "UV", PT::Vec2, "@uv")},
        {out("rgba", "RGBA", PT::Vec4), out("rgb", "RGB", PT::Vec3), out("r", "R", PT::Float), out("a", "A", PT::Float)});
    add("Noise", "Bruit", "Texture", "Bruit de valeur lissé (0..1), sans texture", {in("uv", "UV", PT::Vec2, "@uv"), in("scale", "Échelle", PT::Float, "8")},
        {out("v", "Bruit", PT::Float)});
    add("Panner", "Défilement", "UV", "UV + temps × vitesse (eau, nuages, tapis roulant)", {in("uv", "UV", PT::Vec2, "@uv"), in("speed", "Vitesse", PT::Vec2, "0.1,0")},
        {out("uv", "UV", PT::Vec2)});
    add("Tiling", "Répétition", "UV", "UV × répétitions + décalage", {in("uv", "UV", PT::Vec2, "@uv"), in("tiling", "Répétitions", PT::Vec2, "2,2"),
                                                                     in("offset", "Décalage", PT::Vec2, "0,0")},
        {out("uv", "UV", PT::Vec2)});
    // ---- math (Dynamic = the widest linked input)
    const std::vector<std::pair<std::string, std::string>> binary = {{"Add", "Ajouter"}, {"Subtract", "Soustraire"}, {"Multiply", "Multiplier"},
                                                                     {"Divide", "Diviser"}, {"Min", "Minimum"}, {"Max", "Maximum"}, {"Power", "Puissance"},
                                                                     {"Step", "Seuil"}};
    for (const auto& [t, title] : binary)
        add(t, title, "Math", t == "Step" ? "0 sous le seuil a, 1 au-dessus" : "", {in("a", "A", PT::Dynamic, t == "Multiply" || t == "Divide" || t == "Power" ? "1" : "0"),
                                                                                     in("b", "B", PT::Dynamic, t == "Multiply" || t == "Divide" ? "1" : t == "Power" ? "2" : "0")},
            {out("out", "Résultat", PT::Dynamic)});
    const std::vector<std::pair<std::string, std::string>> unary = {{"OneMinus", "1 - x"}, {"Abs", "Valeur absolue"}, {"Fract", "Partie décimale"},
                                                                    {"Floor", "Partie entière"}, {"Sin", "Sinus"}, {"Cos", "Cosinus"},
                                                                    {"Saturate", "Borner 0..1"}, {"Negate", "Opposé"}, {"Sqrt", "Racine"}};
    for (const auto& [t, title] : unary) add(t, title, "Math", "", {in("x", "X", PT::Dynamic, "0")}, {out("out", "Résultat", PT::Dynamic)});
    add("Mix", "Mélange", "Math", "a → b selon t (0..1)", {in("a", "A", PT::Dynamic, "0"), in("b", "B", PT::Dynamic, "1"), in("t", "T", PT::Float, "0.5")},
        {out("out", "Résultat", PT::Dynamic)});
    add("Clamp", "Borner", "Math", "", {in("x", "X", PT::Dynamic, "0"), in("min", "Min", PT::Dynamic, "0"), in("max", "Max", PT::Dynamic, "1")},
        {out("out", "Résultat", PT::Dynamic)});
    add("Smoothstep", "Transition douce", "Math", "0 avant a, 1 après b, courbe douce entre", {in("a", "A", PT::Dynamic, "0"), in("b", "B", PT::Dynamic, "1"),
                                                                                               in("x", "X", PT::Dynamic, "0.5")},
        {out("out", "Résultat", PT::Dynamic)});
    add("Remap", "Changer d'échelle", "Math", "De [inMin, inMax] vers [outMin, outMax]",
        {in("x", "X", PT::Dynamic, "0.5"), in("inMin", "De min", PT::Dynamic, "0"), in("inMax", "De max", PT::Dynamic, "1"),
         in("outMin", "Vers min", PT::Dynamic, "-1"), in("outMax", "Vers max", PT::Dynamic, "1")},
        {out("out", "Résultat", PT::Dynamic)});
    add("Posterize", "Paliers", "Math", "Arrondit à un nombre de niveaux (style cel)", {in("x", "X", PT::Dynamic, "0"), in("steps", "Niveaux", PT::Float, "4")},
        {out("out", "Résultat", PT::Dynamic)});
    // ---- vectors
    add("Dot", "Produit scalaire", "Vecteur", "", {in("a", "A", PT::Vec3, "0,0,1"), in("b", "B", PT::Vec3, "0,0,1")}, {out("out", "Résultat", PT::Float)});
    add("Length", "Longueur", "Vecteur", "", {in("v", "V", PT::Vec3, "0,0,0")}, {out("out", "Longueur", PT::Float)});
    add("Distance", "Distance", "Vecteur", "", {in("a", "A", PT::Vec3, "0,0,0"), in("b", "B", PT::Vec3, "0,0,0")}, {out("out", "Distance", PT::Float)});
    add("Normalize", "Normaliser", "Vecteur", "", {in("v", "V", PT::Vec3, "0,1,0")}, {out("out", "Résultat", PT::Vec3)});
    add("Split", "Séparer", "Vecteur", "", {in("v", "V", PT::Vec4, "0,0,0,0")},
        {out("x", "X", PT::Float), out("y", "Y", PT::Float), out("z", "Z", PT::Float), out("w", "W", PT::Float)});
    add("Combine", "Assembler", "Vecteur", "",
        {in("x", "X", PT::Float, "0"), in("y", "Y", PT::Float, "0"), in("z", "Z", PT::Float, "0"), in("w", "W", PT::Float, "1")},
        {out("v", "XYZW", PT::Vec4), out("xyz", "XYZ", PT::Vec3), out("xy", "XY", PT::Vec2)});
    // ---- effects
    add("Fresnel", "Fresnel", "Effet", "Bords vus de profil (halo, bouclier, eau)", {in("power", "Puissance", PT::Float, "3")}, {out("out", "Fresnel", PT::Float)},
        kSurface);
    return d;
}

std::string glslType(PT t) {
    switch (t) {
        case PT::Float: return "float";
        case PT::Vec2: return "vec2";
        case PT::Vec3: return "vec3";
        case PT::Vec4: return "vec4";
        default: return "float";
    }
}

/// Converts an expression between value types (broadcast, truncate, or pad with 0 / 1).
std::string convert(const std::string& e, PT from, PT to) {
    if (from == to || to == PT::Dynamic || to == PT::Sampler) return e;
    const int f = static_cast<int>(from), t = static_cast<int>(to);
    if (f == 1) return glslType(to) + "(" + e + ")";
    if (t == 1) return "(" + e + ").x";
    if (t < f) return "(" + e + ")." + std::string("xyzw").substr(0, static_cast<usize>(t));
    if (f == 2 && t == 3) return "vec3(" + e + ", 0.0)";
    if (f == 2 && t == 4) return "vec4(" + e + ", 0.0, 1.0)";
    return "vec4(" + e + ", 1.0)";  // vec3 -> vec4
}

std::string number(float v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", static_cast<double>(v));
    std::string s = buf;
    if (s.find_first_of(".eEn") == std::string::npos) s += ".0";
    return s;
}

std::vector<float> numbers(const std::string& text) {
    std::vector<float> out;
    std::stringstream ss(text);
    std::string part;
    while (std::getline(ss, part, ',')) out.push_back(static_cast<float>(std::atof(part.c_str())));
    return out;
}

std::string literal(const std::string& text, PT t) {
    std::vector<float> v = numbers(text);
    if (v.empty()) v.push_back(0.0f);
    const int n = t == PT::Dynamic ? 1 : static_cast<int>(t);
    if (n == 1) return number(v[0]);
    std::string s = glslType(t) + "(";
    for (int i = 0; i < n; ++i) s += (i ? ", " : "") + number(i < static_cast<int>(v.size()) ? v[static_cast<usize>(i)] : (i == 3 ? 1.0f : v.back()));
    return s + ")";
}

/// sRGB -> linear, as the editor's colors.
float toLinear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }

bool validName(const std::string& n) {
    if (n.empty() || !(std::isalpha(static_cast<unsigned char>(n[0])) || n[0] == '_')) return false;
    for (char c : n)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    static const std::set<std::string> reserved = {"texture", "color", "surface", "vertex", "sprite", "TIME", "TEXTURE", "float", "vec2", "vec3",
                                                   "vec4", "uniform", "in", "out", "inout", "s", "v", "main"};
    return !reserved.count(n) && n.rfind("u_", 0) != 0 && n.rfind("gl_", 0) != 0 && n.rfind("agr", 0) != 0 && n.rfind("n_", 0) != 0;
}

enum class Stage { Fragment, Vertex };

struct Compiler {
    const ShaderGraph& g;
    std::vector<std::string> errors;
    std::vector<std::string> params;
    std::set<std::string> paramNames;
    bool needNoise = false;
    Stage stage = Stage::Fragment;
    std::string body;
    std::map<int, std::map<std::string, std::pair<std::string, PT>>> done;  // node -> port -> (expr, type)
    std::set<int> visiting;
    std::set<int> paramsEmitted;

    std::string where(const ShaderGraph::Node& n, const ShaderNodeDef& d) { return d.title + " (n° " + std::to_string(n.id) + ")"; }
    bool sprite() const { return g.target == ShaderGraph::Target::Sprite; }
    std::string stageVar() const { return stage == Stage::Vertex ? "v" : "s"; }
    std::string value(const ShaderGraph::Node& n, const std::string& id, const std::string& def) {
        auto it = n.values.find(id);
        return it == n.values.end() || it->second.empty() ? def : it->second;
    }

    /// Expression of input `p` of node `n`, converted to `want`.
    std::string input(const ShaderGraph::Node& n, const ShaderPortDef& p, PT want) {
        if (const ShaderGraph::Link* l = g.linkTo(n.id, p.id)) {
            const ShaderGraph::Node* src = g.node(l->fromNode);
            if (!src) return literal("0", want);
            auto outs = emit(*src);
            auto it = outs.find(l->fromPort);
            if (it == outs.end()) return literal("0", want);
            if ((want == PT::Sampler) != (it->second.second == PT::Sampler)) {
                errors.push_back(where(n, *findShaderNodeDef(n.type)) + ": « " + p.label + " » ne peut pas recevoir ce type");
                return literal("0", want);
            }
            return convert(it->second.first, it->second.second, want);
        }
        const std::string def = value(n, p.id, p.defaultValue);
        if (def == "@uv") return convert(stageVar() + ".uv", PT::Vec2, want);
        if (want == PT::Sampler) return "";
        return literal(def, want);
    }
    PT linkedType(const ShaderGraph::Node& n, const ShaderPortDef& p) {
        const ShaderGraph::Link* l = g.linkTo(n.id, p.id);
        if (!l) return PT::Float;
        const ShaderGraph::Node* src = g.node(l->fromNode);
        if (!src) return PT::Float;
        auto outs = emit(*src);
        auto it = outs.find(l->fromPort);
        return it == outs.end() ? PT::Float : it->second.second;
    }

    void param(const std::string& line, const ShaderGraph::Node& n) {
        if (paramsEmitted.insert(n.id).second) params.push_back(line);
    }

    std::map<std::string, std::pair<std::string, PT>> emit(const ShaderGraph::Node& n) {
        if (auto it = done.find(n.id); it != done.end()) return it->second;
        const ShaderNodeDef* d = findShaderNodeDef(n.type);
        std::map<std::string, std::pair<std::string, PT>> out;
        if (!d) {
            errors.push_back("nœud inconnu « " + n.type + " » (n° " + std::to_string(n.id) + ")");
            return out;
        }
        if (!(d->targets & (sprite() ? kSprite : kSurface))) errors.push_back(where(n, *d) + ": pas disponible pour ce type de shader");
        if (visiting.count(n.id)) {
            errors.push_back(where(n, *d) + ": boucle dans le graphe");
            return out;
        }
        visiting.insert(n.id);
        const std::string var = "n_" + std::to_string(n.id);
        const std::string sv = stageVar();
        const bool vert = stage == Stage::Vertex;
        auto decl = [&](const std::string& port, PT t, const std::string& expr) {
            const std::string name = var + "_" + port;
            body += "    " + glslType(t) + " " + name + " = " + expr + ";\n";
            out[port] = {name, t};
        };
        auto fragmentOnly = [&] {
            if (vert) errors.push_back(where(n, *d) + ": utilisable seulement dans la surface (pas dans le déplacement des sommets)");
        };
        // Dynamic type: the widest linked Dynamic input (Float when none).
        PT T = PT::Float;
        for (const ShaderPortDef& p : d->inputs)
            if (p.type == PT::Dynamic) {
                const PT lt = linkedType(n, p);
                if (lt != PT::Sampler && static_cast<int>(lt) > static_cast<int>(T)) T = lt;
            }
        auto I = [&](const std::string& id) -> std::string {
            for (const ShaderPortDef& p : d->inputs)
                if (p.id == id) return input(n, p, p.type == PT::Dynamic ? T : p.type);
            return "0.0";
        };
        const std::string& t = n.type;
        if (t == "UV") decl("uv", PT::Vec2, sv + ".uv");
        else if (t == "Time") decl("t", PT::Float, "TIME");
        else if (t == "Position") decl("p", PT::Vec3, vert ? "v.position" : sprite() ? "vec3(s.worldPos, 0.0)" : "s.worldPos");
        else if (t == "Normal") decl("n", PT::Vec3, sv + ".normal");
        else if (t == "ViewDir") {
            fragmentOnly();
            decl("v", PT::Vec3, vert ? "vec3(0.0, 0.0, 1.0)" : "s.viewDir");
        } else if (t == "VertexColor") decl("c", PT::Vec4, sprite() ? "s.tint" : sv + ".color");
        else if (t == "BaseColor") {
            fragmentOnly();
            decl("rgba", PT::Vec4, sprite() ? "s.color" : vert ? "vec4(1.0)" : "vec4(s.baseColor, s.alpha)");
            out["rgb"] = {var + "_rgba.rgb", PT::Vec3};
            out["a"] = {var + "_rgba.a", PT::Float};
        } else if (t == "SpriteTexture") out["tex"] = {"TEXTURE", PT::Sampler};
        else if (t == "Float") decl("v", PT::Float, literal(value(n, "value", "1"), PT::Float));
        else if (t == "Vector") {
            decl("v", PT::Vec4, literal(value(n, "value", "0,0,0,0"), PT::Vec4));
            out["xyz"] = {var + "_v.xyz", PT::Vec3};
            out["xy"] = {var + "_v.xy", PT::Vec2};
        } else if (t == "Color") {
            std::vector<float> c = numbers(value(n, "value", "1,1,1,1"));
            c.resize(4, 1.0f);
            decl("rgba", PT::Vec4, "vec4(" + number(toLinear(c[0])) + ", " + number(toLinear(c[1])) + ", " + number(toLinear(c[2])) + ", " + number(c[3]) + ")");
            out["rgb"] = {var + "_rgba.rgb", PT::Vec3};
        } else if (t == "ParamFloat" || t == "ParamColor" || t == "ParamTexture") {
            const std::string name = value(n, "name", "");
            if (!validName(name)) errors.push_back(where(n, *d) + ": nom de paramètre invalide « " + name + " » (lettres, chiffres, _)");
            else if (paramNames.count(name) && !paramsEmitted.count(n.id)) errors.push_back(where(n, *d) + ": le nom « " + name + " » est déjà pris");
            paramNames.insert(name);
            if (t == "ParamFloat") {
                param("param float " + name + " = " + literal(value(n, "value", "1"), PT::Float) + " range(" + literal(value(n, "min", "0"), PT::Float) + ", " +
                          literal(value(n, "max", "1"), PT::Float) + ");",
                      n);
                out["v"] = {name, PT::Float};
            } else if (t == "ParamColor") {
                std::vector<float> c = numbers(value(n, "value", "1,1,1,1"));
                c.resize(4, 1.0f);
                param("param color " + name + " = (" + number(c[0]) + ", " + number(c[1]) + ", " + number(c[2]) + ", " + number(c[3]) + ");", n);
                out["rgba"] = {name, PT::Vec4};
                out["rgb"] = {name + ".rgb", PT::Vec3};
                out["a"] = {name + ".a", PT::Float};
            } else {
                const std::string path = value(n, "path", "");
                std::string line = "param texture " + name;
                if (!path.empty()) line += " = \"" + path + "\"";
                if (value(n, "srgb", "true") == "true") line += " srgb";
                param(line + ";", n);
                out["tex"] = {name, PT::Sampler};
            }
        } else if (t == "SampleTexture") {
            fragmentOnly();
            std::string tex = I("tex");
            if (tex.empty()) {
                if (sprite()) tex = "TEXTURE";
                else errors.push_back(where(n, *d) + ": reliez une texture (Paramètre texture)");
            }
            decl("rgba", PT::Vec4, tex.empty() || vert ? "vec4(1.0)" : "texture(" + tex + ", " + I("uv") + ")");
            out["rgb"] = {var + "_rgba.rgb", PT::Vec3};
            out["r"] = {var + "_rgba.r", PT::Float};
            out["a"] = {var + "_rgba.a", PT::Float};
        } else if (t == "Noise") {
            needNoise = true;
            decl("v", PT::Float, "sgNoise(" + I("uv") + " * " + I("scale") + ")");
        } else if (t == "Panner") decl("uv", PT::Vec2, I("uv") + " + TIME * " + I("speed"));
        else if (t == "Tiling") decl("uv", PT::Vec2, I("uv") + " * " + I("tiling") + " + " + I("offset"));
        else if (t == "Add") decl("out", T, I("a") + " + " + I("b"));
        else if (t == "Subtract") decl("out", T, I("a") + " - " + I("b"));
        else if (t == "Multiply") decl("out", T, I("a") + " * " + I("b"));
        else if (t == "Divide") decl("out", T, I("a") + " / max(" + I("b") + ", " + convert("1e-5", PT::Float, T) + ")");
        else if (t == "Min") decl("out", T, "min(" + I("a") + ", " + I("b") + ")");
        else if (t == "Max") decl("out", T, "max(" + I("a") + ", " + I("b") + ")");
        else if (t == "Power") decl("out", T, "pow(max(" + I("a") + ", " + convert("0.0", PT::Float, T) + "), " + I("b") + ")");
        else if (t == "Step") decl("out", T, "step(" + I("a") + ", " + I("b") + ")");
        else if (t == "OneMinus") decl("out", T, convert("1.0", PT::Float, T) + " - " + I("x"));
        else if (t == "Abs") decl("out", T, "abs(" + I("x") + ")");
        else if (t == "Fract") decl("out", T, "fract(" + I("x") + ")");
        else if (t == "Floor") decl("out", T, "floor(" + I("x") + ")");
        else if (t == "Sin") decl("out", T, "sin(" + I("x") + ")");
        else if (t == "Cos") decl("out", T, "cos(" + I("x") + ")");
        else if (t == "Saturate") decl("out", T, "clamp(" + I("x") + ", 0.0, 1.0)");
        else if (t == "Negate") decl("out", T, "-(" + I("x") + ")");
        else if (t == "Sqrt") decl("out", T, "sqrt(max(" + I("x") + ", " + convert("0.0", PT::Float, T) + "))");
        else if (t == "Mix") decl("out", T, "mix(" + I("a") + ", " + I("b") + ", " + I("t") + ")");
        else if (t == "Clamp") decl("out", T, "clamp(" + I("x") + ", " + I("min") + ", " + I("max") + ")");
        else if (t == "Smoothstep") decl("out", T, "smoothstep(" + I("a") + ", " + I("b") + ", " + I("x") + ")");
        else if (t == "Remap")
            decl("out", T, I("outMin") + " + (" + I("x") + " - " + I("inMin") + ") / max(" + I("inMax") + " - " + I("inMin") + ", " +
                               convert("1e-5", PT::Float, T) + ") * (" + I("outMax") + " - " + I("outMin") + ")");
        else if (t == "Posterize") decl("out", T, "floor(" + I("x") + " * max(" + I("steps") + ", 1.0)) / max(" + I("steps") + ", 1.0)");
        else if (t == "Dot") decl("out", PT::Float, "dot(" + I("a") + ", " + I("b") + ")");
        else if (t == "Length") decl("out", PT::Float, "length(" + I("v") + ")");
        else if (t == "Distance") decl("out", PT::Float, "distance(" + I("a") + ", " + I("b") + ")");
        else if (t == "Normalize") decl("out", PT::Vec3, "normalize(" + I("v") + ")");
        else if (t == "Split") {
            decl("v", PT::Vec4, I("v"));
            for (const char* c : {"x", "y", "z", "w"}) out[c] = {var + "_v." + c, PT::Float};
        } else if (t == "Combine") {
            decl("v", PT::Vec4, "vec4(" + I("x") + ", " + I("y") + ", " + I("z") + ", " + I("w") + ")");
            out["xyz"] = {var + "_v.xyz", PT::Vec3};
            out["xy"] = {var + "_v.xy", PT::Vec2};
        } else if (t == "Fresnel") {
            fragmentOnly();
            decl("out", PT::Float, vert ? "0.0" : "pow(1.0 - clamp(dot(normalize(s.normal), normalize(s.viewDir)), 0.0, 1.0), " + I("power") + ")");
        }
        visiting.erase(n.id);
        done[n.id] = out;
        return out;
    }

    /// Assignments of an output node's linked inputs.
    void assign(const ShaderGraph::Node& n, const std::vector<std::pair<std::string, std::string>>& targets) {
        const ShaderNodeDef* d = findShaderNodeDef(n.type);
        for (const auto& [port, lhs] : targets) {
            if (!g.linkTo(n.id, port)) continue;
            for (const ShaderPortDef& p : d->inputs)
                if (p.id == port) body += "    " + lhs + " = " + input(n, p, p.type) + ";\n";
        }
    }
};

}  // namespace

const std::vector<ShaderNodeDef>& shaderNodeDefs() {
    static const std::vector<ShaderNodeDef> defs = buildDefs();
    return defs;
}

const ShaderNodeDef* findShaderNodeDef(const std::string& type) {
    for (const ShaderNodeDef& d : shaderNodeDefs())
        if (d.type == type) return &d;
    return nullptr;
}

// ------------------------------------------------------------------ graph editing
ShaderGraph ShaderGraph::create(Target target) {
    ShaderGraph g;
    g.target = target;
    g.addNode(target == Target::Sprite ? "SpriteOutput" : "SurfaceOutput", 420.0f, 80.0f);
    return g;
}

int ShaderGraph::addNode(const std::string& type, float x, float y) {
    Node n;
    n.id = nextId++;
    n.type = type;
    n.x = x, n.y = y;
    if (const ShaderNodeDef* d = findShaderNodeDef(type))
        for (const ShaderPortDef& f : d->fields) n.values[f.id] = f.defaultValue;
    nodes.push_back(std::move(n));
    return nodes.back().id;
}

void ShaderGraph::removeNode(int id) {
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const Node& n) { return n.id == id; }), nodes.end());
    links.erase(std::remove_if(links.begin(), links.end(), [&](const Link& l) { return l.fromNode == id || l.toNode == id; }), links.end());
}

ShaderGraph::Node* ShaderGraph::node(int id) {
    for (Node& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}
const ShaderGraph::Node* ShaderGraph::node(int id) const { return const_cast<ShaderGraph*>(this)->node(id); }

const ShaderGraph::Link* ShaderGraph::linkTo(int nodeId, const std::string& port) const {
    for (const Link& l : links)
        if (l.toNode == nodeId && l.toPort == port) return &l;
    return nullptr;
}

void ShaderGraph::disconnect(int toNode, const std::string& toPort) {
    links.erase(std::remove_if(links.begin(), links.end(), [&](const Link& l) { return l.toNode == toNode && l.toPort == toPort; }), links.end());
}

Status ShaderGraph::connect(int fromNode, const std::string& fromPort, int toNode, const std::string& toPort) {
    const Node* a = node(fromNode);
    const Node* b = node(toNode);
    if (!a || !b || fromNode == toNode) return makeError(Error::Code::InvalidArgument, "nœud introuvable");
    const ShaderNodeDef* da = findShaderNodeDef(a->type);
    const ShaderNodeDef* db = findShaderNodeDef(b->type);
    if (!da || !db) return makeError(Error::Code::InvalidArgument, "type de nœud inconnu");
    const auto o = std::find_if(da->outputs.begin(), da->outputs.end(), [&](const ShaderPortDef& p) { return p.id == fromPort; });
    const auto i = std::find_if(db->inputs.begin(), db->inputs.end(), [&](const ShaderPortDef& p) { return p.id == toPort; });
    if (o == da->outputs.end() || i == db->inputs.end()) return makeError(Error::Code::InvalidArgument, "port introuvable");
    if ((o->type == PT::Sampler) != (i->type == PT::Sampler)) return makeError(Error::Code::InvalidArgument, "une texture ne se relie qu'à une entrée texture");
    // Cycle: is `fromNode` reachable from `toNode` going downstream?
    std::vector<int> stack{toNode};
    std::set<int> seen;
    while (!stack.empty()) {
        const int cur = stack.back();
        stack.pop_back();
        if (cur == fromNode) return makeError(Error::Code::InvalidArgument, "cette liaison ferait une boucle");
        if (!seen.insert(cur).second) continue;
        for (const Link& l : links)
            if (l.fromNode == cur) stack.push_back(l.toNode);
    }
    disconnect(toNode, toPort);
    links.push_back({fromNode, fromPort, toNode, toPort});
    return {};
}

std::string ShaderGraph::toJson() const {
    json j;
    j["format"] = "agr.shadergraph";
    j["version"] = 1;
    j["target"] = target == Target::Sprite ? "sprite" : "surface";
    json ns = json::array();
    for (const Node& n : nodes) ns.push_back({{"id", n.id}, {"type", n.type}, {"x", n.x}, {"y", n.y}, {"values", n.values}});
    j["nodes"] = ns;
    json ls = json::array();
    for (const Link& l : links) ls.push_back({{"from", l.fromNode}, {"fromPort", l.fromPort}, {"to", l.toNode}, {"toPort", l.toPort}});
    j["links"] = ls;
    return j.dump(2);
}

Result<ShaderGraph> ShaderGraph::fromJson(std::string_view text) {
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object() || j.value("format", "") != "agr.shadergraph")
        return makeError(Error::Code::ParseError, "not a shader graph (format: agr.shadergraph)");
    if (j.value("version", 1) > 1) return makeError(Error::Code::VersionMismatch, "shader graph made by a newer version of AGR");
    ShaderGraph g;
    try {
        g.target = j.value("target", "surface") == "sprite" ? Target::Sprite : Target::Surface;
        for (const json& n : j.value("nodes", json::array())) {
            Node node;
            node.id = n.value("id", 0);
            node.type = n.value("type", "");
            node.x = n.value("x", 0.0f);
            node.y = n.value("y", 0.0f);
            node.values = n.value("values", std::map<std::string, std::string>{});
            g.nextId = std::max(g.nextId, node.id + 1);
            g.nodes.push_back(std::move(node));
        }
        for (const json& l : j.value("links", json::array()))
            g.links.push_back({l.value("from", 0), l.value("fromPort", ""), l.value("to", 0), l.value("toPort", "")});
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("shader graph: ") + e.what());
    }
    return g;
}

// ------------------------------------------------------------------ compile
ShaderGraphCompileResult compileShaderGraph(const ShaderGraph& graph) {
    ShaderGraphCompileResult r;
    Compiler c{graph};
    const bool sprite = graph.target == ShaderGraph::Target::Sprite;
    const ShaderGraph::Node* surfaceOut = nullptr;
    const ShaderGraph::Node* vertexOut = nullptr;
    const ShaderGraph::Node* spriteOut = nullptr;
    for (const ShaderGraph::Node& n : graph.nodes) {
        if (n.type == "SurfaceOutput") (surfaceOut ? r.errors.push_back("deux sorties surface") : void()), surfaceOut = &n;
        if (n.type == "VertexOffset") (vertexOut ? r.errors.push_back("deux déplacements de sommets") : void()), vertexOut = &n;
        if (n.type == "SpriteOutput") (spriteOut ? r.errors.push_back("deux sorties sprite") : void()), spriteOut = &n;
    }
    std::string functions;
    if (sprite) {
        if (!spriteOut) r.errors.push_back("il manque la sortie sprite");
        else {
            c.stage = Stage::Fragment;
            c.assign(*spriteOut, {{"color", "s.color"}});
            functions += "void sprite(inout Sprite s) {\n" + c.body + "}\n";
        }
    } else {
        if (!surfaceOut && !vertexOut) r.errors.push_back("il manque la sortie surface");
        if (vertexOut && graph.linkTo(vertexOut->id, "offset")) {
            c.stage = Stage::Vertex;
            c.body.clear();
            const ShaderPortDef& offset = findShaderNodeDef("VertexOffset")->inputs[0];
            const std::string e = c.input(*vertexOut, offset, PT::Vec3);  // emits the nodes it needs first
            c.body += "    v.position += " + e + ";\n";
            functions += "void vertex(inout Vertex v) {\n" + c.body + "}\n";
            c.done.clear();  // fragment expressions are separate
        }
        if (surfaceOut) {
            c.stage = Stage::Fragment;
            c.body.clear();
            c.assign(*surfaceOut, {{"baseColor", "s.baseColor"},
                                   {"alpha", "s.alpha"},
                                   {"metallic", "s.metallic"},
                                   {"roughness", "s.roughness"},
                                   {"emissive", "s.emissive"},
                                   {"normal", "s.normal"},
                                   {"occlusion", "s.occlusion"}});
            functions += "void surface(inout Surface s) {\n" + c.body + "}\n";
        }
    }
    r.errors.insert(r.errors.end(), c.errors.begin(), c.errors.end());
    if (!r.errors.empty()) return r;
    std::string src = "// Generated from a shader graph (.agrshadergraph): edit the graph, not this file.\n";
    for (const std::string& p : c.params) src += p + "\n";
    if (c.needNoise)
        src += "\nfloat sgHash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }\n"
               "float sgNoise(vec2 p) {\n"
               "    vec2 i = floor(p), f = fract(p);\n"
               "    vec2 u = f * f * (3.0 - 2.0 * f);\n"
               "    return mix(mix(sgHash(i), sgHash(i + vec2(1.0, 0.0)), u.x),\n"
               "               mix(sgHash(i + vec2(0.0, 1.0)), sgHash(i + vec2(1.0, 1.0)), u.x), u.y);\n"
               "}\n";
    src += "\n" + functions;
    r.source = std::move(src);
    return r;
}

std::string shaderUriForGraph(const std::string& graphUri) {
    const std::string ext = ShaderGraph::kExtension;
    if (graphUri.size() > ext.size() && graphUri.compare(graphUri.size() - ext.size(), ext.size(), ext) == 0)
        return graphUri.substr(0, graphUri.size() - ext.size()) + ".agrshader";
    return graphUri + ".agrshader";
}

} // namespace agr
