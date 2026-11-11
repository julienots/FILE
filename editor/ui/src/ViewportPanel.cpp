#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_internal.h>

#include "agr/editor/Picking.hpp"
#include "agr/editor/ui/ImGuiRenderer.hpp"
#include "agr/editor/ui/Panels.hpp"
#include "agr/navigation/NavigationPlugin.hpp"
#include "agr/render/ModelLibrary.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/physics2d/Physics2DComponents.hpp"
#include "agr/physics3d/Physics3DComponents.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/twod/TilemapEditing.hpp"
#include "agr/ui/UIComponents.hpp"

namespace agr::editor {

using math::Mat4;
using math::Vec3;
using math::Vec4;

namespace {

/// World -> screen for overlays. Returns false behind the camera.
bool project(const Mat4& viewProj, Vec3 p, ImVec2 origin, ImVec2 size, ImVec2& out) {
    const Vec4 c = viewProj * Vec4(p, 1.0f);
    if (c.w <= 1e-4f) return false;
    out = {origin.x + (c.x / c.w * 0.5f + 0.5f) * size.x, origin.y + (0.5f - c.y / c.w * 0.5f) * size.y};
    return true;
}

math::Ray rayFromScreen(const Mat4& viewProjGL, ImVec2 uv) {
    const Mat4 inv = math::inverse(viewProjGL);
    const float nx = uv.x * 2.0f - 1.0f, ny = 1.0f - uv.y * 2.0f;
    const Vec3 a = inv.transformPoint({nx, ny, -1.0f});
    const Vec3 b = inv.transformPoint({nx, ny, 1.0f});
    return {a, math::normalize(b - a)};
}

/// Editor grid drawn with ImDrawList: spacing adapts to the zoom level, lines fade
/// with the distance to the focus point and are clipped at the near plane (no
/// aliasing band at the horizon).
void drawGrid(ImDrawList* dl, const Mat4& view, const Mat4& proj, ImVec2 origin, ImVec2 size, bool plane2D, Vec3 focus, float zoom) {
    const float spacing = std::pow(10.0f, std::floor(std::log10(std::max(zoom, 0.5f) / 3.0f)));
    const int cells = 40;
    const float extent = spacing * static_cast<float>(cells);
    const float snap = spacing * 10.0f;
    const float cx = std::round((plane2D ? focus.x : focus.x) / snap) * snap;
    const float cy = std::round((plane2D ? focus.y : focus.z) / snap) * snap;
    auto toWorld = [&](float u, float v) { return plane2D ? Vec3{u, v, 0.0f} : Vec3{u, 0.0f, v}; };
    auto line = [&](Vec3 a, Vec3 b, ImU32 color, float baseAlpha, float thickness) {
        constexpr int kSteps = 10;
        for (int i = 0; i < kSteps; ++i) {
            Vec3 p0 = a + (b - a) * (static_cast<float>(i) / kSteps), p1 = a + (b - a) * (static_cast<float>(i + 1) / kSteps);
            const Vec3 mid = (p0 + p1) * 0.5f;
            const float d = math::length(Vec3{mid.x - (plane2D ? focus.x : focus.x), plane2D ? mid.y - focus.y : 0.0f,
                                              plane2D ? 0.0f : mid.z - focus.z}) / extent;
            const float alpha = baseAlpha * std::clamp(1.0f - d, 0.0f, 1.0f) * std::clamp(1.0f - d, 0.0f, 1.0f);
            if (alpha < 0.01f) continue;
            // Clip against the near plane in view space.
            Vec3 v0 = view.transformPoint(p0), v1 = view.transformPoint(p1);
            const float nearZ = -0.05f;
            if (v0.z > nearZ && v1.z > nearZ) continue;
            if (v0.z > nearZ) v0 = v0 + (v1 - v0) * ((nearZ - v0.z) / (v1.z - v0.z));
            if (v1.z > nearZ) v1 = v1 + (v0 - v1) * ((nearZ - v1.z) / (v0.z - v1.z));
            auto toScreen = [&](Vec3 v) {
                const Vec4 c = proj * Vec4(v, 1.0f);
                return ImVec2{origin.x + (c.x / c.w * 0.5f + 0.5f) * size.x, origin.y + (0.5f - c.y / c.w * 0.5f) * size.y};
            };
            const ImU32 col = (color & 0x00FFFFFFu) | (static_cast<ImU32>(alpha * 255.0f) << 24);
            dl->AddLine(toScreen(v0), toScreen(v1), col, thickness);
        }
    };
    for (int i = -cells; i <= cells; ++i) {
        const float o = static_cast<float>(i) * spacing;
        const bool major = (static_cast<int>(std::round((cx + o) / spacing)) % 10) == 0;
        const float along = cx + o, across = cy + o;
        const bool axisU = std::fabs(across) < spacing * 0.5f;  // line on the U axis (v = 0)
        const bool axisV = std::fabs(along) < spacing * 0.5f;   // line on the V axis (u = 0)
        line(toWorld(cx - extent, across), toWorld(cx + extent, across), axisU ? palette::kAxisX : IM_COL32(255, 255, 255, 255),
             axisU ? 0.8f : (major ? 0.22f : 0.09f), axisU ? 1.5f : 1.0f);
        line(toWorld(along, cy - extent), toWorld(along, cy + extent), axisV ? (plane2D ? palette::kAxisY : palette::kAxisZ) : IM_COL32(255, 255, 255, 255),
             axisV ? 0.8f : (major ? 0.22f : 0.09f), axisV ? 1.5f : 1.0f);
    }
}

struct DevicePreset {
    const char* name;
    float width, height;
    math::Vec4 insets;  ///< left, top, right, bottom (device pixels): notch, gesture bar
};
constexpr DevicePreset kDevices[] = {
    {"Libre", 0, 0, {0, 0, 0, 0}},
    {"Téléphone 20:9 paysage", 2400, 1080, {110, 0, 0, 50}},
    {"Téléphone 20:9 portrait", 1080, 2400, {0, 110, 0, 50}},
    {"Téléphone 16:9 paysage", 1920, 1080, {0, 0, 0, 0}},
    {"Tablette 4:3 paysage", 2048, 1536, {0, 0, 0, 40}},
    {"Tablette 4:3 portrait", 1536, 2048, {0, 0, 0, 40}},
};
constexpr int kDeviceCount = static_cast<int>(sizeof(kDevices) / sizeof(kDevices[0]));

bool toolButton(const char* label, bool active, const char* tooltip) {
    if (active) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(245, 165, 36, 90));
    const bool clicked = ImGui::SmallButton(label);
    if (active) ImGui::PopStyleColor();
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tooltip);
    ImGui::SameLine(0, 3);
    return clicked;
}

} // namespace

rhi::TextureHandle ViewportPanel::texture(EditorShared& s) const {
    return s.device() ? s.device()->renderTargetTexture(m_target) : rhi::TextureHandle{};
}

bool ViewportPanel::worldToScreen(EditorShared& s, Vec3 world, ImVec2& out) const {
    if (m_lastSize.x <= 0.0f || m_lastSize.y <= 0.0f) return false;
    const float aspect = m_lastSize.x / m_lastSize.y;
    return project(projectionGL(s, aspect) * viewMatrix(s), world, m_lastOrigin, m_lastSize, out);
}

