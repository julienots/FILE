#include "agr/editor/ApiReference.hpp"

#include <cstdio>
#include <map>

#include "agr/editor/SceneTemplates.hpp"
#include "agr/scene/Reflection.hpp"
#include "agr/script/ScriptEngine.hpp"

namespace agr::editor {

namespace {
const char* typeName(FieldType t) {
    switch (t) {
        case FieldType::Bool: return "bool";
        case FieldType::Int32: return "int";
        case FieldType::UInt32: return "uint";
        case FieldType::Float: return "float";
        case FieldType::Vec2: return "vec2";
        case FieldType::Vec3: return "vec3";
        case FieldType::Vec4: return "vec4";
        case FieldType::Quat: return "rotation";
        case FieldType::String: return "string";
        case FieldType::Entity: return "entity";
        case FieldType::Enum: return "enum";
        case FieldType::IntArray: return "int[]";
        case FieldType::Vec2Array: return "vec2[]";
    }
    return "?";
}
std::string cell(std::string s) {  // Markdown table cell
    for (char& c : s)
        if (c == '|' || c == '\n') c = c == '|' ? '/' : ' ';
    return s;
}
std::string num(float v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", static_cast<double>(v));
    return b;
}
}  // namespace

std::string apiReferenceMarkdown(ScriptEngine* scripts) {
    registerAllComponentTypes();
    std::string md = "# Référence de l'API (générée)\n\n"
                     "Produite par `agr_editor --api-reference docs/API.md` à partir du moteur lui-même : la liste est "
                     "toujours celle du code. Guides : [SCRIPTING.md](SCRIPTING.md), [ARCHITECTURE.md](ARCHITECTURE.md).\n\n";
    const auto all = ComponentRegistry::instance().all();
    // Index by category.
    std::map<std::string, std::vector<const ComponentInfo*>> byCategory;
    for (const ComponentInfo* c : all)
        if (c->serializable) byCategory[c->category.empty() ? std::string("Autres") : c->category].push_back(c);
    usize fields = 0;
    for (const ComponentInfo* c : all) fields += c->fields.size();
    md += "## Composants (" + std::to_string(all.size()) + ", " + std::to_string(fields) + " champs)\n\n";
    for (const auto& [cat, list] : byCategory) {
        md += "**" + cat + "** : ";
        for (usize i = 0; i < list.size(); ++i) md += (i ? ", " : "") + std::string("[") + list[i]->name + "](#" + list[i]->name + ")";
        md += "\n\n";
    }
    for (const auto& [cat, list] : byCategory)
        for (const ComponentInfo* c : list) {
            md += "### " + c->name + "\n\n";
            if (!c->description.empty()) md += c->description + "\n\n";
            md += "*" + cat + "*" + std::string(c->userAddable ? "" : " — ajouté par le moteur") + "\n\n";
            if (c->fields.empty()) {
                md += "(aucun champ)\n\n";
                continue;
            }
            md += "| Champ | Type | Détails |\n|---|---|---|\n";
            for (const FieldInfo& f : c->fields) {
                std::string type = typeName(f.type);
                if (f.flags & FieldFlag_Color) type = "color";
                if (f.flags & FieldFlag_AssetPath) type = "asset";
                if (f.flags & FieldFlag_Angle) type = "angle";
                std::string details = f.tooltip;
                if (f.hasRange()) details += (details.empty() ? "" : " ; ") + std::string("plage ") + num(f.rangeMin) + " .. " + num(f.rangeMax);
                if (!f.enumEntries.empty()) {
                    details += (details.empty() ? "" : " ; ") + std::string("valeurs : ");
                    for (usize i = 0; i < f.enumEntries.size(); ++i) details += (i ? ", " : "") + std::string("`") + f.enumEntries[i].name + "`";
                }
                if (f.flags & FieldFlag_Transient) details += " (non enregistré)";
                if (f.flags & FieldFlag_ReadOnly) details += " (lecture seule)";
                md += "| `" + f.name + "` | " + type + " | " + cell(details) + " |\n";
            }
            md += "\n";
        }
    if (scripts) {
        md += "## API Lua\n\nModules de `agr` (et leurs fonctions), puis les méthodes d'entité (`e:nom(...)`). Détails et exemples : "
              "[SCRIPTING.md](SCRIPTING.md).\n\n";
        usize count = 0;
        std::string body;
        for (const std::string& top : scripts->completions("agr")) {
            const auto children = scripts->completions("agr." + top);
            if (children.empty()) {
                body += "- `agr." + top + "`\n";
                ++count;
                continue;
            }
            body += "- **`agr." + top + "`** : ";
            for (usize i = 0; i < children.size(); ++i) body += (i ? ", " : "") + std::string("`") + children[i] + "`";
            body += "\n";
            count += children.size();
        }
        const auto methods = scripts->completions(":");
        body += "\n**Méthodes d'entité** (" + std::to_string(methods.size()) + ") : ";
        for (usize i = 0; i < methods.size(); ++i) body += (i ? ", " : "") + std::string("`") + methods[i] + "`";
        md += std::to_string(count + methods.size()) + " fonctions.\n\n" + body + "\n";
    }
    return md;
}

} // namespace agr::editor
