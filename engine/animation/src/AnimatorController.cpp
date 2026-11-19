#include "agr/animation/AnimatorController.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace agr {

using json = nlohmann::json;

namespace {
constexpr const char* kParamTypes[] = {"float", "int", "bool", "trigger"};
constexpr const char* kOps[] = {">", "<", "==", "!=", "true", "false"};

template <typename E, usize N>
E fromName(const std::string& s, const char* const (&names)[N], E fallback) {
    for (usize i = 0; i < N; ++i)
        if (s == names[i]) return static_cast<E>(i);
    return fallback;
}
} // namespace

const char* conditionOpName(ConditionOp op) { return kOps[std::min<usize>(static_cast<usize>(op), 5)]; }

const AnimatorState* AnimatorController::findState(std::string_view name) const {
    for (const AnimatorState& s : states)
        if (s.name == name) return &s;
    return nullptr;
}

const AnimatorParameter* AnimatorController::findParameter(std::string_view name) const {
    for (const AnimatorParameter& p : parameters)
        if (p.name == name) return &p;
    return nullptr;
}

namespace {

const AnimatorState* findIn(const std::vector<AnimatorState>& states, std::string_view name) {
    for (const AnimatorState& s : states)
        if (s.name == name) return &s;
    return nullptr;
}

void validateMachine(const AnimatorController& c, const std::vector<AnimatorState>& states, const std::vector<AnimatorTransition>& transitions,
                     const std::string& entry, const std::string& where, std::vector<std::string>& problems) {
    if (states.empty()) problems.push_back(where + "no states");
    if (!entry.empty() && !findIn(states, entry)) problems.push_back(where + "entry state '" + entry + "' does not exist");
    for (usize i = 0; i < states.size(); ++i)
        for (usize k = i + 1; k < states.size(); ++k)
            if (states[i].name == states[k].name) problems.push_back(where + "two states are named '" + states[i].name + "'");
    auto checkNumber = [&](const AnimatorState& s, const std::string& name) {
        const AnimatorParameter* p = c.findParameter(name);
        if (!p) problems.push_back(where + "blend state '" + s.name + "' uses unknown parameter '" + name + "'");
        else if (p->type == ParameterType::Bool || p->type == ParameterType::Trigger)
            problems.push_back(where + "blend state '" + s.name + "' needs a float or int parameter, '" + name + "' is not");
    };
    for (const AnimatorState& s : states) {
        if (!s.isBlend()) continue;
        checkNumber(s, s.blendParameter);
        if (s.isBlend2D()) checkNumber(s, s.blendParameterY);
        for (const BlendMotion& m : s.motions)
            if (m.clip.empty()) problems.push_back(where + "blend state '" + s.name + "' has a motion without clip");
    }
    for (const AnimatorTransition& t : transitions) {
        if (t.from != "*" && !findIn(states, t.from)) problems.push_back(where + "transition from unknown state '" + t.from + "'");
        if (!findIn(states, t.to)) problems.push_back(where + "transition to unknown state '" + t.to + "'");
        for (const TransitionCondition& cond : t.conditions)
            if (!c.findParameter(cond.parameter)) problems.push_back(where + "condition on unknown parameter '" + cond.parameter + "'");
        if (t.conditions.empty() && t.exitTime < 0.0f)
            problems.push_back(where + "transition " + t.from + " -> " + t.to + " has no condition and no exit time (it would fire immediately)");
    }
}

json statesToJson(const std::vector<AnimatorState>& states) {
    json ss = json::array();
    for (const AnimatorState& s : states) {
        json st = {{"name", s.name}, {"clip", s.clip}, {"spriteClip", s.spriteClip}, {"speed", s.speed},
                   {"position", {s.editorPosition.x, s.editorPosition.y}}};
        if (s.isBlend()) {
            json ms = json::array();
            for (const BlendMotion& m : s.motions) {
                json mj = {{"clip", m.clip}, {"threshold", m.threshold}};
                if (s.isBlend2D()) mj["position"] = {m.position.x, m.position.y};
                ms.push_back(std::move(mj));
            }
            st["blend"] = {{"parameter", s.blendParameter}, {"motions", std::move(ms)}};
            if (s.isBlend2D()) st["blend"]["parameterY"] = s.blendParameterY;
        }
        ss.push_back(std::move(st));
    }
    return ss;
}

json transitionsToJson(const std::vector<AnimatorTransition>& transitions) {
    json ts = json::array();
    for (const AnimatorTransition& t : transitions) {
        json cs = json::array();
        for (const TransitionCondition& c : t.conditions) cs.push_back({{"param", c.parameter}, {"op", conditionOpName(c.op)}, {"value", c.value}});
        ts.push_back({{"from", t.from}, {"to", t.to}, {"conditions", std::move(cs)}, {"exitTime", t.exitTime}, {"duration", t.duration}});
    }
    return ts;
}

std::vector<AnimatorState> statesFromJson(const json& arr) {
    std::vector<AnimatorState> out;
    for (const json& s : arr) {
        AnimatorState st;
        st.name = s.value("name", "");
        st.clip = s.value("clip", "");
        st.spriteClip = s.value("spriteClip", "");
        st.speed = s.value("speed", 1.0f);
        if (s.contains("position") && s["position"].is_array() && s["position"].size() == 2)
            st.editorPosition = {s["position"][0].get<float>(), s["position"][1].get<float>()};
        if (const auto b = s.find("blend"); b != s.end() && b->is_object()) {
            st.blendParameter = b->value("parameter", "");
            st.blendParameterY = b->value("parameterY", "");
            for (const json& m : b->value("motions", json::array())) {
                BlendMotion bm{m.value("clip", ""), m.value("threshold", 0.0f), {0.0f, 0.0f}};
                if (m.contains("position") && m["position"].is_array() && m["position"].size() == 2)
                    bm.position = {m["position"][0].get<float>(), m["position"][1].get<float>()};
                st.motions.push_back(std::move(bm));
            }
            if (st.blendParameterY.empty())
                std::stable_sort(st.motions.begin(), st.motions.end(), [](const BlendMotion& a, const BlendMotion& b) { return a.threshold < b.threshold; });
        }
        out.push_back(std::move(st));
    }
    return out;
}

std::vector<AnimatorTransition> transitionsFromJson(const json& arr) {
    std::vector<AnimatorTransition> out;
    for (const json& t : arr) {
        AnimatorTransition tr;
        tr.from = t.value("from", "*");
        tr.to = t.value("to", "");
        tr.exitTime = t.value("exitTime", -1.0f);
        tr.duration = std::max(0.0f, t.value("duration", 0.1f));
        for (const json& cj : t.value("conditions", json::array()))
            tr.conditions.push_back({cj.value("param", ""), fromName(cj.value("op", "true"), kOps, ConditionOp::IsTrue), cj.value("value", 0.0f)});
        out.push_back(std::move(tr));
    }
    return out;
}

} // namespace

