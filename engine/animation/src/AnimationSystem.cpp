#include "agr/animation/AnimationSystem.hpp"
#include "agr/animation/IK2D.hpp"
#include "agr/animation/IK3D.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/scene/Components.hpp"
#include "agr/scene/Reflection.hpp"
#include "agr/assets/Model.hpp"
#include "agr/twod/SpriteAnimator.hpp"

namespace agr {

namespace {

struct Binding {
    ecs::Entity entity;
    const ComponentInfo* component = nullptr;
    const FieldInfo* field = nullptr;
};

/// Root motion source in a clip: a translation track (imported models) or Transform.position
/// curves (.agranim) of the root bone's path.
struct RootSource {
    const TransformTrack* track = nullptr;
    const Curve* axis[3] = {nullptr, nullptr, nullptr};
    explicit operator bool() const { return track || axis[0] || axis[1] || axis[2]; }
    math::Vec3 at(float t, math::Vec3 fallback) const {
        if (track) {
            const math::Vec4 v = track->sample(t);
            return {v.x, v.y, v.z};
        }
        return {axis[0] ? axis[0]->evaluate(t) : fallback.x, axis[1] ? axis[1]->evaluate(t) : fallback.y, axis[2] ? axis[2]->evaluate(t) : fallback.z};
    }
};
RootSource rootSource(const AnimationClip* c, const std::string& path) {
    RootSource s;
    if (!c) return s;
    for (const TransformTrack& t : c->transformTracks)
        if (t.target == path && t.path == TransformTrack::Path::Translation) s.track = &t;
    if (!s.track)
        for (const AnimationTrack& t : c->tracks)
            if (t.target == path && t.component == "Transform" && t.field == "position" && t.channel >= 0 && t.channel < 3) s.axis[t.channel] = &t.curve;
    return s;
}
/// Root motion: displacement from time 0 to `t`, loops unrolled.
math::Vec3 rootDisplacement(const AnimationClip& c, const RootSource& src, float t) {
    auto at = [&](float x) { return src.at(x, math::Vec3{0.0f}); };
    const math::Vec3 start = at(0.0f);
    if (c.duration <= 0.0f) return math::Vec3{0.0f};
    if (!c.loop) return at(std::clamp(t, 0.0f, c.duration)) - start;
    const float cycles = std::floor(t / c.duration);
    const math::Vec3 perCycle = at(c.duration) - start;
    return perCycle * cycles + (at(t - cycles * c.duration) - start);
}

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
Result<std::shared_ptr<const AnimationClip>> AnimationLibrary::modelClip(const std::string& key) {
    const usize hash = key.find('#');
    const std::string modelUri = key.substr(0, hash), name = key.substr(hash + 1);
    auto model = importModel(m_vfs, modelUri);
    if (!model) return makeError(model.error().code, modelUri + ": " + model.error().message);
    const ModelData& data = model.value();
    std::vector<std::string> meshNames;
    for (const ModelMesh& m : data.meshes) meshNames.push_back(m.name);
    // Every clip of the file is converted at once (a model is imported a single time).
    std::shared_ptr<const AnimationClip> wanted;
    for (const ModelAnimation& an : data.animations) {
        auto c = std::make_shared<AnimationClip>();
        c->loop = true;
        for (const ModelAnimationChannel& ch : an.channels) {
            if (ch.node < 0 || static_cast<usize>(ch.node) >= data.nodes.size()) continue;
            if (ch.path == ModelAnimationChannel::Path::Weights) {
                // Morph weights: one curve per target on MeshRenderer.morph (0..3) / morph2 (4..7).
                const std::string target = modelNodePath(data.nodes, meshNames, static_cast<usize>(ch.node));
                for (u32 w = 0; w < ch.weightCount; ++w) {
                    AnimationTrack& tr = c->track(target, "MeshRenderer", w < 4 ? "morph" : "morph2", static_cast<i32>(w % 4));
                    for (usize k = 0; k < ch.times.size(); ++k) {
                        Keyframe key;
                        key.time = ch.times[k];
                        key.value = ch.weights[k * ch.weightCount + w];
                        key.interpolation = ch.step ? Interpolation::Constant : Interpolation::Linear;
                        tr.curve.keys.push_back(key);
                    }
                }
                continue;
            }
            TransformTrack t;
            t.target = modelNodePath(data.nodes, meshNames, static_cast<usize>(ch.node));
            t.path = static_cast<TransformTrack::Path>(static_cast<u8>(ch.path));
            t.step = ch.step;
            t.times = ch.times;
            t.values = ch.values;
            c->transformTracks.push_back(std::move(t));
        }
        c->fitDuration();
        const std::string clipKey = modelUri + "#" + an.name;
        m_clips[clipKey] = {c, m_vfs.modifiedTime(modelUri)};
        if (an.name == name) wanted = c;
    }
    if (!wanted) return makeError(Error::Code::NotFound, modelUri + " has no animation named '" + name + "'");
    return wanted;
}

std::vector<std::string> AnimationLibrary::modelClipNames(std::string_view modelUri) {
    std::vector<std::string> names;
    auto model = importModel(m_vfs, modelUri);
    if (model)
        for (const ModelAnimation& an : model.value().animations) names.push_back(an.name);
    return names;
}

Result<std::shared_ptr<const AnimationClip>> AnimationLibrary::clip(std::string_view uri) {
    const std::string key(uri);
    if (auto it = m_clips.find(key); it != m_clips.end()) return it->second.value;
    if (key.find('#') != std::string::npos) return modelClip(key);
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
        if (m_vfs.modifiedTime(it->first.substr(0, it->first.find('#'))) != it->second.modified) {
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
        std::vector<ecs::Entity> transformBindings;  // parallel to clip->transformTracks
        float time = 0.0f;
        float speed = 1.0f;
        std::string state;
        bool fresh = true;  // events at t = 0 fire on the first update
        // Blend state: `clip` is the lower motion of the pair around the parameter, `second`
        // the upper one mixed with `secondWeight`; `phase` (cycles) keeps both in step.
        std::vector<std::shared_ptr<const AnimationClip>> motions;
        std::vector<float> thresholds;
        std::string blendParameter;
        float phase = 0.0f;
        std::shared_ptr<const AnimationClip> second;
        float secondTime = 0.0f, secondWeight = 0.0f;
        std::vector<i32> secondMap;  // clip->transformTracks[i] -> index in second (or -1)
        const AnimationClip* secondMapFor[2] = {nullptr, nullptr};
        // Every motion of a blend state with its bindings and this frame's weight (pose path).
        std::vector<std::vector<Binding>> motionBindings;
        std::vector<std::vector<ecs::Entity>> motionXforms;
        std::vector<float> mixWeights;
        std::vector<math::Vec2> positions;  // 2D blend: motion positions
        std::string blendParameterY;
    };
    /// A layer of the state machine: its own states and playback over the base layer.
    struct Layer {
        std::unique_ptr<Runtime> rt;
        const AnimatorLayer* def = nullptr;
        float weight = 1.0f;
        std::shared_ptr<AnimatorController> machine;  // the layer's states with the controller's parameters
    };
    std::vector<Layer> layers;
    std::string layerName;  // "" for the base layer
    std::string controllerUri, clipUri;
    std::shared_ptr<const AnimatorController> controller;
    std::unordered_map<std::string, float> params;
    Playback current, previous;
    bool fading = false;
    float fade = 0.0f, fadeDuration = 0.0f;
    bool fadeInterruptible = false;  // the running crossfade comes from an interruptible transition
    // Interrupted crossfade: the new one starts from the pose shown at that moment (frozen values
    // keyed by component / field / channel; pointers are only compared, never followed).
    bool useSnapshot = false, snapshotPending = false;
    Playback interruptedA, interruptedB;
    std::unordered_map<u64, float> frozenProps;
    std::unordered_map<u64, math::Vec4> frozenXforms;
    float stateTime = 0.0f;
    bool valid = false;  // false: missing assets (reported once)
};

// ---------------------------------------------------------------- poses
// Values of several clips mixed with weights (blend states, layers, crossfades) before writing.
struct AnimationSystem::PoseScratch {
    struct Prop {
        void* comp;
        const FieldInfo* field;
        i32 channel;
        float sum, weight;
        const std::string* text;
        float textWeight;
        float mask = 1.0f;  // layer mask weight of its target
    };
    struct Xform {
        TransformComponent* tr;
        TransformTrack::Path path;
        math::Vec4 sum;
        float weight;
        float mask = 1.0f;
    };
    struct Pose {
        std::vector<Prop> props;
        std::vector<Xform> xforms;
        std::unordered_map<u64, u32> propIndex, xformIndex;
        void clear() {
            props.clear();
            xforms.clear();
            propIndex.clear();
            xformIndex.clear();
        }
        static u64 key(const void* a, const void* b, i32 c) {
            u64 h = reinterpret_cast<uintptr_t>(a) * 0x9E3779B97F4A7C15ull;
            h ^= reinterpret_cast<uintptr_t>(b) + 0x632BE59BD9B4E019ull + (h << 6) + (h >> 2);
            return h ^ (static_cast<u64>(static_cast<u32>(c)) * 0xC2B2AE3D27D4EB4Full);
        }
        Prop& prop(void* comp, const FieldInfo* field, i32 ch) {
            auto [it, inserted] = propIndex.try_emplace(key(comp, field, ch), static_cast<u32>(props.size()));
            if (inserted) props.push_back({comp, field, ch, 0.0f, 0.0f, nullptr, 0.0f, 1.0f});
            return props[it->second];
        }
        Xform& xform(TransformComponent* tr, TransformTrack::Path path, bool* created = nullptr) {
            auto [it, inserted] = xformIndex.try_emplace(key(tr, nullptr, static_cast<i32>(path)), static_cast<u32>(xforms.size()));
            if (inserted) xforms.push_back({tr, path, {0, 0, 0, 0}, 0.0f, 1.0f});
            if (created) *created = inserted;
            return xforms[it->second];
        }
        const Prop* findProp(void* comp, const FieldInfo* field, i32 ch) const {
            auto it = propIndex.find(key(comp, field, ch));
            return it == propIndex.end() ? nullptr : &props[it->second];
        }
        const Xform* findXform(TransformComponent* tr, TransformTrack::Path path) const {
            auto it = xformIndex.find(key(tr, nullptr, static_cast<i32>(path)));
            return it == xformIndex.end() ? nullptr : &xforms[it->second];
        }
        /// Weighted sums -> values (rotations renormalized).
        void finish() {
            for (Prop& p : props)
                if (p.weight > 0.0f) p.sum /= p.weight;
            for (Xform& x : xforms) {
                if (x.weight <= 0.0f) continue;
                if (x.path == TransformTrack::Path::Rotation) {
                    const float len = std::sqrt(x.sum.x * x.sum.x + x.sum.y * x.sum.y + x.sum.z * x.sum.z + x.sum.w * x.sum.w);
                    x.sum = len > 1e-8f ? x.sum * (1.0f / len) : math::Vec4(0, 0, 0, 1);
                } else {
                    x.sum = x.sum * (1.0f / x.weight);
                }
            }
        }
    };
    Pose a, b, c, d;
};

namespace {

math::Vec4 currentXform(const TransformComponent& tr, TransformTrack::Path path) {
    if (path == TransformTrack::Path::Translation) return math::Vec4(tr.position, 0.0f);
    if (path == TransformTrack::Path::Scale) return math::Vec4(tr.scale, 0.0f);
    return {tr.rotation.x, tr.rotation.y, tr.rotation.z, tr.rotation.w};
}

void setXform(TransformComponent& tr, TransformTrack::Path path, math::Vec4 v) {
    switch (path) {
        case TransformTrack::Path::Translation: tr.position = {v.x, v.y, v.z}; break;
        case TransformTrack::Path::Scale: tr.scale = {v.x, v.y, v.z}; break;
        case TransformTrack::Path::Rotation: tr.rotation = {v.x, v.y, v.z, v.w}; break;
    }
}

/// Adds `clip` at `time` with `weight` to the pose (tracks outside the layer's mask skipped).
void addClipToPose(Scene& scene, ecs::Entity root, const AnimationClip& clip, const std::vector<Binding>& bindings, const std::vector<ecs::Entity>& xforms,
                   float time, float weight, const AnimatorLayer* layer, AnimationSystem::PoseScratch::Pose& pose) {
    if (weight <= 0.0f) return;
    const float ts = sampleTime(clip, time);
    for (usize i = 0; i < clip.tracks.size() && i < bindings.size(); ++i) {
        const AnimationTrack& track = clip.tracks[i];
        const float mw = layer ? layer->maskWeight(track.target) : 1.0f;
        if (track.curve.empty() || mw <= 0.0f) continue;
        void* c = componentPtr(scene, bindings[i]);
        if (!c) continue;
        auto& p = pose.prop(c, bindings[i].field, std::clamp(track.channel, 0, 3));
        p.mask = mw;
        if (bindings[i].field->type == FieldType::String) {
            if (weight > p.textWeight) {
                p.text = &track.curve.evaluateText(ts);
                p.textWeight = weight;
            }
            continue;
        }
        p.sum += track.curve.evaluate(ts) * weight;
        p.weight += weight;
    }
    auto& reg = scene.registry();
    for (usize i = 0; i < clip.transformTracks.size(); ++i) {
        const TransformTrack& track = clip.transformTracks[i];
        const float mw = layer ? layer->maskWeight(track.target) : 1.0f;
        if (mw <= 0.0f) continue;
        const ecs::Entity target = i < xforms.size() ? xforms[i] : AnimationSystem::resolvePath(scene, root, track.target);
        if (target.isNull() || !scene.valid(target)) continue;
        auto* tr = reg.tryGet<TransformComponent>(target);
        if (!tr) continue;
        math::Vec4 v = track.sample(ts);
        bool created = false;
        auto& x = pose.xform(tr, track.path, &created);
        x.mask = mw;
        // Quaternions: keep every sample in the hemisphere of the first one before summing.
        if (track.path == TransformTrack::Path::Rotation && !created && x.sum.x * v.x + x.sum.y * v.y + x.sum.z * v.z + x.sum.w * v.w < 0.0f) v = v * -1.0f;
        x.sum = x.sum + v * weight;
        x.weight += weight;
    }
}

} // namespace

AnimationSystem::AnimationSystem(VirtualFileSystem& vfs) : m_vfs(vfs), m_library(vfs), m_pose(std::make_unique<PoseScratch>()) {}
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

