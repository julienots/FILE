#include "agr/animation/AnimatorController.hpp"

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

std::vector<std::string> AnimatorController::validate() const {
    std::vector<std::string> problems;
    if (states.empty()) problems.push_back("no states");
    if (!entry.empty() && !findState(entry)) problems.push_back("entry state '" + entry + "' does not exist");
    for (usize i = 0; i < states.size(); ++i)
        for (usize k = i + 1; k < states.size(); ++k)
            if (states[i].name == states[k].name) problems.push_back("two states are named '" + states[i].name + "'");
    for (const AnimatorTransition& t : transitions) {
        if (t.from != "*" && !findState(t.from)) problems.push_back("transition from unknown state '" + t.from + "'");
        if (!findState(t.to)) problems.push_back("transition to unknown state '" + t.to + "'");
        for (const TransitionCondition& c : t.conditions)
            if (!findParameter(c.parameter)) problems.push_back("condition on unknown parameter '" + c.parameter + "'");
        if (t.conditions.empty() && t.exitTime < 0.0f)
            problems.push_back("transition " + t.from + " -> " + t.to + " has no condition and no exit time (it would fire immediately)");
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
    json ss = json::array();
    for (const AnimatorState& s : states)
        ss.push_back({{"name", s.name}, {"clip", s.clip}, {"spriteClip", s.spriteClip}, {"speed", s.speed},
                      {"position", {s.editorPosition.x, s.editorPosition.y}}});
    j["states"] = std::move(ss);
    json ts = json::array();
    for (const AnimatorTransition& t : transitions) {
        json cs = json::array();
        for (const TransitionCondition& c : t.conditions) cs.push_back({{"param", c.parameter}, {"op", conditionOpName(c.op)}, {"value", c.value}});
        ts.push_back({{"from", t.from}, {"to", t.to}, {"conditions", std::move(cs)}, {"exitTime", t.exitTime}, {"duration", t.duration}});
    }
    j["transitions"] = std::move(ts);
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
        for (const json& s : j.value("states", json::array())) {
            AnimatorState st;
            st.name = s.value("name", "");
            st.clip = s.value("clip", "");
            st.spriteClip = s.value("spriteClip", "");
            st.speed = s.value("speed", 1.0f);
            if (s.contains("position") && s["position"].is_array() && s["position"].size() == 2)
                st.editorPosition = {s["position"][0].get<float>(), s["position"][1].get<float>()};
            c.states.push_back(std::move(st));
        }
        for (const json& t : j.value("transitions", json::array())) {
            AnimatorTransition tr;
            tr.from = t.value("from", "*");
            tr.to = t.value("to", "");
            tr.exitTime = t.value("exitTime", -1.0f);
            tr.duration = std::max(0.0f, t.value("duration", 0.1f));
            for (const json& cj : t.value("conditions", json::array()))
                tr.conditions.push_back({cj.value("param", ""), fromName(cj.value("op", "true"), kOps, ConditionOp::IsTrue), cj.value("value", 0.0f)});
            c.transitions.push_back(std::move(tr));
        }
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("animator: ") + e.what());
    }
    return c;
}

} // namespace agr