bool AnimatorLayer::masks(std::string_view path) const {
    if (mask.empty()) return true;
    for (const std::string& m : mask)
        if (m.empty() || path == m || (path.size() > m.size() && path.substr(0, m.size()) == m && path[m.size()] == '/')) return true;
    return false;
}

std::vector<std::string> AnimatorController::validate() const {
    std::vector<std::string> problems;
    validateMachine(*this, states, transitions, entry, "", problems);
    for (const AnimatorLayer& l : layers) {
        validateMachine(*this, l.states, l.transitions, l.entry, "layer '" + l.name + "': ", problems);
        for (usize i = 0; i < layers.size(); ++i)
            if (&layers[i] != &l && layers[i].name == l.name && &layers[i] < &l) problems.push_back("two layers are named '" + l.name + "'");
    }
    return problems;
}

std::string AnimatorController::toJson() const {
    json j;
    j["version"] = 1;
    j["entry"] = entry;
    json ps = json::array();
    for (const AnimatorParameter& p : parameters)
        ps.push_back({{"name", p.name}, {"type", kParamTypes[static_cast<usize>(p.type)]}, {"default", p.defaultValue}});
    j["parameters"] = std::move(ps);
    j["states"] = statesToJson(states);
    j["transitions"] = transitionsToJson(transitions);
    if (!layers.empty()) {
        json ls = json::array();
        for (const AnimatorLayer& l : layers)
            ls.push_back({{"name", l.name}, {"weight", l.weight}, {"mask", l.mask}, {"entry", l.entry}, {"states", statesToJson(l.states)},
                          {"transitions", transitionsToJson(l.transitions)}});
        j["layers"] = std::move(ls);
    }
    return j.dump(2);
}

Result<AnimatorController> AnimatorController::fromJson(std::string_view text) {
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return makeError(Error::Code::ParseError, "animator: invalid JSON");
    if (j.value("version", 1) > 1) return makeError(Error::Code::VersionMismatch, "animator: made by a newer version of AGR");
    AnimatorController c;
    try {
        c.entry = j.value("entry", "");
        for (const json& p : j.value("parameters", json::array()))
            c.parameters.push_back({p.value("name", ""), fromName(p.value("type", "float"), kParamTypes, ParameterType::Float), p.value("default", 0.0f)});
        c.states = statesFromJson(j.value("states", json::array()));
        c.transitions = transitionsFromJson(j.value("transitions", json::array()));
        for (const json& lj : j.value("layers", json::array())) {
            AnimatorLayer l;
            l.name = lj.value("name", "");
            l.weight = std::clamp(lj.value("weight", 1.0f), 0.0f, 1.0f);
            for (const json& m : lj.value("mask", json::array()))
                if (m.is_string()) l.mask.push_back(m.get<std::string>());
            l.entry = lj.value("entry", "");
            l.states = statesFromJson(lj.value("states", json::array()));
            l.transitions = transitionsFromJson(lj.value("transitions", json::array()));
            c.layers.push_back(std::move(l));
        }
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("animator: ") + e.what());
    }
    return c;
}

} // namespace agr