    // Transform tracks (imported skeletal / node animations).
    if (!to.transformTracks.empty()) {
        const bool cached = rt && rt->current.clip.get() == &to && rt->current.transformBindings.size() == to.transformTracks.size();
        auto& reg = scene.registry();
        const float fromSample = from ? sampleTime(*from, fromTime) : 0.0f;
        // Blend states mix in their second motion (same target and path) with its weight.
        auto blended = [&](Runtime::Playback* pb, const AnimationClip& clip, usize index, math::Vec4 v) {
            if (!pb || pb->clip.get() != &clip || !pb->second || pb->secondWeight <= 0.0f) return v;
            const AnimationClip& second = *pb->second;
            if (pb->secondMapFor[0] != &clip || pb->secondMapFor[1] != &second) {
                pb->secondMap.assign(clip.transformTracks.size(), -1);
                for (usize a = 0; a < clip.transformTracks.size(); ++a)
                    for (usize b = 0; b < second.transformTracks.size(); ++b)
                        if (second.transformTracks[b].path == clip.transformTracks[a].path && second.transformTracks[b].target == clip.transformTracks[a].target)
                            pb->secondMap[a] = static_cast<i32>(b);
                pb->secondMapFor[0] = &clip;
                pb->secondMapFor[1] = &second;
            }
            const i32 k = pb->secondMap[index];
            if (k < 0) return v;
            const TransformTrack& t = second.transformTracks[static_cast<usize>(k)];
            return blendTransform(t.path, v, t.sample(sampleTime(second, pb->secondTime)), pb->secondWeight);
        };
        Runtime::Playback* toPlayback = rt ? &rt->current : nullptr;
        Runtime::Playback* fromPlayback = rt && rt->fading ? &rt->previous : nullptr;
        for (usize i = 0; i < to.transformTracks.size(); ++i) {
            const TransformTrack& track = to.transformTracks[i];
            ecs::Entity target = cached ? rt->current.transformBindings[i] : resolvePath(scene, root, track.target);
            if (target.isNull() || !scene.valid(target)) continue;
            auto* tr = reg.tryGet<TransformComponent>(target);
            if (!tr) continue;
            math::Vec4 v = blended(toPlayback, to, i, track.sample(ts));
            if (weight < 1.0f) {
                // Crossfade from the previous clip's value for the same target, else from the current pose.
                math::Vec4 base;
                const TransformTrack* old = nullptr;
                usize oldIndex = 0;
                if (from)
                    for (usize o = 0; o < from->transformTracks.size(); ++o)
                        if (from->transformTracks[o].path == track.path && from->transformTracks[o].target == track.target) {
                            old = &from->transformTracks[o];
                            oldIndex = o;
                        }
                if (old) base = blended(fromPlayback, *from, oldIndex, old->sample(fromSample));
                else if (track.path == TransformTrack::Path::Translation) base = math::Vec4(tr->position, 0.0f);
                else if (track.path == TransformTrack::Path::Scale) base = math::Vec4(tr->scale, 0.0f);
                else base = {tr->rotation.x, tr->rotation.y, tr->rotation.z, tr->rotation.w};
                v = blendTransform(track.path, base, v, weight);
            }
            switch (track.path) {
                case TransformTrack::Path::Translation: tr->position = {v.x, v.y, v.z}; break;
                case TransformTrack::Path::Scale: tr->scale = {v.x, v.y, v.z}; break;
                case TransformTrack::Path::Rotation: tr->rotation = {v.x, v.y, v.z, v.w}; break;
            }
        }
        m_stats.tracksApplied += static_cast<u32>(to.transformTracks.size());
    }
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
        for (const AnimatorLayer& def : rt.controller->layers) {
            Runtime::Layer layer;
            layer.def = &def;
            layer.weight = def.weight;
            layer.machine = std::make_shared<AnimatorController>();
            layer.machine->parameters = rt.controller->parameters;
            layer.machine->states = def.states;
            layer.machine->transitions = def.transitions;
            layer.machine->entry = def.entry;
            layer.rt = std::make_unique<Runtime>();
            layer.rt->controller = layer.machine;
            layer.rt->layerName = def.name;
            const AnimatorState* le = layer.machine->findState(def.entry);
            if (!le && !layer.machine->states.empty()) le = &layer.machine->states.front();
            layer.rt->valid = le != nullptr;
            if (le) enterState(scene, e, *layer.rt, le, 0.0f);
            rt.layers.push_back(std::move(layer));
        }
    } else if (!anim->clip.empty()) {
        auto c = m_library.clip(anim->clip);
        if (!c) {
            AGR_LOG_ERROR("Animation", "'{}': {}", scene.nameOf(e), c.error().message);
            return slot.get();
        }
        rt.current.clip = c.value();
        rt.current.bindings = bindTracks(scene, e, *rt.current.clip);
        for (const TransformTrack& t : rt.current.clip->transformTracks) rt.current.transformBindings.push_back(resolvePath(scene, e, t.target));
        rt.valid = true;
    }
    return slot.get();
}