void ViewportPanel::beginFrame(EditorShared& s) {
    rhi::Device* d = s.device();
    if (!d || m_wantWidth == 0 || m_wantHeight == 0) return;
    if (m_target && m_wantWidth == m_width && m_wantHeight == m_height) return;
    d->destroyRenderTarget(m_target);
    auto rt = d->createRenderTarget({.width = m_wantWidth, .height = m_wantHeight, .colorFormat = rhi::TextureFormat::RGBA8, .depth = true, .samples = 1, .debugName = "agr.editor.viewport"});
    if (!rt) {
        AGR_LOG_ERROR("Editor", "viewport target: {}", rt.error().message);
        m_target = {};
        return;
    }
    m_target = rt.value();
    m_width = m_wantWidth;
    m_height = m_wantHeight;
}

void ViewportPanel::release(EditorShared& s) {
    if (s.device()) s.device()->destroyRenderTarget(m_target);
    m_target = {};
}

Mat4 ViewportPanel::viewMatrix(EditorShared& s) const {
    if (gameView)
        if (auto v = RenderView::fromPrimaryCamera(s.ctx.scene())) return v->view;
    return s.ctx.camera().renderView().view;
}

Mat4 ViewportPanel::projectionGL(EditorShared& s, float aspect) const {
    if (gameView)
        if (auto v = RenderView::fromPrimaryCamera(s.ctx.scene())) return v->projectionMatrix(aspect, true);
    return s.ctx.camera().renderView().projectionMatrix(aspect, true);
}

void ViewportPanel::renderScene(EditorShared& s) {
    Renderer* r = s.renderer();
    if (!r || !m_visible || !m_target) return;
    std::optional<RenderView> view;
    if (gameView) view = RenderView::fromPrimaryCamera(s.ctx.scene());
    else view = s.ctx.camera().renderView();
    if (view) view->overlay = gameView;  // the game UI shows in the game view only
    if (!view) {  // game view without camera: clear to a neutral color
        RenderView empty;
        empty.clearColor = {0.03f, 0.03f, 0.035f, 1.0f};
        empty.farPlane = 0.2f;  // nothing visible
        view = empty;
    }
    r->renderView(s.ctx.scene(), *view, m_target, m_width, m_height);
}

void ViewportPanel::handleCamera(EditorShared& s, const FrameTime& time, bool hovered) {
    if (gameView) return;
    ImGuiIO& io = ImGui::GetIO();
    EditorCamera& cam = s.ctx.camera();
    const float dt = static_cast<float>(time.unscaledDeltaSeconds);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) m_flying = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) m_flying = false;

    if (cam.mode2D) {
        if ((m_flying || ImGui::IsMouseDown(ImGuiMouseButton_Middle)) && (hovered || m_flying))
            cam.pan({io.MouseDelta.x, io.MouseDelta.y}, static_cast<float>(m_height ? m_height : 1) / io.DisplayFramebufferScale.y);
        if (hovered && io.MouseWheel != 0.0f) cam.zoom(io.MouseWheel);
        return;
    }
    if (m_flying) {
        cam.look(-io.MouseDelta.x * 0.004f, -io.MouseDelta.y * 0.004f);
        Vec3 dir{};
        // Physical keys: WASD on QWERTY, ZQSD on AZERTY.
        if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_Z)) dir.z += 1;
        if (ImGui::IsKeyDown(ImGuiKey_S)) dir.z -= 1;
        if (ImGui::IsKeyDown(ImGuiKey_D)) dir.x += 1;
        if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_Q)) dir.x -= 1;
        if (ImGui::IsKeyDown(ImGuiKey_E)) dir.y += 1;
        if (ImGui::IsKeyDown(ImGuiKey_C)) dir.y -= 1;
        if (math::lengthSquared(dir) > 0.0f) cam.move(math::normalize(dir), flySpeed * dt * (io.KeyShift ? 3.0f : 1.0f));
        if (io.MouseWheel != 0.0f) flySpeed = std::clamp(flySpeed * std::pow(1.2f, io.MouseWheel), 0.5f, 500.0f);
        return;
    }
    if (hovered || ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) cam.pan({io.MouseDelta.x, io.MouseDelta.y}, static_cast<float>(m_height) / io.DisplayFramebufferScale.y);
        if (io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left)) cam.orbit(-io.MouseDelta.x * 0.006f, -io.MouseDelta.y * 0.006f);
    }
    if (hovered && io.MouseWheel != 0.0f) cam.zoom(io.MouseWheel);
}

void ViewportPanel::drawOverlays(EditorShared& s, ImVec2 origin, ImVec2 size) {
    Scene& scene = s.ctx.scene();
    auto& reg = scene.registry();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float aspect = size.x / std::max(size.y, 1.0f);
    const Mat4 view = viewMatrix(s), proj = projectionGL(s, aspect), vp = proj * view;

    if (showGrid && !gameView) {
        const EditorCamera& cam = s.ctx.camera();
        const Vec3 focus = cam.mode2D ? Vec3{cam.position2D.x, cam.position2D.y, 0.0f} : cam.target;
        drawGrid(dl, view, proj, origin, size, cam.mode2D, focus, cam.mode2D ? cam.orthoSize * 2.0f : cam.distance);
    }

    // Selection bounds.
    for (ecs::Entity e : s.ctx.selection().entities(scene)) {
        const math::Aabb b = worldBounds(scene, e, s.pickAssets());
        if (!b.valid()) continue;
        Vec3 corners[8];
        for (int i = 0; i < 8; ++i) corners[i] = {(i & 1) ? b.max.x : b.min.x, (i & 2) ? b.max.y : b.min.y, (i & 4) ? b.max.z : b.min.z};
        ImVec2 sp[8];
        bool ok = true;
        for (int i = 0; i < 8 && ok; ++i) ok = project(vp, corners[i], origin, size, sp[i]);
        if (!ok) continue;
        static const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        const ImU32 col = e == scene.findByUuid(s.ctx.selection().primary()) ? palette::kSelect : IM_COL32(61, 139, 253, 140);
        for (auto& ed : edges) dl->AddLine(sp[ed[0]], sp[ed[1]], col, 1.5f);
    }

    if (showColliders) drawColliders(s, origin, size, vp);
    if (showNavigation) drawNavigation(s, origin, size, vp);

    // Icons for entities without geometry (cameras, lights).
    m_iconHits.clear();
    if (showIcons) {
        auto drawIcon = [&](ecs::Entity e, icons::Kind kind, bool direction) {
            const auto& w = reg.get<WorldTransformComponent>(e);
            if (!w.activeInHierarchy) return;
            ImVec2 p;
            if (!project(vp, w.matrix.translation(), origin, size, p)) return;
            const bool sel = s.ctx.selection().contains(scene.uuidOf(e));
            dl->AddCircleFilled(p, 13.0f, IM_COL32(14, 16, 19, 200));
            dl->AddCircle(p, 13.0f, sel ? palette::kSelect : palette::kGraphite700, 20, 1.5f);
            icons::draw(dl, kind, p, 15.0f, sel ? palette::kSelect : palette::kText);
            m_iconHits.emplace_back(e, p);
            if (direction) {  // light direction
                ImVec2 q;
                if (project(vp, w.matrix.translation() + math::normalize(w.matrix.transformVector({0, 0, -1})) * 1.5f, origin, size, q))
                    dl->AddLine(p, q, palette::kAmber, 2.0f);
            }
        };
        reg.view<CameraComponent, WorldTransformComponent>().each([&](ecs::Entity e, CameraComponent&, WorldTransformComponent&) {
            if (!gameView) drawIcon(e, icons::Kind::Camera, false);
        });
        reg.view<DirectionalLightComponent, WorldTransformComponent>().each(
            [&](ecs::Entity e, DirectionalLightComponent&, WorldTransformComponent&) { drawIcon(e, icons::Kind::Light, true); });
        reg.view<PointLightComponent, WorldTransformComponent>().each(
            [&](ecs::Entity e, PointLightComponent&, WorldTransformComponent&) { drawIcon(e, icons::Kind::Light, false); });
        reg.view<SpotLightComponent, WorldTransformComponent>().each(
            [&](ecs::Entity e, SpotLightComponent&, WorldTransformComponent&) { drawIcon(e, icons::Kind::Light, true); });
    }

    // Play mode frame and statistics.
    if (s.ctx.playState() != PlayState::Edit)
        dl->AddRect(origin, {origin.x + size.x, origin.y + size.y}, s.ctx.playState() == PlayState::Playing ? palette::kAmber : palette::kTextMuted, 0.0f, 3.0f);
    if (Renderer* r = s.renderer()) {
        const auto& st = r->stats();
        char text[224];
        int n = std::snprintf(text, sizeof text, "%u draws  %llu tris  %u+%u visibles  %u éliminés", st.drawCalls,
                              static_cast<unsigned long long>(st.triangles), st.meshesVisible, st.spritesVisible, st.meshesCulled + st.spritesCulled);
        if (n > 0 && (st.tilesVisible || st.particlesVisible || st.lights2D))
            std::snprintf(text + n, sizeof text - static_cast<usize>(n), "  |  %u tuiles  %u particules  %u lumières 2D", st.tilesVisible,
                          st.particlesVisible, st.lights2D);
        const ImVec2 ts = ImGui::CalcTextSize(text);
        dl->AddText({origin.x + size.x - ts.x - 10, origin.y + size.y - ts.y - 8}, IM_COL32(230, 228, 223, 150), text);
    }
}

