// Spin plugin — demonstrates the AGR plugin API end to end:
//   * registers a reflected component (visible to the serializer / inspector),
//   * runs a system over the active scene every frame,
//   * cleans everything up on unload (mandatory for dynamic plugins).
#include "agr/runtime/Engine.hpp"
#include "agr/runtime/Plugin.hpp"
#include "agr/scene/Components.hpp"

namespace {

struct SpinComponent {
    agr::math::Vec3 axis{0.0f, 1.0f, 0.0f};
    float speed = agr::math::radians(90.0f);  ///< radians per second
};

class SpinPlugin final : public agr::Plugin {
public:
    agr::PluginInfo info() const override {
        return {"Spin", "1.0.0", "Rotates entities that own a Spin component.", "Superessence Studio", {}};
    }

    agr::Status onLoad(agr::Engine& engine) override {
        (void)engine;
        agr::ComponentRegistry::instance()
            .add<SpinComponent>("Spin")
            .category("Gameplay")
            .description("Continuously rotates the entity around an axis.")
            .field("axis", &SpinComponent::axis)
            .field("speed", &SpinComponent::speed, {.flags = agr::FieldFlag_Angle, .speed = 0.01f});
        AGR_LOG_INFO("Spin", "Spin component registered");
        return {};
    }

    void onUpdate(agr::Engine& engine, const agr::FrameTime& time) override {
        const float dt = static_cast<float>(time.deltaSeconds);
        engine.scene().registry().view<SpinComponent, agr::TransformComponent>().each(
            [dt](agr::ecs::Entity, SpinComponent& spin, agr::TransformComponent& t) {
                const auto delta = agr::math::Quat::fromAxisAngle(spin.axis, spin.speed * dt);
                t.rotation = agr::math::normalize(delta * t.rotation);
            });
    }

    void onUnload(agr::Engine& engine) override {
        const auto id = agr::ecs::componentTypeId<SpinComponent>();
        engine.scene().registry().erasePool(id);
        agr::ComponentRegistry::instance().remove(id);
    }
};

} // namespace

AGR_DEFINE_PLUGIN(SpinPlugin)