void AnimationSystem::enterState(Scene& scene, ecs::Entity e, Runtime& rt, const AnimatorState* state, float fade) {
    rt.fadeInterruptible = false;
    rt.useSnapshot = false;
    if (fade > 0.0f && rt.fading) {  // interrupting a crossfade: blend from what is on screen now
        rt.interruptedA = std::move(rt.current);
        rt.interruptedB = std::move(rt.previous);
        rt.previous = {};
        rt.useSnapshot = rt.snapshotPending = true;
        rt.frozenProps.clear();
        rt.frozenXforms.clear();
        rt.fading = true;
        rt.fade = 0.0f;
        rt.fadeDuration = fade;
    } else if (fade > 0.0f && rt.current.clip) {
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
    if (state->isBlend()) {
        for (const BlendMotion& m : state->motions) {
            auto c = m_library.clip(m.clip);
            if (!c) {
                AGR_LOG_ERROR("Animation", "blend state '{}': {}", state->name, c.error().message);
                continue;
            }
            rt.current.motions.push_back(c.value());
            rt.current.thresholds.push_back(m.threshold);
            rt.current.positions.push_back(m.position);
            rt.current.motionBindings.push_back(bindTracks(scene, e, *c.value()));
            std::vector<ecs::Entity> xf;
            for (const TransformTrack& t : c.value()->transformTracks) xf.push_back(resolvePath(scene, e, t.target));
            rt.current.motionXforms.push_back(std::move(xf));
        }
        rt.current.blendParameter = state->blendParameter;
        rt.current.blendParameterY = state->blendParameterY;
        rt.current.mixWeights.assign(rt.current.motions.size(), 0.0f);
        if (!rt.current.mixWeights.empty()) rt.current.mixWeights[0] = 1.0f;
        if (!rt.current.motions.empty()) {
            rt.current.clip = rt.current.motions.front();
            rt.current.bindings = bindTracks(scene, e, *rt.current.clip);
            for (const TransformTrack& t : rt.current.clip->transformTracks) rt.current.transformBindings.push_back(resolvePath(scene, e, t.target));
        }
    } else if (!state->clip.empty()) {
        if (auto c = m_library.clip(state->clip)) {
            rt.current.clip = c.value();
            rt.current.bindings = bindTracks(scene, e, *rt.current.clip);
            for (const TransformTrack& t : rt.current.clip->transformTracks) rt.current.transformBindings.push_back(resolvePath(scene, e, t.target));
        } else {
            AGR_LOG_ERROR("Animation", "state '{}': {}", state->name, c.error().message);
        }
    }
    if (!state->spriteClip.empty())
        if (auto* sa = scene.registry().tryGet<SpriteAnimatorComponent>(e)) sa->clip = state->spriteClip;
    if (!rt.fading) rt.previous = {};
    m_events.push_back({e, AnimationEvent::Kind::StateEnter, state->name, rt.layerName});
}

void AnimationSystem::updateBlend(Scene& scene, ecs::Entity e, Runtime& rt, float step, const std::unordered_map<std::string, float>& params) {
    Runtime::Playback& p = rt.current;
    const usize n = p.motions.size();
    auto it = params.find(p.blendParameter);
    const float x = it == params.end() ? 0.0f : it->second;
    p.mixWeights.assign(n, 0.0f);
    if (!p.blendParameterY.empty()) {
        // 2D blend space: gradient band interpolation (each motion's influence fades out toward
        // every other motion), normalized. Exact on a motion; smooth and local in between.
        auto iy = params.find(p.blendParameterY);
        const math::Vec2 q{x, iy == params.end() ? 0.0f : iy->second};
        float total = 0.0f;
        for (usize i = 0; i < n; ++i) {
            float w = 1.0f;
            for (usize j = 0; j < n && w > 0.0f; ++j) {
                if (i == j) continue;
                const math::Vec2 pij = p.positions[j] - p.positions[i];
                const float len2 = pij.x * pij.x + pij.y * pij.y;
                if (len2 < 1e-12f) continue;
                const math::Vec2 piq = q - p.positions[i];
                w = std::min(w, std::clamp(1.0f - (piq.x * pij.x + piq.y * pij.y) / len2, 0.0f, 1.0f));
            }
            p.mixWeights[i] = w;
            total += w;
        }
        if (total <= 1e-6f) {  // outside every band: the nearest motion
            usize best = 0;
            float bestD = 1e30f;
            for (usize i = 0; i < n; ++i) {
                const math::Vec2 d = q - p.positions[i];
                if (d.x * d.x + d.y * d.y < bestD) bestD = d.x * d.x + d.y * d.y, best = i;
            }
            p.mixWeights.assign(n, 0.0f);
            p.mixWeights[best] = 1.0f;
            total = 1.0f;
        }
        float dur = 0.0f;
        usize heaviest = 0;
        for (usize i = 0; i < n; ++i) {
            p.mixWeights[i] /= total;
            dur += p.mixWeights[i] * std::max(p.motions[i]->duration, 1e-4f);
            if (p.mixWeights[i] > p.mixWeights[heaviest]) heaviest = i;
        }
        p.phase += step * p.speed / std::max(dur, 1e-4f);
        if (p.clip != p.motions[heaviest]) {  // events and exit times follow the heaviest motion
            p.clip = p.motions[heaviest];
            p.bindings = p.motionBindings[heaviest];
            p.transformBindings = p.motionXforms[heaviest];
        }
        p.time = p.phase * std::max(p.clip->duration, 1e-4f);
        p.second = nullptr;
        p.secondWeight = 0.0f;
        return;
    }
    // Pair of motions around x and the weight of the upper one (clamped at both ends).
    usize lo = 0;
    while (lo + 1 < n && x >= p.thresholds[lo + 1]) ++lo;
    const usize hi = std::min(lo + 1, n - 1);
    float w = 0.0f;
    if (hi != lo && x > p.thresholds[lo]) w = std::clamp((x - p.thresholds[lo]) / std::max(p.thresholds[hi] - p.thresholds[lo], 1e-6f), 0.0f, 1.0f);
    const float durLo = std::max(p.motions[lo]->duration, 1e-4f), durHi = std::max(p.motions[hi]->duration, 1e-4f);
    // Phase in cycles: both clips advance by the same fraction of their own length (feet in step).
    p.phase += step * p.speed / (durLo + (durHi - durLo) * w);
    if (p.clip != p.motions[lo]) {
        p.clip = p.motions[lo];
        p.bindings = bindTracks(scene, e, *p.clip);
        p.transformBindings.clear();
        for (const TransformTrack& t : p.clip->transformTracks) p.transformBindings.push_back(resolvePath(scene, e, t.target));
    }
    p.time = p.phase * durLo;
    p.mixWeights[lo] = 1.0f - w;
    p.mixWeights[hi] += w;
    p.second = hi != lo && w > 0.0f ? p.motions[hi] : nullptr;
    p.secondTime = p.phase * durHi;
    p.secondWeight = p.second ? w : 0.0f;
}

void AnimationSystem::stepMachine(Scene& scene, ecs::Entity e, Runtime& rt, float step, std::unordered_map<std::string, float>& params) {
    Runtime::Playback& cur = rt.current;
    const float t0 = cur.time;
    if (!cur.motions.empty()) updateBlend(scene, e, rt, step, params);
    else cur.time += step * cur.speed;
    rt.stateTime += step;
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
                if ((at > lo || (cur.fresh && at >= lo)) && at <= hi) m_events.push_back({e, AnimationEvent::Kind::ClipEvent, ev.name, rt.layerName});
            }
    }
    cur.fresh = false;
    if (rt.fading) {
        rt.previous.time += step * rt.previous.speed;
        rt.previous.secondTime += step * rt.previous.speed;
        if (rt.previous.clip) rt.previous.phase += step * rt.previous.speed / std::max(rt.previous.clip->duration, 1e-4f);
        rt.fade += step;
        if (rt.fade >= rt.fadeDuration) {
            rt.fading = false;
            rt.previous = {};
            rt.useSnapshot = false;
            rt.frozenProps.clear();
            rt.frozenXforms.clear();
        }
    }
    // Transitions (one per frame; while crossfading only when the running one is interruptible).
    if (rt.controller && (!rt.fading || rt.fadeInterruptible)) {
        for (const AnimatorTransition& tr : rt.controller->transitions) {
            if (tr.from != "*" && tr.from != cur.state) continue;
            if (tr.from == "*" && tr.to == cur.state) continue;
            if (tr.exitTime >= 0.0f) {
                const float d = cur.clip ? cur.clip->duration : 0.0f;
                const float normalized = d > 0.0f ? cur.time / d : 1.0f;
                if (normalized < tr.exitTime) continue;
            }
            bool ok = true;
            for (const TransitionCondition& c : tr.conditions) {
                auto p = params.find(c.parameter);
                const float v = p == params.end() ? 0.0f : p->second;
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
            const AnimatorState* target = rt.controller->findState(tr.to);
            if (!target) continue;
            for (const TransitionCondition& c : tr.conditions) {
                const AnimatorParameter* p = rt.controller->findParameter(c.parameter);
                if (p && p->type == ParameterType::Trigger) params[c.parameter] = 0.0f;  // consumed
            }
            if (rt.fading) ++m_stats.interruptions;
            enterState(scene, e, rt, target, tr.duration);
            rt.fadeInterruptible = tr.interruptible;
            ++m_stats.transitions;
            break;
        }
    }
}