void ViewportPanel::drawNavigation(EditorShared& s, ImVec2 origin, ImVec2 size, const Mat4& vp) {
    auto* plugin = s.ctx.engine().service<NavigationPlugin>();
    if (!plugin) return;
    NavigationSystem& nav = plugin->navigation();
    Scene& scene = s.ctx.scene();
    nav.ensureBuilt(scene);  // also in edit mode (rebuilt when static colliders change)
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto line = [&](Vec3 a, Vec3 b, ImU32 col, float t) {
        ImVec2 pa, pb;
        if (project(vp, a, origin, size, pa) && project(vp, b, origin, size, pb)) dl->AddLine(pa, pb, col, t);
    };
    const std::vector<Vec3> edges = nav.mesh().debugEdges();
    for (usize i = 0; i + 1 < edges.size(); i += 2) line(edges[i] + Vec3{0, 0.03f, 0}, edges[i + 1] + Vec3{0, 0.03f, 0}, IM_COL32(60, 210, 230, 170), 1.2f);
    const NavGrid& g = nav.grid();
    if (!g.empty() && nav.stats().gridBlocked <= 20000) {
        const float h = g.cellSize() * 0.5f;
        for (u32 y = 0; y < g.height(); ++y)
            for (u32 x = 0; x < g.width(); ++x) {
                if (!g.blocked(static_cast<i32>(x), static_cast<i32>(y))) continue;
                const math::Vec2 c = g.cellCenter(static_cast<i32>(x), static_cast<i32>(y));
                ImVec2 a, b;
                if (project(vp, {c.x - h, c.y - h, 0.0f}, origin, size, a) && project(vp, {c.x + h, c.y + h, 0.0f}, origin, size, b))
                    dl->AddRectFilled({std::min(a.x, b.x), std::min(a.y, b.y)}, {std::max(a.x, b.x), std::max(a.y, b.y)}, IM_COL32(230, 70, 60, 55));
            }
    }
    scene.registry().view<NavAgentComponent>().each([&](ecs::Entity e, NavAgentComponent&) {
        const auto path = nav.path(e);
        Vec3 prev = scene.worldMatrix(e).transformPoint({0, 0, 0});
        for (const Vec3& p : path) {
            line(prev, p, IM_COL32(250, 200, 50, 230), 2.0f);
            prev = p;
        }
    });
    const NavigationStats& st = nav.stats();
    char text[256];
    std::snprintf(text, sizeof text, "Navigation : %u polygones 3D, grille %ux%u (%u bloquées)%s%s", st.meshPolygons, g.width(), g.height(),
                  st.gridBlocked, st.lastError.empty() ? "" : " - ", st.lastError.c_str());
    dl->AddText({origin.x + 12, origin.y + size.y - 44}, st.lastError.empty() ? IM_COL32(60, 210, 230, 255) : palette::kWarning, text);
}

