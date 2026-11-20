#include "agr/render/ParticleCurves.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "agr/core/Log.hpp"

namespace agr {

using json = nlohmann::json;

namespace {

template <typename K, typename V, typename Lerp>
V sampleKeys(const std::vector<K>& keys, float t, V fallback, V K::*value, Lerp lerp) {
    if (keys.empty()) return fallback;
    if (t <= keys.front().t) return keys.front().*value;
    if (t >= keys.back().t) return keys.back().*value;
    for (usize i = 1; i < keys.size(); ++i)
        if (t <= keys[i].t) {
            const float span = std::max(keys[i].t - keys[i - 1].t, 1e-6f);
            return lerp(keys[i - 1].*value, keys[i].*value, (t - keys[i - 1].t) / span);
        }
    return keys.back().*value;
}

math::Color lerpColor(math::Color a, math::Color b, float f) {
    return {a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f, a.w + (b.w - a.w) * f};
}

} // namespace

float ParticleCurves::sizeAt(float t) const {
    return sampleKeys(size, t, 1.0f, &Key::value, [](float a, float b, float f) { return a + (b - a) * f; });
}

math::Color ParticleCurves::colorAt(float t) const { return sampleKeys(gradient, t, math::Color{1, 1, 1, 1}, &ColorKey::color, lerpColor); }

void ParticleCurves::sort() {
    std::stable_sort(size.begin(), size.end(), [](const Key& a, const Key& b) { return a.t < b.t; });
    std::stable_sort(gradient.begin(), gradient.end(), [](const ColorKey& a, const ColorKey& b) { return a.t < b.t; });
}

std::string ParticleCurves::toJson() const {
    json j;
    j["version"] = 1;
    json s = json::array();
    for (const Key& k : size) s.push_back({k.t, k.value});
    json g = json::array();
    for (const ColorKey& k : gradient) g.push_back({k.t, k.color.x, k.color.y, k.color.z, k.color.w});
    j["size"] = std::move(s);
    j["gradient"] = std::move(g);
    return j.dump(2);
}

Result<ParticleCurves> ParticleCurves::fromJson(std::string_view text) {
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return makeError(Error::Code::ParseError, "particle curves: invalid JSON");
    if (j.value("version", 1) > 1) return makeError(Error::Code::VersionMismatch, "particle curves: made by a newer version of AGR");
    ParticleCurves c;
    try {
        for (const json& k : j.value("size", json::array()))
            if (k.is_array() && k.size() == 2) c.size.push_back({std::clamp(k[0].get<float>(), 0.0f, 1.0f), k[1].get<float>()});
        for (const json& k : j.value("gradient", json::array()))
            if (k.is_array() && k.size() == 5)
                c.gradient.push_back({std::clamp(k[0].get<float>(), 0.0f, 1.0f), {k[1].get<float>(), k[2].get<float>(), k[3].get<float>(), k[4].get<float>()}});
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("particle curves: ") + e.what());
    }
    c.sort();
    return c;
}

ParticleCurvesLut bakeParticleCurves(const ParticleCurves& curves) {
    ParticleCurvesLut lut;
    lut.hasSize = !curves.size.empty();
    lut.hasColor = !curves.gradient.empty();
    for (int i = 0; i < ParticleCurvesLut::kSamples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(ParticleCurvesLut::kSamples - 1);
        lut.size[static_cast<usize>(i)] = curves.sizeAt(t);
        lut.color[static_cast<usize>(i)] = curves.colorAt(t);
    }
    return lut;
}

float ParticleCurvesLut::sizeAt(float t) const {
    const float x = std::clamp(t, 0.0f, 1.0f) * (kSamples - 1);
    const int i = std::min(static_cast<int>(x), kSamples - 2);
    const float f = x - static_cast<float>(i);
    return size[static_cast<usize>(i)] + (size[static_cast<usize>(i + 1)] - size[static_cast<usize>(i)]) * f;
}

math::Color ParticleCurvesLut::colorAt(float t) const {
    const float x = std::clamp(t, 0.0f, 1.0f) * (kSamples - 1);
    const int i = std::min(static_cast<int>(x), kSamples - 2);
    return lerpColor(color[static_cast<usize>(i)], color[static_cast<usize>(i + 1)], x - static_cast<float>(i));
}

ParticleCurvesLibrary::Entry* ParticleCurvesLibrary::entry(const std::string& uri) {
    if (uri.empty()) return nullptr;
    auto it = m_entries.find(uri);
    if (it != m_entries.end()) return it->second.curves ? &it->second : nullptr;
    Entry& e = m_entries[uri];
    e.modifiedTime = m_vfs.modifiedTime(uri);
    auto text = m_vfs.readText(uri);
    if (!text) {
        AGR_LOG_ERROR("Particles", "'{}': {}", uri, text.error().message);
        return nullptr;
    }
    auto parsed = ParticleCurves::fromJson(text.value());
    if (!parsed) {
        AGR_LOG_ERROR("Particles", "'{}': {}", uri, parsed.error().message);
        return nullptr;
    }
    e.curves = std::make_unique<ParticleCurves>(std::move(parsed.value()));
    e.lut = std::make_unique<ParticleCurvesLut>(bakeParticleCurves(*e.curves));
    return &e;
}

const ParticleCurvesLut* ParticleCurvesLibrary::get(const std::string& uri) {
    Entry* e = entry(uri);
    return e ? e->lut.get() : nullptr;
}

const ParticleCurves* ParticleCurvesLibrary::curves(const std::string& uri) {
    Entry* e = entry(uri);
    return e ? e->curves.get() : nullptr;
}

Status ParticleCurvesLibrary::save(const std::string& uri, const ParticleCurves& curves) {
    if (Status s = m_vfs.writeText(uri, curves.toJson()); !s) return s;
    preview(uri, curves);
    m_entries[uri].modifiedTime = m_vfs.modifiedTime(uri);
    return {};
}

void ParticleCurvesLibrary::preview(const std::string& uri, const ParticleCurves& curves) {
    Entry& e = m_entries[uri];
    e.curves = std::make_unique<ParticleCurves>(curves);
    e.curves->sort();
    e.lut = std::make_unique<ParticleCurvesLut>(bakeParticleCurves(*e.curves));
}

u32 ParticleCurvesLibrary::reloadChanged() {
    u32 n = 0;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (m_vfs.modifiedTime(it->first) != it->second.modifiedTime) {
            it = m_entries.erase(it);
            ++n;
        } else {
            ++it;
        }
    }
    return n;
}

} // namespace agr