template <typename Sample>
void AnimationSystem::applyAdditive(Scene&, ecs::Entity, Runtime& rt, const AnimatorLayer*, float weight, float fade, Sample&& sample) {
    using Pose = PoseScratch::Pose;
    Pose &cur = m_pose->a, &curRef = m_pose->b, &prev = m_pose->c, &prevRef = m_pose->d;
    // Deltas from the first frame: values for properties and positions, rotations as ref^-1 * q.
    auto toDelta = [](Pose& pose, const Pose& ref) {
        for (auto& p : pose.props) {
            const auto* r = ref.findProp(p.comp, p.field, p.channel);
            p.sum = p.weight > 0.0f && r && r->weight > 0.0f ? p.sum - r->sum : 0.0f;
        }
        for (auto& x : pose.xforms) {
            const auto* r = ref.findXform(x.tr, x.path);
            if (x.path == TransformTrack::Path::Rotation) {
                const math::Quat q{x.sum.x, x.sum.y, x.sum.z, x.sum.w};
                const math::Quat rq = r ? math::Quat{r->sum.x, r->sum.y, r->sum.z, r->sum.w} : q;
                const math::Quat d = math::normalize(rq.conjugate() * q);
                x.sum = {d.x, d.y, d.z, d.w};
            } else {
                x.sum = r ? x.sum - r->sum : math::Vec4(0, 0, 0, 0);
            }
        }
    };
    sample(rt.current, cur, false);
    sample(rt.current, curRef, true);
    toDelta(cur, curRef);
    auto identityDelta = [](TransformTrack::Path path) { return path == TransformTrack::Path::Rotation ? math::Vec4(0, 0, 0, 1) : math::Vec4(0, 0, 0, 0); };
    if (fade < 1.0f) {
        sample(rt.previous, prev, false);
        sample(rt.previous, prevRef, true);
        toDelta(prev, prevRef);
        for (auto& p : cur.props) {
            const auto* o = prev.findProp(p.comp, p.field, p.channel);
            const float from = o ? o->sum : 0.0f;
            p.sum = from + (p.sum - from) * fade;
        }
        for (auto& x : cur.xforms) {
            const auto* o = prev.findXform(x.tr, x.path);
            x.sum = blendTransform(x.path, o ? o->sum : identityDelta(x.path), x.sum, fade);
        }
        for (const auto& o : prev.props)
            if (!cur.findProp(o.comp, o.field, o.channel)) {
                auto& p = cur.prop(o.comp, o.field, o.channel);
                p.sum = o.sum * (1.0f - fade);
                p.weight = 1.0f;
            }
        for (const auto& o : prev.xforms)
            if (!cur.findXform(o.tr, o.path)) {
                auto& x = cur.xform(o.tr, o.path);
                x.sum = blendTransform(o.path, o.sum, identityDelta(o.path), fade);
                x.weight = 1.0f;
            }
    }
    // Added on top of the current values, scaled by the layer weight.
    std::vector<FieldWrite> writes;
    for (const auto& p : cur.props) {
        if (p.field->type == FieldType::String || p.sum == 0.0f) continue;
        const auto base = readChannel(p.comp, *p.field, p.channel);
        if (!base) continue;
        auto w = std::find_if(writes.begin(), writes.end(), [&](const FieldWrite& fw) { return fw.component == p.comp && fw.field == p.field; });
        if (w == writes.end()) {
            writes.push_back({p.comp, p.field, {0, 0, 0, 0}, 0, nullptr});
            w = writes.end() - 1;
        }
        w->v[p.channel] = *base + p.sum * weight * p.mask;
        w->mask |= static_cast<u8>(1u << p.channel);
    }
    for (FieldWrite& w : writes) writeField(w);
    for (const auto& x : cur.xforms) {
        TransformComponent& tr = *x.tr;
        switch (x.path) {
            case TransformTrack::Path::Translation: tr.position = tr.position + math::Vec3(x.sum.x, x.sum.y, x.sum.z) * weight * x.mask; break;
            case TransformTrack::Path::Scale: tr.scale = tr.scale + math::Vec3(x.sum.x, x.sum.y, x.sum.z) * weight * x.mask; break;
            case TransformTrack::Path::Rotation:
                tr.rotation = math::normalize(tr.rotation * math::slerp(math::Quat::identity(), math::Quat{x.sum.x, x.sum.y, x.sum.z, x.sum.w}, weight * x.mask));
                break;
        }
    }
    m_stats.tracksApplied += static_cast<u32>(cur.props.size() + cur.xforms.size());
}