void ViewportPanel::drawColliders(EditorShared& s, ImVec2 origin, ImVec2 size, const Mat4& vp) {
    Scene& scene = s.ctx.scene();
    auto& reg = scene.registry();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    constexpr ImU32 kSolid = IM_COL32(80, 220, 120, 200), kTrigger = IM_COL32(245, 165, 36, 220);
    auto line = [&](const Mat4& w, Vec3 a, Vec3 b, ImU32 col) {
        ImVec2 pa, pb;
        if (project(vp, w.transformPoint(a), origin, size, pa) && project(vp, w.transformPoint(b), origin, size, pb)) dl->AddLine(pa, pb, col, 1.5f);
    };
    auto loop = [&](const Mat4& w, const std::vector<Vec3>& pts, ImU32 col) {
        for (usize i = 0; i < pts.size(); ++i) line(w, pts[i], pts[(i + 1) % pts.size()], col);
    };
    // Circle of radius r around c in the plane spanned by axes u, v (local space).
    auto circle = [&](const Mat4& w, Vec3 c, Vec3 u, Vec3 v, float r, ImU32 col, float from = 0.0f, float to = 2.0f * math::kPi, bool closed = true) {
        std::vector<Vec3> pts;
        const int n = 24;
        for (int i = 0; i <= n; ++i) {
            const float a = from + (to - from) * static_cast<float>(i) / n;
            pts.push_back(c + u * (std::cos(a) * r) + v * (std::sin(a) * r));
        }
        for (usize i = 0; i + 1 < pts.size(); ++i) line(w, pts[i], pts[i + 1], col);
        if (closed) line(w, pts.back(), pts.front(), col);
    };
    auto visible = [&](ecs::Entity e) {
        const auto* w = reg.tryGet<WorldTransformComponent>(e);
        const auto* st = reg.tryGet<StateComponent>(e);
        return w && w->activeInHierarchy && !(st && st->hiddenInEditor);
    };
    reg.view<Collider2DComponent, WorldTransformComponent>().each([&](ecs::Entity e, Collider2DComponent& c, WorldTransformComponent& w) {
        if (!visible(e)) return;
        const ImU32 col = c.isTrigger ? kTrigger : kSolid;
        const Vec3 o{c.offset.x, c.offset.y, 0.0f};
        switch (c.shape) {
            case ColliderShape2D::Box: {
                const float hx = c.size.x * 0.5f, hy = c.size.y * 0.5f;
                loop(w.matrix, {o + Vec3{-hx, -hy, 0}, o + Vec3{hx, -hy, 0}, o + Vec3{hx, hy, 0}, o + Vec3{-hx, hy, 0}}, col);
                break;
            }
            case ColliderShape2D::Circle: circle(w.matrix, o, {1, 0, 0}, {0, 1, 0}, c.radius, col); break;
            case ColliderShape2D::Capsule: {
                const float r = c.size.x * 0.5f, half = std::max(c.size.y * 0.5f - r, 0.0f);
                circle(w.matrix, o + Vec3{0, half, 0}, {1, 0, 0}, {0, 1, 0}, r, col, 0.0f, math::kPi, false);
                circle(w.matrix, o + Vec3{0, -half, 0}, {1, 0, 0}, {0, 1, 0}, r, col, math::kPi, 2.0f * math::kPi, false);
                line(w.matrix, o + Vec3{-r, -half, 0}, o + Vec3{-r, half, 0}, col);
                line(w.matrix, o + Vec3{r, -half, 0}, o + Vec3{r, half, 0}, col);
                break;
            }
        }
    });
    reg.view<TilemapCollider2DComponent, TilemapComponent, WorldTransformComponent>().each(
        [&](ecs::Entity e, TilemapCollider2DComponent&, TilemapComponent& tm, WorldTransformComponent& w) {
            if (!visible(e)) return;
            for (u32 y = 0; y < tm.height; ++y) {
                u32 x = 0;
                while (x < tm.width) {
                    if (tm.tileAt(x, y) < 0) {
                        ++x;
                        continue;
                    }
                    const u32 start = x;
                    while (x < tm.width && tm.tileAt(x, y) >= 0) ++x;
                    const float x0 = static_cast<float>(start) * tm.cellSize.x, x1 = static_cast<float>(x) * tm.cellSize.x;
                    const float y0 = static_cast<float>(y) * tm.cellSize.y, y1 = static_cast<float>(y + 1) * tm.cellSize.y;
                    loop(w.matrix, {{x0, y0, 0}, {x1, y0, 0}, {x1, y1, 0}, {x0, y1, 0}}, kSolid);
                }
            }
        });
    reg.view<ColliderComponent, WorldTransformComponent>().each([&](ecs::Entity e, ColliderComponent& c, WorldTransformComponent& w) {
        if (!visible(e)) return;
        const ImU32 col = c.isTrigger ? kTrigger : kSolid;
        switch (c.shape) {
            case ColliderShape3D::Box: {
                const Vec3 h = c.size * 0.5f;
                Vec3 p[8];
                for (int i = 0; i < 8; ++i) p[i] = c.offset + Vec3{(i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z};
                static const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
                for (auto& ed : edges) line(w.matrix, p[ed[0]], p[ed[1]], col);
                break;
            }
            case ColliderShape3D::Sphere:
                circle(w.matrix, c.offset, {1, 0, 0}, {0, 1, 0}, c.radius, col);
                circle(w.matrix, c.offset, {1, 0, 0}, {0, 0, 1}, c.radius, col);
                circle(w.matrix, c.offset, {0, 1, 0}, {0, 0, 1}, c.radius, col);
                break;
            case ColliderShape3D::Capsule: {
                const float r = c.radius, half = std::max(c.height * 0.5f - r, 0.0f);
                const Vec3 top = c.offset + Vec3{0, half, 0}, bottom = c.offset - Vec3{0, half, 0};
                circle(w.matrix, top, {1, 0, 0}, {0, 0, 1}, r, col);
                circle(w.matrix, bottom, {1, 0, 0}, {0, 0, 1}, r, col);
                circle(w.matrix, top, {1, 0, 0}, {0, 1, 0}, r, col, 0.0f, math::kPi, false);
                circle(w.matrix, bottom, {1, 0, 0}, {0, 1, 0}, r, col, math::kPi, 2.0f * math::kPi, false);
                circle(w.matrix, top, {0, 0, 1}, {0, 1, 0}, r, col, 0.0f, math::kPi, false);
                circle(w.matrix, bottom, {0, 0, 1}, {0, 1, 0}, r, col, math::kPi, 2.0f * math::kPi, false);
                for (Vec3 d : {Vec3{r, 0, 0}, Vec3{-r, 0, 0}, Vec3{0, 0, r}, Vec3{0, 0, -r}}) line(w.matrix, bottom + d, top + d, col);
                break;
            }
        }
    });
}

void ViewportPanel::drawGizmo(EditorShared& s, ImVec2 origin, ImVec2 size) {
    Scene& scene = s.ctx.scene();
    const ecs::Entity primary = scene.findByUuid(s.ctx.selection().primary());
    const bool editing = s.ctx.playState() == PlayState::Edit || s.ctx.playState() == PlayState::Paused;
    if (primary.isNull() || !editing) {
        m_gizmoWasUsing = false;
        return;
    }
    if (const auto* st = scene.registry().tryGet<StateComponent>(primary); st && st->lockedInEditor) return;
    if (scene.registry().has<UIRectComponent>(primary) || scene.registry().has<UICanvasComponent>(primary)) return;  // UI: edited in the game view
    const float aspect = size.x / std::max(size.y, 1.0f);
    const Mat4 view = viewMatrix(s), proj = projectionGL(s, aspect);
    const bool ortho = gameView ? (RenderView::fromPrimaryCamera(scene) && RenderView::fromPrimaryCamera(scene)->projection == Projection::Orthographic)
                                : s.ctx.camera().mode2D;
    ImGuizmo::SetOrthographic(ortho);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);
    const ImGuizmo::OPERATION op = gizmoOp == GizmoOp::Translate ? ImGuizmo::TRANSLATE
                                   : gizmoOp == GizmoOp::Rotate  ? ImGuizmo::ROTATE
                                                                 : ImGuizmo::SCALE;
    const ImGuizmo::MODE mode = (gizmoLocal || gizmoOp == GizmoOp::Scale) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    const bool doSnap = snap || ImGui::GetIO().KeyCtrl;
    const float snapValue = gizmoOp == GizmoOp::Translate ? snapTranslate : gizmoOp == GizmoOp::Rotate ? snapRotate : snapScale;
    const float snapValues[3] = {snapValue, snapValue, snapValue};

    const Mat4 before = scene.worldMatrix(primary);
    Mat4 after = before;
    if (ImGuizmo::Manipulate(view.data(), proj.data(), op, mode, &after.c[0].x, nullptr, doSnap ? snapValues : nullptr)) {
        const Mat4 delta = after * math::inverse(before);
        scene.setWorldMatrix(primary, after);
        // Other selected entities follow (unless an ancestor of theirs is selected too).
        for (ecs::Entity e : s.ctx.selection().entities(scene)) {
            if (e == primary) continue;
            bool ancestorSelected = false;
            for (ecs::Entity p = scene.parent(e); !p.isNull(); p = scene.parent(p)) ancestorSelected |= s.ctx.selection().contains(scene.uuidOf(p));
            if (!ancestorSelected) scene.setWorldMatrix(e, delta * scene.worldMatrix(e));
        }
        scene.updateTransforms();
    }
    const bool using_ = ImGuizmo::IsUsing();
    if (m_gizmoWasUsing && !using_) s.ctx.commit(gizmoOp == GizmoOp::Translate ? "Move" : gizmoOp == GizmoOp::Rotate ? "Rotate" : "Scale");
    m_gizmoWasUsing = using_;
}

