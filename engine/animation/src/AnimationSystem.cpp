#include "agr/animation/AnimationSystem.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/scene/Components.hpp"
#include "agr/scene/Reflection.hpp"
#include "agr/twod/SpriteAnimator.hpp"

namespace agr {

namespace {

struct Binding {
    ecs::Entity entity;
    const ComponentInfo* component = nullptr;
    const FieldInfo* field = nullptr;
};

float sampleTime(const AnimationClip& c, float t) {
    if (c.duration <= 0.0f) return 0.0f;
    if (c.loop) {
        const float m = std::fmod(t, c.duration);
        return m < 0.0f ? m + c.duration : m;
    }
    return std::clamp(t, 0.0f, c.duration);
}

void* componentPtr(Scene& scene, const Binding& b) {
    if (!b.component || !scene.valid(b.entity)) return nullptr;
    ecs::PoolBase* pool = scene.registry().findPool(b.component->id);
    return pool ? pool->getRaw(b.entity) : nullptr;
}

/// Value of one channel of a field (-1 / 0 for scalars). Rotations: Euler degrees.
std::optional<float> readChannel(void* c, const FieldInfo& f, i32 channel) {
    const i32 ch = std::clamp(channel, 0, 3);
    switch (f.type) {
        case FieldType::Float: return f.ref<float>(c);
        case FieldType::Int32:
        case FieldType::Enum: return static_cast<float>(f.ref<i32>(c));
        case FieldType::UInt32: return static_cast<float>(f.ref<u32>(c));
        case FieldType::Bool: return f.ref<bool>(c) ? 1.0f : 0.0f;
        case FieldType::Vec2: return ch < 2 ? std::optional<float>((&f.ref<math::Vec2>(c).x)[ch]) : std::nullopt;
        case FieldType::Vec3: return ch < 3 ? std::optional<float>((&f.ref<math::Vec3>(c).x)[ch]) : std::nullopt;
        case FieldType::Vec4: return (&f.ref<math::Vec4>(c).x)[ch];
        case FieldType::Quat: {
            if (ch > 2) return std::nullopt;
            const math::Vec3 e = f.ref<math::Quat>(c).toEuler();
            return math::degrees((&e.x)[ch]);
        }
        default: return std::nullopt;
    }
}

bool animatable(FieldType t) {
    switch (t) {
        case FieldType::Float:
        case FieldType::Int32:
        case FieldType::UInt32:
        case FieldType::Enum:
        case FieldType::Bool:
        case FieldType::Vec2:
        case FieldType::Vec3:
        case FieldType::Vec4:
        case FieldType::Quat:
        case FieldType::String: return true;
        default: return false;
    }
}

/// Values gathered for one field before writing it once.
struct FieldWrite {
    void* component;
    const FieldInfo* field;
    float v[4];
    u8 mask = 0;
    const std::string* text = nullptr;
};

void writeField(FieldWrite& w) {
    void* c = w.component;
    const FieldInfo& f = *w.field;
    switch (f.type) {
        case FieldType::Float: f.ref<float>(c) = w.v[0]; break;
        case FieldType::Int32:
        case FieldType::Enum: f.ref<i32>(c) = static_cast<i32>(std::lround(w.v[0])); break;
        case FieldType::UInt32: f.ref<u32>(c) = static_cast<u32>(std::max(0L, std::lround(w.v[0]))); break;
        case FieldType::Bool: f.ref<bool>(c) = w.v[0] >= 0.5f; break;
        case FieldType::Vec2:
        case FieldType::Vec3:
        case FieldType::Vec4: {
            float* dst = f.type == FieldType::Vec2 ? &f.ref<math::Vec2>(c).x : f.type == FieldType::Vec3 ? &f.ref<math::Vec3>(c).x : &f.ref<math::Vec4>(c).x;
            const int n = f.type == FieldType::Vec2 ? 2 : f.type == FieldType::Vec3 ? 3 : 4;
            for (int i = 0; i < n; ++i)
                if (w.mask & (1u << i)) dst[i] = w.v[i];
            break;
        }
        case FieldType::Quat: {
            math::Quat& q = f.ref<math::Quat>(c);
            math::Vec3 e = q.toEuler();
            for (int i = 0; i < 3; ++i)
                if (w.mask & (1u << i)) (&e.x)[i] = math::radians(w.v[i]);
            q = math::Quat::fromEuler(e);
            break;
        }
        case FieldType::String:
            if (w.text) f.ref<std::string>(c) = *w.text;
            break;
        default: break;
    }
}

} // namespace

// ------------------------------------------------------------------ library
Result<std::shared_ptr<const AnimationClip>> AnimationLibrary::clip(std::string_view uri) {
    const std::string key(uri);
    if (auto it = m_clips.find(key); it != m_clips.end()) return it->second.value;
    auto text = m_vfs.readText(uri);
    if (!text) return text.error();
    auto parsed = AnimationClip::fromJson(text.value());
    if (!parsed) return makeError(parsed.error().code, key + ": " + parsed.error().message);
    auto ptr = std::make_shared<const AnimationClip>(std::move(parsed.value()));
    m_clips[key] = {ptr, m_vfs.modifiedTime(uri)};
    return ptr;
}

Result<std::shared_ptr<const AnimatorController>> AnimationLibrary::controller(std::string_view uri) {
    const std::string key(uri);
    if (auto it = m_controllers.find(key); it != m_controllers.end()) return it->second.value;
    auto text = m_vfs.readText(uri);
    if (!text) return text.error();
    auto parsed = AnimatorController::fromJson(text.value());
    if (!parsed) return makeError(parsed.error().code, key + ": " + parsed.error().message);
    auto ptr = std::make_shared<const AnimatorController>(std::move(parsed.value()));
    m_controllers[key] = {ptr, m_vfs.modifiedTime(uri)};
    return ptr;
}

u32 AnimationLibrary::reloadChanged() {
    u32 n = 0;
    for (auto it = m_clips.begin(); it != m_clips.end();) {
        if (m_vfs.modifiedTime(it->first) != it->second.modified) {
            it = m_clips.erase(it);
            ++n;
        } else {
            ++it;
        }
    }
    for (auto it = m_controllers.begin(); it != m_controllers.end();) {
        if (m_vfs.modifiedTime(it->first) != it->second.modified) {
            it = m_controllers.erase(it);
            ++n;
        } else {
            ++it;
        }
    }
    return n;
}

// ------------------------------------------------------------------ runtime
struct AnimationSystem::Runtime {
    struct Playback {
        std::shared_ptr<const AnimationClip> clip;
        std::vector<Binding> bindings;  // parallel to clip->tracks
        float time = 0.0f;
        float speed = 1.0f;
        std::string state;
        bool fresh = true;  // events at t = 0 fire on the first update
    };
    std::string controllerUri, clipUri;
    std::shared_ptr<const AnimatorController> controller;
    std::unordered_map<std::string, float> params;
    Playback current, previous;
    bool fading = false;
    float fade = 0.0f, fadeDuration = 0.0f;
    float stateTime = 0.0f;
    bool valid = false;  // false: missing assets (reported once)
};

AnimationSystem::AnimationSystem(VirtualFileSystem& vfs) : m_vfs(vfs), m_library(vfs) {}
AnimationSystem::~AnimationSystem() = default;

ecs::Entity AnimationSystem::resolvePath(Scene& scene, ecs::Entity root, std::string_view path) {
    ecs::Entity e = root;
    while (!path.empty() && !e.isNull()) {
        const usize slash = path.find('/');
        const std::string_view name = path.substr(0, slash);
        ecs::Entity found;
        scene.forEachChild(e, [&](ecs::Entity c) {
            if (found.isNull() && scene.nameOf(c) == name) found = c;
        });
        e = found;
        path = slash == std::string_view::npos ? std::string_view{} : path.substr(slash + 1);
    }
    return e;
}

std::optional<std::string> AnimationSystem::pathOf(Scene& scene, ecs::Entity root, ecs::Entity e) {
    std::vector<std::string> names;
    for (ecs::Entity x = e; x != root; x = scene.parent(x)) {
        if (x.isNull()) return std::nullopt;
        names.push_back(scene.nameOf(x));
    }
    std::string path;
    for (auto it = names.rbegin(); it != names.rend(); ++it) {
        if (!path.empty()) path += '/';
        path += *it;
    }
    return path;
}

static std::vector<Binding> bindTracks(Scene& scene, ecs::Entity root, const AnimationClip& clip) {
    std::vector<Binding> out;
    out.reserve(clip.tracks.size());
    auto& registry = ComponentRegistry::instance();
    for (const AnimationTrack& t : clip.tracks) {
        Binding b;
        b.entity = AnimationSystem::resolvePath(scene, root, t.target);
        b.component = registry.find(t.component);
        b.field = b.component ? b.component->field(t.field) : nullptr;
        if (!b.field || !animatable(b.field->type)) b = {};
        out.push_back(b);
    }
    return out;
}

void AnimationSystem::apply(Scene& scene, ecs::Entity root, Runtime* rt, const AnimationClip* from, float fromTime, const AnimationClip& to,
                            float toTime, float weight) {
    std::vector<Binding> local;
    const std::vector<Binding>* toBindings = nullptr;
    if (rt && rt->current.clip.get() == &to) {
        toBindings = &rt->current.bindings;
    } else {
        local = bindTracks(scene, root, to);
        toBindings = &local;
    }
    std::vector<FieldWrite> writes;
    const float ts = sampleTime(to, toTime);
    const float fs = from ? sampleTime(*from, fromTime) : 0.0f;
    for (usize i = 0; i < to.tracks.size(); ++i) {
        const AnimationTrack& track = to.tracks[i];
        const Binding& b = (*toBindings)[i];
        if (track.curve.empty()) continue;
        void* c = componentPtr(scene, b);
        if (!c) continue;
        auto w = std::find_if(writes.begin(), writes.end(), [&](const FieldWrite& fw) { return fw.component == c && fw.field == b.field; });
        if (w == writes.end()) {
            writes.push_back({c, b.field, {0, 0, 0, 0}, 0, nullptr});
            w = writes.end() - 1;
        }
        if (b.field->type == FieldType::String) {
            w->text = &track.curve.evaluateText(ts);
            continue;
        }
        float v = track.curve.evaluate(ts);
        if (weight < 1.0f) {
            const AnimationTrack* old = from ? from->findTrack(track.target, track.component, track.field, track.channel) : nullptr;
            const std::optional<float> base = old && !old->curve.empty() ? std::optional<float>(old->curve.evaluate(fs)) : readChannel(c, *b.field, track.channel);
            if (base) v = *base + (v - *base) * weight;
        }
        const i32 ch = std::clamp(track.channel, 0, 3);
        w->v[ch] = v;
        w->mask |= static_cast<u8>(1u << ch);
    }
    for (FieldWrite& w : writes) writeField(w);
    m_stats.tracksApplied += static_cast<u32>(to.tracks.size());
}

AnimationSystem::Runtime* AnimationSystem::runtime(Scene& scene, ecs::Entity e) {
    auto* anim = scene.registry().tryGet<AnimatorComponent>(e);
    if (!anim) return nullptr;
    if (&scene != m_scene || scene.generation() != m_sceneGeneration) {
        m_runtimes.clear();
        m_scene = &scene;
        m_sceneGeneration = scene.generation();
    }
    auto& slot = m_runtimes[e.packed()];
    std::shared_ptr<const AnimatorController> controller;
    if (!anim->controller.empty()) {
        if (auto c = m_library.controller(anim->controller)) controller = c.value();
    }
    // (Re)created when the component's assets change or a file was reloaded.
    const bool stale = !slot || slot->controllerUri != anim->controller || slot->clipUri != anim->clip ||
                       (controller && slot->controller != controller);
    if (!stale) return slot.get();
    slot = std::make_unique<Runtime>();
    Runtime& rt = *slot;
    rt.controllerUri = anim->controller;
    rt.clipUri = anim->clip;
    if (!anim->controller.empty()) {
        auto c = m_library.controller(anim->controller);
        if (!c) {
            AGR_LOG_ERROR("Animation", "'{}': {}", scene.nameOf(e), c.error().message);
            return slot.get();
        }
        rt.controller = c.value();
        for (const std::string& p : rt.controller->validate()) AGR_LOG_WARN("Animation", "{}: {}", anim->controller, p);
        for (const AnimatorParameter& p : rt.controller->parameters) rt.params[p.name] = p.type == ParameterType::Trigger ? 0.0f : p.defaultValue;
        const AnimatorState* entry = rt.controller->findState(rt.controller->entry);
        if (!entry && !rt.controller->states.empty()) entry = &rt.controller->states.front();
        rt.valid = entry != nullptr;
        if (entry) enterState(scene, e, rt, entry, 0.0f);
    } else if (!anim->clip.empty()) {
        auto c = m_library.clip(anim->clip);
        if (!c) {
            AGR_LOG_ERROR("Animation", "'{}': {}", scene.nameOf(e), c.error().message);
            return slot.get();
        }
        rt.current.clip = c.value();
        rt.current.bindings = bindTracks(scene, e, *rt.current.clip);
        rt.valid = true;
    }
    return slot.get();
}

void AnimationSystem::enterState(Scene& scene, ecs::Entity e, Runtime& rt, const AnimatorState* state, float fade) {
    if (fade > 0.0f && rt.current.clip) {
        rt.previous = std::move(rt.current);
        rt.fading = true;
        rt.fade = 0.0f;
        rt.fadeDuration = fade;
    } else {
        rt.previous = {};
        rt.fading = false;
    }
    rt.current = {};
    rt.current.state = state->name;
    rt.current.speed = state->speed;
    rt.stateTime = 0.0f;
    if (!state->clip.empty()) {
        if (auto c = m_library.clip(state->clip)) {
            rt.current.clip = c.value();
            rt.current.bindings = bindTracks(scene, e, *rt.current.clip);
        } else {
            AGR_LOG_ERROR("Animation", "state '{}': {}", state->name, c.error().message);
        }
    }
    if (!state->spriteClip.empty())
        if (auto* sa = scene.registry().tryGet<SpriteAnimatorComponent>(e)) sa->clip = state->spriteClip;
    if (!rt.fading) rt.previous = {};
    m_events.push_back({e, AnimationEvent::Kind::StateEnter, state->name});
}

void AnimationSystem::update(Scene& scene, float dt) {
    Stopwatch sw;
    m_stats.animators = 0;
    m_stats.tracksApplied = 0;
    if (&scene != m_scene || scene.generation() != m_sceneGeneration) {
        m_runtimes.clear();
        m_scene = &scene;
        m_sceneGeneration = scene.generation();
    }
    std::unordered_set<u64> alive;
    std::vector<ecs::Entity> animated;
    scene.registry().view<AnimatorComponent>().each([&](ecs::Entity e, AnimatorComponent&) { animated.push_back(e); });
    for (ecs::Entity e : animated) {
        alive.insert(e.packed());
        if (!scene.isActiveInHierarchy(e)) continue;
        auto& anim = scene.registry().get<AnimatorComponent>(e);
        Runtime* rt = runtime(scene, e);
        if (!rt || !rt->valid) continue;
        ++m_stats.animators;
        if (!anim.playing) continue;
        const float step = dt * anim.speed;
        Runtime::Playback& cur = rt->current;
        const float t0 = cur.time;
        cur.time += step * cur.speed;
        rt->stateTime += step;
        // Clip events crossed this frame (every loop).
        if (cur.clip && !cur.clip->events.empty() && cur.time != t0) {
            const AnimationClip& c = *cur.clip;
            const float d = std::max(c.duration, 1e-4f);
            const float lo = std::min(t0, cur.time), hi = std::max(t0, cur.time);
            const i64 n0 = c.loop ? static_cast<i64>(std::floor(lo / d)) : 0, n1 = c.loop ? static_cast<i64>(std::floor(hi / d)) : 0;
            for (i64 n = n0; n <= n1; ++n)
                for (const AnimationEventKey& ev : c.events) {
                    const float at = static_cast<float>(n) * d + ev.time;
                    if (!c.loop && at > c.duration) continue;
                    if ((at > lo || (cur.fresh && at >= lo)) && at <= hi) m_events.push_back({e, AnimationEvent::Kind::ClipEvent, ev.name});
                }
        }
        cur.fresh = false;
        if (rt->fading) {
            rt->previous.time += step * rt->previous.speed;
            rt->fade += step;
            if (rt->fade >= rt->fadeDuration) {
                rt->fading = false;
                rt->previous = {};
            }
        }
        // Transitions (one per frame; not while crossfading).
        if (rt->controller && !rt->fading) {
            for (const AnimatorTransition& tr : rt->controller->transitions) {
                if (tr.from != "*" && tr.from != cur.state) continue;
                if (tr.from == "*" && tr.to == cur.state) continue;
                if (tr.exitTime >= 0.0f) {
                    const float d = cur.clip ? cur.clip->duration : 0.0f;
                    const float normalized = d > 0.0f ? cur.time / d : 1.0f;
                    if (normalized < tr.exitTime) continue;
                }
                bool ok = true;
                for (const TransitionCondition& c : tr.conditions) {
                    auto p = rt->params.find(c.parameter);
                    const float v = p == rt->params.end() ? 0.0f : p->second;
                    switch (c.op) {
                        case ConditionOp::Greater: ok = v > c.value; break;
                        case ConditionOp::Less: ok = v < c.value; break;
                        case ConditionOp::Equal: ok = std::fabs(v - c.value) < 1e-4f; break;
                        case ConditionOp::NotEqual: ok = std::fabs(v - c.value) >= 1e-4f; break;
                        case ConditionOp::IsTrue: ok = v >= 0.5f; break;
                        case ConditionOp::IsFalse: ok = v < 0.5f; break;
                    }
                    if (!ok) break;
                }
                if (!ok) continue;
                const AnimatorState* target = rt->controller->findState(tr.to);
                if (!target) continue;
                for (const TransitionCondition& c : tr.conditions) {
                    const AnimatorParameter* p = rt->controller->findParameter(c.parameter);
                    if (p && p->type == ParameterType::Trigger) rt->params[c.parameter] = 0.0f;  // consumed
                }
                enterState(scene, e, *rt, target, tr.duration);
                ++m_stats.transitions;
                break;
            }
        }
        Runtime::Playback& now = rt->current;
        if (now.clip) {
            const float w = rt->fading && rt->fadeDuration > 0.0f ? std::clamp(rt->fade / rt->fadeDuration, 0.0f, 1.0f) : 1.0f;
            apply(scene, e, rt, rt->fading ? rt->previous.clip.get() : nullptr, rt->previous.time, *now.clip, now.time, w);
        }
    }
    for (auto it = m_runtimes.begin(); it != m_runtimes.end();) {
        if (!alive.count(it->first)) it = m_runtimes.erase(it);
        else ++it;
    }
    m_stats.clips = static_cast<u32>(m_library.clipCount());
    m_stats.controllers = static_cast<u32>(m_library.controllerCount());
    m_stats.updateMs = sw.elapsedMilliseconds();
}

std::vector<AnimationEvent> AnimationSystem::takeEvents() {
    std::vector<AnimationEvent> out;
    out.swap(m_events);
    return out;
}

void AnimationSystem::reset() {
    m_runtimes.clear();
    m_events.clear();
}

// ---------------------------------------------------------------- parameters
bool AnimationSystem::setFloat(Scene& scene, ecs::Entity e, std::string_view name, float value) {
    Runtime* rt = runtime(scene, e);
    if (!rt || !rt->controller) return false;
    const AnimatorParameter* p = rt->controller->findParameter(name);
    if (!p) return false;
    rt->params[p->name] = p->type == ParameterType::Int ? std::round(value) : value;
    return true;
}

bool AnimationSystem::setBool(Scene& scene, ecs::Entity e, std::string_view name, bool value) {
    return setFloat(scene, e, name, value ? 1.0f : 0.0f);
}

bool AnimationSystem::setTrigger(Scene& scene, ecs::Entity e, std::string_view name) { return setFloat(scene, e, name, 1.0f); }
bool AnimationSystem::resetTrigger(Scene& scene, ecs::Entity e, std::string_view name) { return setFloat(scene, e, name, 0.0f); }

std::optional<float> AnimationSystem::parameter(Scene& scene, ecs::Entity e, std::string_view name) {
    Runtime* rt = runtime(scene, e);
    if (!rt) return std::nullopt;
    auto it = rt->params.find(std::string(name));
    if (it == rt->params.end()) return std::nullopt;
    return it->second;
}

bool AnimationSystem::play(Scene& scene, ecs::Entity e, std::string_view state, float fade) {
    Runtime* rt = runtime(scene, e);
    if (!rt || !rt->controller) return false;
    const AnimatorState* s = rt->controller->findState(state);
    if (!s) return false;
    enterState(scene, e, *rt, s, fade);
    rt->valid = true;
    return true;
}

std::string AnimationSystem::currentState(ecs::Entity e) const {
    auto it = m_runtimes.find(e.packed());
    return it == m_runtimes.end() ? std::string() : it->second->current.state;
}

float AnimationSystem::stateTime(ecs::Entity e) const {
    auto it = m_runtimes.find(e.packed());
    return it == m_runtimes.end() ? 0.0f : it->second->stateTime;
}

void AnimationSystem::preview(Scene& scene, ecs::Entity root, const AnimationClip& clip, float time) {
    apply(scene, root, nullptr, nullptr, 0.0f, clip, time, 1.0f);
}

std::optional<float> AnimationSystem::readTrack(Scene& scene, ecs::Entity root, const AnimationTrack& track) {
    AnimationClip one;
    one.tracks.push_back(track);
    const std::vector<Binding> b = bindTracks(scene, root, one);
    void* c = componentPtr(scene, b[0]);
    if (!c) return std::nullopt;
    return readChannel(c, *b[0].field, track.channel);
}

std::optional<std::string> AnimationSystem::readTrackText(Scene& scene, ecs::Entity root, const AnimationTrack& track) {
    AnimationClip one;
    one.tracks.push_back(track);
    const std::vector<Binding> b = bindTracks(scene, root, one);
    void* c = componentPtr(scene, b[0]);
    if (!c || b[0].field->type != FieldType::String) return std::nullopt;
    return b[0].field->ref<std::string>(c);
}

bool AnimationSystem::isTextTrack(const AnimationTrack& track) {
    const ComponentInfo* info = ComponentRegistry::instance().find(track.component);
    const FieldInfo* f = info ? info->field(track.field) : nullptr;
    return f && f->type == FieldType::String;
}

void registerAnimationComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<AnimatorComponent>()) return;
    reg.add<AnimatorComponent>("Animator")
        .category("Animation")
        .description("Animates fields of the entity and its children with clips (.agranim) and a state machine (.agrctrl).")
        .field("controller", &AnimatorComponent::controller, {.tooltip = "State machine (.agrctrl)", .flags = FieldFlag_AssetPath})
        .field("clip", &AnimatorComponent::clip, {.tooltip = "Clip played alone when there is no state machine (.agranim)", .flags = FieldFlag_AssetPath})
        .field("speed", &AnimatorComponent::speed, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("playing", &AnimatorComponent::playing);
}

} // namespace agr