void AnimationSystem::applyLayer(Scene& scene, ecs::Entity e, Runtime& rt, const AnimatorLayer* layer, float weight) {
    using Pose = PoseScratch::Pose;
    Pose& cur = m_pose->a;
    Pose& prev = m_pose->b;
    // `reference`: the first frame of every clip (additive layers add the change from it).
    auto sample = [&](Runtime::Playback& pb, Pose& pose, bool reference) {
        pose.clear();
        if (!pb.motions.empty()) {
            for (usize i = 0; i < pb.motions.size(); ++i) {
                const float w = i < pb.mixWeights.size() ? pb.mixWeights[i] : 0.0f;
                addClipToPose(scene, e, *pb.motions[i], pb.motionBindings[i], pb.motionXforms[i],
                              reference ? 0.0f : pb.phase * std::max(pb.motions[i]->duration, 1e-4f), w, layer, pose);
            }
        } else if (pb.clip) {
            addClipToPose(scene, e, *pb.clip, pb.bindings, pb.transformBindings, reference ? 0.0f : pb.time, 1.0f, layer, pose);
        }
        pose.finish();
    };
    const float fade = rt.fading && rt.fadeDuration > 0.0f ? std::clamp(rt.fade / rt.fadeDuration, 0.0f, 1.0f) : 1.0f;
    if (layer && layer->additive) {
        applyAdditive(scene, e, rt, layer, weight, fade, sample);
        return;
    }
    sample(rt.current, cur, false);
    if (fade < 1.0f) {
        // Crossfade: from the previous state's values (or the current value where it had none).
        if (rt.useSnapshot) {
            // Values on screen when the crossfade was interrupted (captured once, then fixed).
            auto capture = [&](const Pose& pose) {
                for (const auto& p : pose.props) {
                    const u64 k = Pose::key(p.comp, p.field, p.channel);
                    if (!rt.frozenProps.count(k))
                        if (auto v = readChannel(p.comp, *p.field, p.channel)) rt.frozenProps[k] = *v;
                }
                for (const auto& x : pose.xforms) {
                    const u64 k = Pose::key(x.tr, nullptr, static_cast<i32>(x.path));
                    if (!rt.frozenXforms.count(k)) rt.frozenXforms[k] = currentXform(*x.tr, x.path);
                }
            };
            if (rt.snapshotPending) {
                for (Runtime::Playback* pb : {&rt.interruptedA, &rt.interruptedB})
                    if (pb->clip || !pb->motions.empty()) {
                        sample(*pb, prev, false);
                        capture(prev);
                    }
                rt.interruptedA = {};
                rt.interruptedB = {};
                rt.snapshotPending = false;
            }
            capture(cur);  // targets new to this state start from their current value
            prev.clear();
            for (const auto& p : cur.props)
                if (auto it = rt.frozenProps.find(Pose::key(p.comp, p.field, p.channel)); it != rt.frozenProps.end()) {
                    auto& q = prev.prop(p.comp, p.field, p.channel);
                    q.sum = it->second;
                    q.weight = 1.0f;
                }
            for (const auto& x : cur.xforms)
                if (auto it = rt.frozenXforms.find(Pose::key(x.tr, nullptr, static_cast<i32>(x.path))); it != rt.frozenXforms.end()) {
                    auto& q = prev.xform(x.tr, x.path);
                    q.sum = it->second;
                    q.weight = 1.0f;
                }
        } else {
            sample(rt.previous, prev, false);
        }
        for (auto& p : cur.props) {
            if (!p.weight) continue;
            const auto* o = prev.findProp(p.comp, p.field, p.channel);
            const std::optional<float> base = o && o->weight > 0.0f ? std::optional<float>(o->sum) : readChannel(p.comp, *p.field, p.channel);
            if (base) p.sum = *base + (p.sum - *base) * fade;
            if (p.text && fade < 0.5f && o && o->text) p.text = o->text;
        }
        for (auto& x : cur.xforms) {
            const auto* o = prev.findXform(x.tr, x.path);
            x.sum = blendTransform(x.path, o ? o->sum : currentXform(*x.tr, x.path), x.sum, fade);
        }
        for (const auto& o : prev.props)  // animated only by the previous state: back to the current value
            if (o.weight > 0.0f && !cur.findProp(o.comp, o.field, o.channel)) {
                auto& p = cur.prop(o.comp, o.field, o.channel);
                const std::optional<float> now = readChannel(o.comp, *o.field, o.channel);
                p.sum = now ? o.sum + (*now - o.sum) * fade : o.sum;
                p.weight = 1.0f;
            }
        for (const auto& o : prev.xforms)
            if (!cur.findXform(o.tr, o.path)) {
                auto& x = cur.xform(o.tr, o.path);
                x.sum = blendTransform(o.path, o.sum, currentXform(*o.tr, o.path), fade);
                x.weight = 1.0f;
            }
    }
    // Write over the values below (lower layers) with the layer's weight, one write per field.
    std::vector<FieldWrite> writes;
    for (const auto& p : cur.props) {
        if (p.weight <= 0.0f && !p.text) continue;
        auto w = std::find_if(writes.begin(), writes.end(), [&](const FieldWrite& fw) { return fw.component == p.comp && fw.field == p.field; });
        if (w == writes.end()) {
            writes.push_back({p.comp, p.field, {0, 0, 0, 0}, 0, nullptr});
            w = writes.end() - 1;
        }
        if (p.field->type == FieldType::String) {
            if (weight >= 0.5f) w->text = p.text;
            continue;
        }
        float v = p.sum;
        const float pw = weight * p.mask;
        if (pw < 1.0f)
            if (const auto base = readChannel(p.comp, *p.field, p.channel)) v = *base + (v - *base) * pw;
        w->v[p.channel] = v;
        w->mask |= static_cast<u8>(1u << p.channel);
    }
    for (FieldWrite& w : writes) writeField(w);
    for (const auto& x : cur.xforms) {
        if (x.weight <= 0.0f) continue;
        const float xw = weight * x.mask;
        setXform(*x.tr, x.path, xw < 1.0f ? blendTransform(x.path, currentXform(*x.tr, x.path), x.sum, xw) : x.sum);
    }
    m_stats.tracksApplied += static_cast<u32>(cur.props.size() + cur.xforms.size());
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
        // Root motion: where the playing clips were before this step.
        const AnimationClip* curBefore = rt->current.clip.get();
        const AnimationClip* prevBefore = rt->previous.clip.get();
        const float curT0 = rt->current.time, prevT0 = rt->previous.time;
        const float curPhase0 = rt->current.phase;
        const std::string curState0 = rt->current.state;
        stepMachine(scene, e, *rt, step, rt->params);
        for (Runtime::Layer& l : rt->layers)
            if (l.rt && l.rt->valid) stepMachine(scene, e, *l.rt, step, rt->params);
        auto isMix = [](const Runtime::Playback& pb) { return !pb.motions.empty(); };
        // Root motion: moves the entity by `delta` (bone parent space) and pins the bone at `start`.
        auto applyRoot = [&](math::Vec3 delta, math::Vec3 start) {
            const ecs::Entity bone = resolvePath(scene, e, anim.rootMotion);
            if (bone.isNull() || bone == e) return;
            auto& bt = scene.registry().get<TransformComponent>(bone);
            bt.position.x = start.x;
            bt.position.z = start.z;
            if (anim.rootMotionY) bt.position.y = start.y;
            else delta.y = 0.0f;
            const ecs::Entity boneParent = scene.parent(bone);
            const math::Vec3 world = scene.worldMatrix(boneParent.isNull() ? e : boneParent).transformVector(delta);
            anim.rootMotionDelta = world;
            if (anim.applyRootMotion) {
                auto& et = scene.registry().get<TransformComponent>(e);
                const ecs::Entity p = scene.parent(e);
                et.position = et.position + (p.isNull() ? world : math::inverse(scene.worldMatrix(p)).transformVector(world));
            }
        };
        if (!rt->layers.empty() || isMix(rt->current) || (rt->fading && isMix(rt->previous))) {
            // Pose path: blend states (every motion, property and transform tracks), layers with masks.
            applyLayer(scene, e, *rt, nullptr, 1.0f);
            for (Runtime::Layer& l : rt->layers)
                if (l.rt && l.rt->valid && l.weight > 0.0f) applyLayer(scene, e, *l.rt, l.def, l.weight);
            // Root motion of a blend state: every motion's movement over the same phase step, by weight.
            Runtime::Playback& pb = rt->current;
            if (!anim.rootMotion.empty() && isMix(pb) && pb.state == curState0) {
                const ecs::Entity bone = resolvePath(scene, e, anim.rootMotion);
                if (!bone.isNull()) {
                    const math::Vec3 here = scene.registry().get<TransformComponent>(bone).position;
                    math::Vec3 delta{0.0f}, start{0.0f};
                    float total = 0.0f;
                    for (usize i = 0; i < pb.motions.size() && i < pb.mixWeights.size(); ++i) {
                        const float w = pb.mixWeights[i];
                        const RootSource src = rootSource(pb.motions[i].get(), anim.rootMotion);
                        if (w <= 0.0f || !src) continue;
                        const AnimationClip& c = *pb.motions[i];
                        const float d = std::max(c.duration, 1e-4f);
                        delta = delta + (rootDisplacement(c, src, pb.phase * d) - rootDisplacement(c, src, curPhase0 * d)) * w;
                        start = start + src.at(0.0f, here) * w;
                        total += w;
                    }
                    const float fade = rt->fading && rt->fadeDuration > 0.0f ? std::clamp(rt->fade / rt->fadeDuration, 0.0f, 1.0f) : 1.0f;
                    if (total > 0.0f) applyRoot(delta * fade, start / total);
                }
            }
            continue;
        }
        Runtime::Playback& now = rt->current;
        if (now.clip) {
            const float w = rt->fading && rt->fadeDuration > 0.0f ? std::clamp(rt->fade / rt->fadeDuration, 0.0f, 1.0f) : 1.0f;
            apply(scene, e, rt, rt->fading ? rt->previous.clip.get() : nullptr, rt->previous.time, *now.clip, now.time, w);
            if (!anim.rootMotion.empty()) {
                // Movement of the root track over this step, for the current clip and the one fading out.
                auto movement = [&](const AnimationClip* clip, float t1, const AnimationClip* before, float t0) {
                    const RootSource src = rootSource(clip, anim.rootMotion);
                    if (!src) return math::Vec3{0.0f};
                    return rootDisplacement(*clip, src, t1) - (clip == before ? rootDisplacement(*clip, src, t0) : math::Vec3{0.0f});
                };
                math::Vec3 delta = movement(now.clip.get(), now.time, curBefore, curT0) * w;
                if (rt->fading && rt->previous.clip) {
                    // The clip fading out was the current one before a transition made this frame.
                    const AnimationClip* pc = rt->previous.clip.get();
                    const float pt0 = pc == curBefore ? curT0 : prevT0;
                    delta = delta + movement(pc, rt->previous.time, pc == curBefore || pc == prevBefore ? pc : nullptr, pt0) * (1.0f - w);
                }
                const ecs::Entity bone = resolvePath(scene, e, anim.rootMotion);
                const RootSource src = rootSource(now.clip.get(), anim.rootMotion);
                if (!bone.isNull() && bone != e && src)  // the bone stays where the clip starts
                    applyRoot(delta, src.at(0.0f, scene.registry().get<TransformComponent>(bone).position));
            }
        }
    }
    m_stats.ik2DChains = solveIK2D(scene);  // after the clips: IK overrides the animated pose
    m_stats.ik3DChains = solveIK3D(scene);
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