void ViewportPanel::handlePicking(EditorShared& s, ImVec2 origin, ImVec2 size) {
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !ImGuizmo::IsOver()) {
        m_pressed = true;
        m_pressPos = io.MousePos;
    }
    if (!m_pressed || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) return;
    m_pressed = false;
    const float dx = io.MousePos.x - m_pressPos.x, dy = io.MousePos.y - m_pressPos.y;
    if (dx * dx + dy * dy > 16.0f || ImGuizmo::IsUsing()) return;  // that was a drag

    Scene& scene = s.ctx.scene();
    if (gameView && s.ui && m_lastSize.x > 0.0f) {  // a click on the game UI is not a pick in the scene
        const float k = static_cast<float>(m_width) / m_lastSize.x;
        if (s.ui->ui().isPointerOverUI({(io.MousePos.x - origin.x) * k, (io.MousePos.y - origin.y) * k})) return;
    }
    ecs::Entity hit{};
    for (auto& [e, p] : m_iconHits) {  // icons win: they are drawn on top
        const float ix = io.MousePos.x - p.x, iy = io.MousePos.y - p.y;
        if (ix * ix + iy * iy <= 14.0f * 14.0f) hit = e;
    }
    if (hit.isNull()) {
        const float aspect = size.x / std::max(size.y, 1.0f);
        const ImVec2 uv{(io.MousePos.x - origin.x) / size.x, (io.MousePos.y - origin.y) / size.y};
        if (auto h = pick(scene, rayFromScreen(projectionGL(s, aspect) * viewMatrix(s), uv), s.pickAssets())) hit = h->entity;
    }
    Selection& sel = s.ctx.selection();
    if (hit.isNull()) {
        if (!io.KeyCtrl) sel.clear();
    } else if (io.KeyCtrl) {
        sel.toggle(scene.uuidOf(hit));
    } else {
        sel.set(scene.uuidOf(hit));
    }
}

bool ViewportPanel::uiToScreen(math::Vec2 px, ImVec2& out) const {
    if (m_width == 0 || m_lastSize.x < 1.0f) return false;
    const float k = static_cast<float>(m_width) / m_lastSize.x;
    out = {m_lastOrigin.x + px.x / k, m_lastOrigin.y + px.y / k};
    return true;
}

bool ViewportPanel::handleUIEditing(EditorShared& s, ImVec2 origin, ImVec2 size, bool hovered) {
    if (!gameView || !s.ui || s.ctx.playState() != PlayState::Edit || size.x < 1.0f || size.y < 1.0f) {
        m_uiDrag = 0;
        return false;
    }
    Scene& scene = s.ctx.scene();
    auto& reg = scene.registry();
    UISystem& ui = s.ui->ui();
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float k = static_cast<float>(m_width) / size.x;  // UI pixels per window unit
    auto toScreen = [&](float x, float y) { return ImVec2{origin.x + x / k, origin.y + y / k}; };
    const math::Vec2 mouse{(io.MousePos.x - origin.x) * k, (io.MousePos.y - origin.y) * k};

    // Selected element: outline, anchor markers, resize handles.
    const ecs::Entity sel = scene.findByUuid(s.ctx.selection().primary());
    UIRectComponent* rect = sel.isNull() ? nullptr : reg.tryGet<UIRectComponent>(sel);
    const bool laidOut = rect && !ui.canvasOf(sel).isNull() && !reg.has<UICanvasComponent>(sel);
    const ImU32 accent = IM_COL32(250, 140, 40, 255);
    struct Handle {
        math::Vec2 sign;
        ImVec2 pos;
    };
    std::vector<Handle> handles;
    if (laidOut) {
        const math::Vec4 r = rect->screenRect;
        const ImVec2 a = toScreen(r.x, r.y), b = toScreen(r.x + r.z, r.y + r.w);
        dl->AddRect(a, b, accent, 0.0f, 1.5f);
        // Anchors: small triangles at the anchor points in the parent's rectangle.
        math::Vec4 pr{0, 0, static_cast<float>(m_width), static_cast<float>(m_height)};
        if (const auto* parentRect = reg.tryGet<UIRectComponent>(scene.parent(sel)); parentRect && !reg.has<UICanvasComponent>(scene.parent(sel)))
            pr = parentRect->screenRect;
        for (const math::Vec2 an : {rect->anchorMin, math::Vec2{rect->anchorMax.x, rect->anchorMin.y}, math::Vec2{rect->anchorMin.x, rect->anchorMax.y}, rect->anchorMax}) {
            const ImVec2 p = toScreen(pr.x + pr.z * an.x, pr.y + pr.w * an.y);
            dl->AddTriangleFilled({p.x, p.y}, {p.x - 5, p.y - 9}, {p.x + 5, p.y - 9}, IM_COL32(255, 255, 255, 200));
        }
        for (float hy = -1; hy <= 1; ++hy)
            for (float hx = -1; hx <= 1; ++hx) {
                if (hx == 0 && hy == 0) continue;
                const ImVec2 p{(a.x + b.x) * 0.5f + hx * (b.x - a.x) * 0.5f, (a.y + b.y) * 0.5f + hy * (b.y - a.y) * 0.5f};
                handles.push_back({{hx, hy}, p});
                dl->AddRectFilled({p.x - 4, p.y - 4}, {p.x + 4, p.y + 4}, accent);
            }
    }

    // Press: a handle of the selection, else the element under the mouse.
    if (m_uiDrag == 0 && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt) {
        for (const Handle& h : handles)
            if (std::fabs(io.MousePos.x - h.pos.x) <= 6 && std::fabs(io.MousePos.y - h.pos.y) <= 6) {
                m_uiDrag = 2;
                m_uiHandle = h.sign;
            }
        if (m_uiDrag == 0) {
            const ecs::Entity hit = ui.elementAt(mouse);
            if (hit.isNull()) return false;  // nothing of the UI here: regular picking
            if (io.KeyCtrl) {
                s.ctx.selection().toggle(scene.uuidOf(hit));
                return true;
            }
            s.ctx.selection().set(scene.uuidOf(hit));
            rect = reg.tryGet<UIRectComponent>(hit);
            m_uiDrag = rect ? 1 : 0;
        }
        if (m_uiDrag != 0) {
            m_uiDragStart = io.MousePos;
            m_uiStartPos = rect->position;
            m_uiStartSize = rect->size;
            m_uiChanged = false;
        }
        return true;
    }
    if (m_uiDrag == 0) return false;

    const ecs::Entity target = scene.findByUuid(s.ctx.selection().primary());
    rect = target.isNull() ? nullptr : reg.tryGet<UIRectComponent>(target);
    if (!rect) {
        m_uiDrag = 0;
        return false;
    }
    const float scale = std::max(1e-3f, ui.canvasScale(ui.canvasOf(target)));
    const math::Vec2 d{(io.MousePos.x - m_uiDragStart.x) * k / scale, (io.MousePos.y - m_uiDragStart.y) * k / scale};
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (std::fabs(d.x) + std::fabs(d.y) > 0.0f) m_uiChanged = true;
        const float snapStep = (snap || io.KeyCtrl) ? 10.0f : 0.0f;
        auto snapValue = [&](float v) { return snapStep > 0.0f ? std::round(v / snapStep) * snapStep : std::round(v); };
        if (m_uiDrag == 1) {
            rect->position = {snapValue(m_uiStartPos.x + d.x), snapValue(m_uiStartPos.y + d.y)};
        } else {
            math::Vec2 size2 = m_uiStartSize, pos = m_uiStartPos;
            const float* sign = &m_uiHandle.x;
            for (int axis = 0; axis < 2; ++axis) {
                const float delta = axis == 0 ? d.x : d.y, pivot = axis == 0 ? rect->pivot.x : rect->pivot.y;
                float& sz = axis == 0 ? size2.x : size2.y;
                float& p = axis == 0 ? pos.x : pos.y;
                if (sign[axis] > 0) {
                    sz += delta;
                    p += delta * pivot;
                } else if (sign[axis] < 0) {
                    sz -= delta;
                    p += delta * (1.0f - pivot);
                }
            }
            rect->size = {snapValue(size2.x), snapValue(size2.y)};
            rect->position = {snapValue(pos.x), snapValue(pos.y)};
        }
        if (reg.has<UILayoutComponent>(scene.parent(target)) && m_uiDrag == 1)
            ImGui::SetTooltip("Placé par la mise en page (UILayout) du parent : la position est ignorée");
        return true;
    }
    if (m_uiChanged) s.ctx.commit(m_uiDrag == 1 ? "Move UI Element" : "Resize UI Element");
    m_uiDrag = 0;
    return true;
}

