#include "agr/ui/UITheme.hpp"

#include <nlohmann/json.hpp>

namespace agr {

using json = nlohmann::json;

namespace {

struct Slot {
    const char* name;
    math::Vec4 UITheme::*member;
};
constexpr Slot kSlots[] = {{"text", &UITheme::text},     {"panel", &UITheme::panel},   {"button", &UITheme::button},
                           {"buttonHover", &UITheme::buttonHover}, {"buttonPressed", &UITheme::buttonPressed},
                           {"buttonDisabled", &UITheme::buttonDisabled}, {"accent", &UITheme::accent}, {"track", &UITheme::track},
                           {"handle", &UITheme::handle}, {"field", &UITheme::field}};

} // namespace

std::string UITheme::toJson() const {
    json j;
    j["version"] = 1;
    j["font"] = font;
    for (const Slot& s : kSlots) {
        const math::Vec4& c = this->*s.member;
        j[s.name] = {c.x, c.y, c.z, c.w};
    }
    return j.dump(2);
}

Result<UITheme> UITheme::fromJson(std::string_view text) {
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return makeError(Error::Code::ParseError, "theme: invalid JSON");
    if (j.value("version", 1) > 1) return makeError(Error::Code::VersionMismatch, "theme: made by a newer version of AGR");
    UITheme t;
    try {
        t.font = j.value("font", "");
        for (const Slot& s : kSlots) {
            const auto it = j.find(s.name);
            if (it == j.end()) continue;
            if (!it->is_array() || (it->size() != 3 && it->size() != 4)) return makeError(Error::Code::ParseError, std::string("theme: '") + s.name + "' must be [r, g, b, a]");
            (t.*s.member) = {(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>(), it->size() == 4 ? (*it)[3].get<float>() : 1.0f};
        }
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("theme: ") + e.what());
    }
    return t;
}

} // namespace agr