bool AnimationSystem::play(Scene& scene, ecs::Entity e, std::string_view state, float fade, std::string_view layer) {
    Runtime* rt = runtime(scene, e);
    if (!rt || !rt->controller) return false;
    Runtime* target = rt;
    if (!layer.empty()) {
        target = nullptr;
        for (Runtime::Layer& l : rt->layers)
            if (l.def && l.def->name == layer) target = l.rt.get();
        if (!target) return false;
    }
    const AnimatorState* s = target->controller->findState(state);
    if (!s) return false;
    enterState(scene, e, *target, s, fade);
    target->valid = true;
    return true;
}

std::string AnimationSystem::currentState(ecs::Entity e, std::string_view layer) const {
    auto it = m_runtimes.find(e.packed());
    if (it == m_runtimes.end()) return {};
    if (layer.empty()) return it->second->current.state;
    for (const Runtime::Layer& l : it->second->layers)
        if (l.def && l.def->name == layer && l.rt) return l.rt->current.state;
    return {};
}

bool AnimationSystem::setLayerWeight(Scene& scene, ecs::Entity e, std::string_view layer, float weight) {
    Runtime* rt = runtime(scene, e);
    if (!rt) return false;
    for (Runtime::Layer& l : rt->layers)
        if (l.def && l.def->name == layer) {
            l.weight = std::clamp(weight, 0.0f, 1.0f);
            return true;
        }
    return false;
}