bool ViewportPanel::handleTerrainSculpting(EditorShared& s, ImVec2 origin, ImVec2 size, bool hovered) {
    TerrainTool& tool = s.terrainTool;
    Scene& scene = s.ctx.scene();
    const ecs::Entity e = scene.findByUuid(s.ctx.selection().primary());
    TerrainComponent* tc = tool.active && !e.isNull() ? scene.registry().tryGet<TerrainComponent>(e) : nullptr;
    auto endStroke = [&] {
        if (m_sculpting && m_sculptChanged) s.ctx.commit(tool.settings.brush == terrain::Brush::Paint ? "Paint Terrain" : "Sculpt Terrain");
        m_sculpting = m_sculptChanged = false;
    };
    if (!tc || s.ctx.playState() != PlayState::Edit) {
        endStroke();
        tool.active = false;
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (hovered && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        endStroke();
        tool.active = false;
        return false;
    }
    const Mat4 world = scene.worldMatrix(e), inv = math::inverse(world);
    const float aspect = size.x / std::max(size.y, 1.0f);
    const Mat4 vp = projectionGL(s, aspect) * viewMatrix(s);
    const math::Ray ray = rayFromScreen(vp, {(io.MousePos.x - origin.x) / size.x, (io.MousePos.y - origin.y) / size.y});
    const math::Ray local{inv.transformPoint(ray.origin), math::normalize(inv.transformVector(ray.direction))};
    float dist = 0.0f;
    const bool hit = hovered && terrain::raycast(*tc, local, 10000.0f, dist);
    const Vec3 p = local.at(dist);
    if (hit) {  // brush outline on the ground
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pts[48];
        bool ok = true;
        for (int i = 0; i < 48 && ok; ++i) {
            const float a = static_cast<float>(i) / 48.0f * 2.0f * math::kPi;
            const float x = p.x + std::cos(a) * tool.settings.radius, z = p.z + std::sin(a) * tool.settings.radius;
            ok = project(vp, world.transformPoint({x, terrain::heightAt(*tc, x, z) + 0.05f, z}), origin, size, pts[i]);
        }
        if (ok) dl->AddPolyline(pts, 48, tool.settings.brush == terrain::Brush::Lower ? palette::kError : palette::kAmber, 2.0f, ImDrawFlags_Closed);
    }
    if (hit && !io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (io.KeyCtrl && tool.settings.brush == terrain::Brush::Flatten) {
            tool.settings.targetHeight = terrain::heightAt(*tc, p.x, p.z);  // pick the height
        } else {
            m_sculpting = true;
            m_sculptChanged = false;
        }
    }
    if (m_sculpting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (hit) {
            terrain::BrushSettings b = tool.settings;
            const float dt = std::min(io.DeltaTime, 0.1f);
            const bool height = b.brush == terrain::Brush::Raise || b.brush == terrain::Brush::Lower;
            b.strength = height ? b.strength * dt : std::min(1.0f, b.strength * dt * 6.0f);  // per second
            m_sculptChanged |= terrain::applyBrush(*tc, {p.x, p.z}, b);
        }
    } else if (m_sculpting) {
        endStroke();
    }
    return true;
}

bool ViewportPanel::handleTilemapPainting(EditorShared& s, ImVec2 origin, ImVec2 size, bool hovered) {
    TilemapBrush& brush = s.brush;
    Scene& scene = s.ctx.scene();
    const ecs::Entity e = scene.findByUuid(s.ctx.selection().primary());
    TilemapComponent* tm = brush.active && !e.isNull() ? scene.registry().tryGet<TilemapComponent>(e) : nullptr;
    if (!tm || s.ctx.playState() != PlayState::Edit) {
        brush.active = false;  // selection changed or play mode: painting stops
        m_painting = false;
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (hovered && !io.WantTextInput && !io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            brush.active = false;
            m_painting = false;
            return false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_B, false)) brush.tool = TilemapBrush::Tool::Paint;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) brush.tool = TilemapBrush::Tool::Erase;
        if (ImGui::IsKeyPressed(ImGuiKey_U, false)) brush.tool = TilemapBrush::Tool::Rect;
        if (ImGui::IsKeyPressed(ImGuiKey_G, false)) brush.tool = TilemapBrush::Tool::Fill;
        if (ImGui::IsKeyPressed(ImGuiKey_I, false)) brush.tool = TilemapBrush::Tool::Pick;
    }
    const Mat4 world = scene.registry().get<WorldTransformComponent>(e).matrix;
    const float aspect = size.x / std::max(size.y, 1.0f);
    const Mat4 vp = projectionGL(s, aspect) * viewMatrix(s);
    const math::Ray ray = rayFromScreen(vp, {(io.MousePos.x - origin.x) / size.x, (io.MousePos.y - origin.y) / size.y});
    const auto cell = tilemapCellAt(world, *tm, ray);
    const i32 paintValue = brush.tool == TilemapBrush::Tool::Erase ? -1 : brush.tile;

    // Outline of a cell range (inclusive) in the tilemap plane.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto outline = [&](i32 x0, i32 y0, i32 x1, i32 y1, ImU32 color, float thickness) {
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
        const float ax = static_cast<float>(x0) * tm->cellSize.x, ay = static_cast<float>(y0) * tm->cellSize.y;
        const float bx = static_cast<float>(x1 + 1) * tm->cellSize.x, by = static_cast<float>(y1 + 1) * tm->cellSize.y;
        ImVec2 p[4];
        const Vec3 corners[4] = {{ax, ay, 0}, {bx, ay, 0}, {bx, by, 0}, {ax, by, 0}};
        for (int i = 0; i < 4; ++i)
            if (!project(vp, world.transformPoint(corners[i]), origin, size, p[i])) return;
        dl->AddQuad(p[0], p[1], p[2], p[3], color, thickness);
    };
    outline(0, 0, static_cast<i32>(tm->width) - 1, static_cast<i32>(tm->height) - 1, IM_COL32(245, 165, 36, 110), 1.0f);
    if (m_painting && brush.tool == TilemapBrush::Tool::Rect) outline(m_strokeStart.first, m_strokeStart.second, m_strokeLast.first, m_strokeLast.second, palette::kAmber, 2.0f);
    else if (cell && hovered) outline(cell->first, cell->second, cell->first, cell->second,
                                      brush.tool == TilemapBrush::Tool::Erase ? palette::kError : palette::kAmber, 2.0f);

    const bool inside = cell && tilemapContains(*tm, cell->first, cell->second);
    if (hovered && !io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && cell) {
        switch (brush.tool) {
            case TilemapBrush::Tool::Paint:
            case TilemapBrush::Tool::Erase:
            case TilemapBrush::Tool::Rect:
                m_painting = true;
                m_strokeChanged = brush.tool != TilemapBrush::Tool::Rect && tilemapSet(*tm, cell->first, cell->second, paintValue);
                m_strokeStart = m_strokeLast = *cell;
                break;
            case TilemapBrush::Tool::Fill:
                if (inside && tilemapFloodFill(*tm, cell->first, cell->second, paintValue) > 0) s.ctx.commit("Fill Tilemap");
                break;
            case TilemapBrush::Tool::Pick:
                if (inside) {
                    const i32 t = tm->tileAt(static_cast<u32>(cell->first), static_cast<u32>(cell->second));
                    if (t >= 0) brush.tile = t;
                    brush.tool = t >= 0 ? TilemapBrush::Tool::Paint : TilemapBrush::Tool::Erase;
                }
                break;
        }
    }
    if (m_painting) {
        if (cell && *cell != m_strokeLast) {
            if (brush.tool != TilemapBrush::Tool::Rect) {
                // Bresenham from the previous cell: fast drags leave no gaps.
                i32 x = m_strokeLast.first, y = m_strokeLast.second;
                const i32 dx = std::abs(cell->first - x), dy = -std::abs(cell->second - y);
                const i32 sx = x < cell->first ? 1 : -1, sy = y < cell->second ? 1 : -1;
                i32 err = dx + dy;
                while (true) {
                    m_strokeChanged |= tilemapSet(*tm, x, y, paintValue);
                    if (x == cell->first && y == cell->second) break;
                    const i32 e2 = 2 * err;
                    if (e2 >= dy) { err += dy; x += sx; }
                    if (e2 <= dx) { err += dx; y += sy; }
                }
            }
            m_strokeLast = *cell;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (brush.tool == TilemapBrush::Tool::Rect)
                m_strokeChanged = tilemapFillRect(*tm, m_strokeStart.first, m_strokeStart.second, m_strokeLast.first, m_strokeLast.second, paintValue) > 0;
            if (m_strokeChanged) s.ctx.commit(brush.tool == TilemapBrush::Tool::Erase ? "Erase Tiles" : "Paint Tiles");
            m_painting = false;
        }
    }

    static const char* toolNames[] = {"Pinceau", "Gomme", "Rectangle", "Remplissage", "Pipette"};
    char hint[160];
    std::snprintf(hint, sizeof hint, "Peinture de tuiles : %s, tuile %d%s   (Échap pour quitter)", toolNames[static_cast<int>(brush.tool)], brush.tile,
                  inside ? "" : "   - hors de la carte");
    dl->AddText({origin.x + 12, origin.y + 40}, palette::kAmber, hint);
    return true;
}

