#include "agr/runtime/GameManifest.hpp"

#include <cctype>

#include <nlohmann/json.hpp>

namespace agr {

using nlohmann::json;

namespace {
constexpr const char* kOrientations[] = {"landscape", "portrait", "sensor"};
}

const char* enumName(ScreenOrientation o) { return kOrientations[std::min<usize>(static_cast<usize>(o), 2)]; }

std::string GameManifest::toJson() const {
    json j{{"version", 1},
           {"name", name},
           {"organization", organization},
           {"gameVersion", version},
           {"versionCode", versionCode},
           {"packageId", packageId},
           {"startupScene", startupScene},
           {"graphicsProfile", graphicsProfile},
           {"orientation", enumName(orientation)},
           {"targetFps", targetFps},
           {"backQuits", backQuits},
           {"vibration", vibration},
           {"keepScreenOn", keepScreenOn},
           {"engineVersion", engineVersion}};
    return j.dump(2);
}

Result<GameManifest> GameManifest::fromJson(std::string_view text) {
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return makeError(Error::Code::ParseError, "game.json: invalid JSON");
    if (j.value("version", 1) > 1) return makeError(Error::Code::VersionMismatch, "game.json: made by a newer version of AGR");
    GameManifest m;
    try {
        m.name = j.value("name", m.name);
        m.organization = j.value("organization", m.organization);
        m.version = j.value("gameVersion", m.version);
        m.versionCode = j.value("versionCode", m.versionCode);
        m.packageId = j.value("packageId", m.packageId);
        m.startupScene = j.value("startupScene", m.startupScene);
        m.graphicsProfile = j.value("graphicsProfile", m.graphicsProfile);
        const std::string o = j.value("orientation", "landscape");
        for (u8 i = 0; i < 3; ++i)
            if (o == kOrientations[i]) m.orientation = static_cast<ScreenOrientation>(i);
        m.targetFps = j.value("targetFps", m.targetFps);
        m.backQuits = j.value("backQuits", m.backQuits);
        m.vibration = j.value("vibration", m.vibration);
        m.keepScreenOn = j.value("keepScreenOn", m.keepScreenOn);
        m.engineVersion = j.value("engineVersion", "");
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("game.json: ") + e.what());
    }
    if (m.startupScene.empty()) return makeError(Error::Code::ParseError, "game.json: no startup scene");
    return m;
}

std::string GameManifest::safeName(std::string_view name) {
    std::string out;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) out += c;
        else if (c == ' ' || c == '-' || c == '_') out += '_';
    }
    return out.empty() ? std::string("Game") : out;
}

bool GameManifest::validPackageId(std::string_view id) {
    int segments = 0;
    usize i = 0;
    while (i <= id.size()) {
        const usize dot = id.find('.', i);
        const std::string_view seg = id.substr(i, dot == std::string_view::npos ? std::string_view::npos : dot - i);
        if (seg.empty() || !std::isalpha(static_cast<unsigned char>(seg[0]))) return false;
        for (char c : seg)
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
        ++segments;
        if (dot == std::string_view::npos) break;
        i = dot + 1;
    }
    return segments >= 2;
}

} // namespace agr