std::optional<float> AnimationSystem::layerWeight(Scene& scene, ecs::Entity e, std::string_view layer) {
    Runtime* rt = runtime(scene, e);
    if (!rt) return std::nullopt;
    for (const Runtime::Layer& l : rt->layers)
        if (l.def && l.def->name == layer) return l.weight;
    return std::nullopt;
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
    registerIK2DComponents();
    registerIK3DComponents();
    auto& reg = ComponentRegistry::instance();
    if (reg.find<AnimatorComponent>()) return;
    reg.add<AnimatorComponent>("Animator")
        .category("Animation")
        .description("Animates fields of the entity and its children with clips (.agranim) and a state machine (.agrctrl).")
        .field("controller", &AnimatorComponent::controller, {.tooltip = "State machine (.agrctrl)", .flags = FieldFlag_AssetPath})
        .field("clip", &AnimatorComponent::clip, {.tooltip = "Clip played alone when there is no state machine (.agranim)", .flags = FieldFlag_AssetPath})
        .field("speed", &AnimatorComponent::speed, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("playing", &AnimatorComponent::playing)
        .field("rootMotion", &AnimatorComponent::rootMotion, {.tooltip = "Bone whose movement in the clips moves this entity (\"Armature/Hips\"); empty = off"})
        .field("rootMotionY", &AnimatorComponent::rootMotionY, {.tooltip = "Also the vertical movement (jumps, climbs)"})
        .field("applyRootMotion", &AnimatorComponent::applyRootMotion, {.tooltip = "Move the Transform; off: read rootMotionDelta in a script (character controller)"})
        .field("rootMotionDelta", &AnimatorComponent::rootMotionDelta, {.tooltip = "World movement of the last frame (read-only)"});
}

} // namespace agr