void ViewportPanel::handleDrop(EditorShared& s, ImVec2 origin, ImVec2 size) {
    if (!ImGui::BeginDragDropTargetCustom(ImRect(origin, {origin.x + size.x, origin.y + size.y}), ImGui::GetID("##viewportDrop"))) return;
    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadAsset)) {
        const std::string uri = static_cast<const char*>(p->Data);
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float dropAspect = size.x / std::max(size.y, 1.0f);
        const math::Ray dropRay = rayFromScreen(projectionGL(s, dropAspect) * viewMatrix(s),
                                                {(mouse.x - origin.x) / size.x, (mouse.y - origin.y) / size.y});
        if (uri.size() > 4 && uri.compare(uri.size() - 4, 4, ".lua") == 0) {  // attach the script to the entity under the cursor
            auto hit = pick(s.ctx.scene(), dropRay, s.pickAssets());
            if (hit) {
                auto& reg = s.ctx.scene().registry();
                if (!reg.has<ScriptComponent>(hit->entity)) reg.emplace<ScriptComponent>(hit->entity);
                reg.get<ScriptComponent>(hit->entity).script = uri;
                s.ctx.selection().set(s.ctx.scene().uuidOf(hit->entity));
                s.ctx.commit("Attach Script");
            } else {
                s.notify("Déposez le script sur un objet de la scène (ou utilisez « Ajouter un composant › Script »)");
            }
            ImGui::EndDragDropTarget();
            return;
        }
        const bool materialFile = uri.size() > 7 && uri.compare(uri.size() - 7, 7, kMaterialExtension) == 0;
        if (materialFile) {  // onto the mesh under the cursor
            auto hit = pick(s.ctx.scene(), dropRay, s.pickAssets());
            auto* mr = hit ? s.ctx.scene().registry().tryGet<MeshRendererComponent>(hit->entity) : nullptr;
            if (mr) {
                mr->material = uri;
                s.ctx.selection().set(s.ctx.scene().uuidOf(hit->entity));
                s.ctx.commit("Assign Material");
            } else {
                s.notify("Déposez un matériau sur un objet 3D (MeshRenderer)");
            }
            ImGui::EndDragDropTarget();
            return;
        }
        if (isPrefabFile(uri)) {
            Vec3 pos{};
            const bool flat = s.ctx.camera().mode2D;
            if (auto h = flat ? std::nullopt : pick(s.ctx.scene(), dropRay, s.pickAssets())) pos = h->point;
            else if (float t = intersectPlane(dropRay, flat ? Vec3{0, 0, 1} : Vec3{0, 1, 0}, 0.0f); t >= 0.0f) pos = dropRay.at(t);
            if (auto r = s.ctx.instantiatePrefab(uri, pos); !r) s.notify("Échec : " + r.error().message);
            ImGui::EndDragDropTarget();
            return;
        }
        if (isModelFile(uri)) {
            const ModelAsset* model = s.models() ? s.models()->load(uri) : nullptr;
            if (!model || model->state != AssetState::Ready) {
                s.notify("Import impossible : " + (model ? model->error : std::string("pas de bibliothèque de modèles")));
            } else {
                Vec3 pos{};
                if (auto h = pick(s.ctx.scene(), dropRay, s.pickAssets())) pos = h->point;
                else if (float t = intersectPlane(dropRay, Vec3{0, 1, 0}, 0.0f); t >= 0.0f) pos = dropRay.at(t);
                s.ctx.instantiateModel(*model, pos);
                if (!model->warnings.empty()) s.notify(std::to_string(model->warnings.size()) + " avertissement(s) d'import : voir la console");
            }
            ImGui::EndDragDropTarget();
            return;
        }
        const bool sheetFile = uri.size() > 9 && uri.compare(uri.size() - 9, 9, ".agrsheet") == 0;
        const SpriteSheet* sheet = sheetFile && s.sheets() ? s.sheets()->get(uri) : nullptr;
        if (sheetFile && !sheet) {
            s.notify("Feuille de sprites invalide : " + uri + " (voir la console)");
        } else if (isImageFile(uri) || sheet) {
            const ImVec2 m = ImGui::GetIO().MousePos;
            const float aspect = size.x / std::max(size.y, 1.0f);
            const math::Ray ray = rayFromScreen(projectionGL(s, aspect) * viewMatrix(s), {(m.x - origin.x) / size.x, (m.y - origin.y) / size.y});
            Vec3 pos{};
            const bool flat = s.ctx.camera().mode2D;
            if (auto h = flat ? std::nullopt : pick(s.ctx.scene(), ray, s.pickAssets())) pos = h->point;
            else if (float t = intersectPlane(ray, flat ? Vec3{0, 0, 1} : Vec3{0, 1, 0}, 0.0f); t >= 0.0f) pos = ray.at(t);
            if (!flat) pos.y += 0.5f;  // stand on the surface (sprites are 1 unit tall)
            s.ctx.createSprite(uri, pos, {}, sheet);
        } else {
            s.notify("Ce type de fichier ne peut pas être déposé dans la scène");
        }
    }
    ImGui::EndDragDropTarget();
}

