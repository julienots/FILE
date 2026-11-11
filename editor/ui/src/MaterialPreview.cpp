#include <imgui.h>

#include "agr/core/Log.hpp"
#include "agr/editor/ui/Panels.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr::editor {

void MaterialPreview::render(EditorShared& s, const std::string& material) {
    rhi::Device* device = s.device();
    if (material.empty() || !device || !s.assets()) return;
    if (!m_renderer) {
        m_renderer = std::make_unique<Renderer>(*device, *s.assets());
        if (Status st = m_renderer->initialize(); !st) {
            AGR_LOG_ERROR("Editor", "material preview renderer: {}", st.error().message);
            m_renderer.reset();
            return;
        }
        m_renderer->setModels(s.models());
        m_renderer->setMaterials(s.materials());
        m_renderer->setShaders(s.shaders());
    }
    Renderer* renderer = m_renderer.get();
    renderer->setTime(ImGui::GetTime());  // animated shaders (TIME) move in the preview too
    if (!m_scene) {
        registerRenderComponents();
        m_scene = std::make_unique<Scene>("Material Preview");
        auto& reg = m_scene->registry();
        const auto cam = m_scene->createEntity("Camera");
        auto& c = reg.emplace<CameraComponent>(cam);
        c.fieldOfView = math::radians(32.0f);
        c.clearColor = {0.11f, 0.12f, 0.14f, 1.0f};
        reg.get<TransformComponent>(cam).position = {0.0f, 0.0f, 2.2f};
        const auto sun = m_scene->createEntity("Sun");
        auto& l = reg.emplace<DirectionalLightComponent>(sun);
        l.intensity = 2.2f;
        l.castShadows = false;
        reg.get<TransformComponent>(sun).rotation = math::Quat::fromEuler({math::radians(-30.0f), math::radians(-35.0f), 0.0f});
        auto& env = reg.emplace<EnvironmentComponent>(sun);
        env.sky = SkyMode::Gradient;
        env.ambientIntensity = 0.8f;
        env.sunDisc = false;
        const auto fill = m_scene->createEntity("Fill");  // rim light from behind-left: shows roughness on the edge
        auto& p = reg.emplace<PointLightComponent>(fill);
        p.color = {0.7f, 0.8f, 1.0f, 1.0f};
        p.intensity = 6.0f;
        p.range = 8.0f;
        reg.get<TransformComponent>(fill).position = {-1.6f, 1.0f, -1.2f};
        m_sphere = m_scene->createEntity("Sphere");
        reg.emplace<MeshRendererComponent>(m_sphere).mesh = PrimitiveMesh::Sphere;
    }
    m_scene->registry().get<MeshRendererComponent>(m_sphere).material = material;
    if (!m_target) {
        auto rt = device->createRenderTarget({.width = kSize, .height = kSize, .depth = true, .debugName = "agr.editor.material"});
        if (!rt) {
            AGR_LOG_ERROR("Editor", "material preview target: {}", rt.error().message);
            return;
        }
        m_target = rt.value();
    }
    m_scene->updateTransforms();
    auto view = RenderView::fromPrimaryCamera(*m_scene);
    if (!view) return;
    view->direct = true;
    renderer->renderView(*m_scene, *view, m_target, kSize, kSize);
    device->prepareTextureForSampling(device->renderTargetTexture(m_target));
    m_rendered = true;
}

rhi::TextureHandle MaterialPreview::texture(EditorShared& s) const {
    if (!m_rendered || !s.device()) return {};
    return s.device()->renderTargetTexture(m_target);
}

void MaterialPreview::release(EditorShared& s) {
    if (s.device() && m_target) s.device()->destroyRenderTarget(m_target);
    m_target = {};
    m_rendered = false;
    m_scene.reset();
    m_renderer.reset();
}

} // namespace agr::editor