void ViewportPanel::draw(EditorShared& s, const FrameTime& time) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    const bool open = ImGui::Begin("Scène", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    m_visible = open;
    if (!open) {
        ImGui::End();
        return;
    }
    if (s.focusViewport) {
        ImGui::SetWindowFocus();
        s.focusViewport = false;
    }
    const ImVec2 panelOrigin = ImGui::GetCursorScreenPos();
    const ImVec2 panelSize = ImGui::GetContentRegionAvail();
    ImVec2 origin = panelOrigin, size = panelSize;
    // Device preview: the game view keeps the device's aspect ratio (letterboxed).
    const DevicePreset& device = kDevices[std::clamp(devicePreset, 0, kDeviceCount - 1)];
    const bool preview = gameView && devicePreset > 0;
    if (preview) {
        const float k = std::min(panelSize.x / device.width, panelSize.y / device.height);
        size = {std::floor(device.width * k), std::floor(device.height * k)};
        origin = {std::floor(panelOrigin.x + (panelSize.x - size.x) * 0.5f), std::floor(panelOrigin.y + (panelSize.y - size.y) * 0.5f)};
        ImGui::GetWindowDrawList()->AddRectFilled(panelOrigin, {panelOrigin.x + panelSize.x, panelOrigin.y + panelSize.y}, IM_COL32(8, 9, 11, 255));
        ImGui::SetCursorScreenPos(origin);
    }
    m_lastOrigin = origin;
    m_lastSize = size;
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    m_wantWidth = static_cast<u32>(std::max(1.0f, size.x * scale.x));
    m_wantHeight = static_cast<u32>(std::max(1.0f, size.y * scale.y));

    const rhi::TextureHandle tex = texture(s);
    const bool flip = s.device() && s.device()->info().renderTargetsFlipY;
    if (tex) ImGui::Image(ImTextureRef(ImGuiRenderer::toImTexture(tex)), size, {0, flip ? 1.0f : 0.0f}, {1, flip ? 0.0f : 1.0f});
    else ImGui::Dummy(size);
    const bool hovered = ImGui::IsItemHovered();

    handleCamera(s, time, hovered);
    if (preview) {
        // Simulated safe area: hatched bands where the notch and the gesture bar are.
        const float k = size.x / device.width;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImU32 band = IM_COL32(0, 0, 0, 120);
        const math::Vec4 in = device.insets;
        if (in.x > 0) dl->AddRectFilled(origin, {origin.x + in.x * k, origin.y + size.y}, band);
        if (in.y > 0) dl->AddRectFilled(origin, {origin.x + size.x, origin.y + in.y * k}, band);
        if (in.z > 0) dl->AddRectFilled({origin.x + size.x - in.z * k, origin.y}, {origin.x + size.x, origin.y + size.y}, band);
        if (in.w > 0) dl->AddRectFilled({origin.x, origin.y + size.y - in.w * k}, {origin.x + size.x, origin.y + size.y}, band);
        dl->AddRect(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(255, 255, 255, 40));
    }
    if (s.ui) {
        if (preview) {
            const float px = static_cast<float>(m_width) / device.width;  // device pixels -> view pixels
            s.ui->setSafeAreaOverride(math::Vec4{device.insets.x * px, device.insets.y * px, device.insets.z * px, device.insets.w * px});
        } else {
            s.ui->setSafeAreaOverride(gameView ? std::optional<math::Vec4>(math::Vec4{0, 0, 0, 0}) : std::nullopt);
        }
    }
    // Play mode: the mouse acts as a finger (touch controls, multi-touch code paths).
    {
        Input& input = s.engine.input();
        const ImGuiIO& io = ImGui::GetIO();
        const bool playing = s.ctx.playState() == PlayState::Playing && gameView && simulateTouch;
        constexpr u64 kFinger = 0x46494E47;  // "FING"
        const math::Vec2 p{io.MousePos.x, io.MousePos.y};
        if (playing && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            input.touchDown(kFinger, p, 1.0f);
            m_touching = true;
        } else if (m_touching && (!playing || !ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
            input.touchUp(kFinger, p);
            m_touching = false;
        } else if (m_touching) {
            input.touchMove(kFinger, p, 1.0f);
        }
    }
    // Overlays stay inside the (possibly letterboxed) view.
    ImGui::PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
    drawOverlays(s, origin, size);
    const bool painting = handleTilemapPainting(s, origin, size, hovered) || handleTerrainSculpting(s, origin, size, hovered);
    const bool uiEditing = !painting && handleUIEditing(s, origin, size, hovered);
    const bool fingerMode = gameView && simulateTouch && s.ctx.playState() != PlayState::Edit;  // clicks are touches, not picks
    if (!painting && !uiEditing && !fingerMode) {
        drawGizmo(s, origin, size);
        handlePicking(s, origin, size);
    } else {
        m_pressed = false;
    }
    // Play mode, game view: the game UI receives the mouse relative to this view.
    if (s.ui) {
        if (gameView) s.ui->setViewport({origin.x, origin.y, size.x, size.y}, m_width, m_height);
        else s.ui->clearViewport();
    }
    handleDrop(s, origin, size);
    ImGui::PopClipRect();

    // Keyboard: gizmo modes and focus (viewport hovered, not flying, not typing).
    if (hovered && !painting && !m_flying && !ImGui::GetIO().WantTextInput && !ImGui::GetIO().KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) gizmoOp = GizmoOp::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) gizmoOp = GizmoOp::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) gizmoOp = GizmoOp::Scale;
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) s.ctx.focusSelection();
    }

    // Toolbar overlay.
    ImGui::SetCursorScreenPos({panelOrigin.x + 8, panelOrigin.y + 8});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {7, 3});
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(21, 24, 29, 220));
    if (toolButton("Déplacer", gizmoOp == GizmoOp::Translate, "Déplacer (W)")) gizmoOp = GizmoOp::Translate;
    if (toolButton("Pivoter", gizmoOp == GizmoOp::Rotate, "Pivoter (E)")) gizmoOp = GizmoOp::Rotate;
    if (toolButton("Échelle", gizmoOp == GizmoOp::Scale, "Mettre à l'échelle (R)")) gizmoOp = GizmoOp::Scale;
    ImGui::SameLine(0, 10);
    if (toolButton(gizmoLocal ? "Local" : "Monde", false, "Repère du gizmo")) gizmoLocal = !gizmoLocal;
    if (toolButton("Magnétisme", snap, "Aligner sur la grille (ou maintenir Ctrl)")) snap = !snap;
    ImGui::SameLine(0, 10);
    EditorCamera& cam = s.ctx.camera();
    if (toolButton(cam.mode2D ? "2D" : "3D", false, "Basculer la vue 2D / 3D")) cam.mode2D = !cam.mode2D;
    if (toolButton("Grille", showGrid, nullptr)) showGrid = !showGrid;
    if (toolButton("Icônes", showIcons, nullptr)) showIcons = !showIcons;
    if (toolButton("Collisions", showColliders, "Afficher les formes de collision (vert ; déclencheurs en ambre)")) showColliders = !showColliders;
    if (toolButton("Navigation", showNavigation, "Maillage de navigation 3D (cyan), cases bloquées de la grille 2D (rouge), chemins des agents (jaune)"))
        showNavigation = !showNavigation;
    if (toolButton("Vue jeu", gameView, "Voir la scène depuis la caméra principale du jeu")) gameView = !gameView;
    if (gameView) {
        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(190);
        if (ImGui::BeginCombo("##device", kDevices[devicePreset].name)) {
            for (int i = 0; i < kDeviceCount; ++i)
                if (ImGui::Selectable(kDevices[i].name, i == devicePreset)) devicePreset = i;
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Aperçu d'un format d'écran, avec ses zones sûres (encoche, barre de gestes)");
        ImGui::SameLine();
        if (toolButton("Tactile", simulateTouch, "En mode Jeu, la souris agit comme un doigt")) simulateTouch = !simulateTouch;
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (gameView && !RenderView::fromPrimaryCamera(s.ctx.scene())) {
        ImGui::SetCursorScreenPos({origin.x + 12, origin.y + 40});
        ImGui::TextColored(palette::toVec4(palette::kWarning), "Aucune caméra principale active dans la scène.");
    }
    ImGui::End();
}

} // namespace agr::editor
