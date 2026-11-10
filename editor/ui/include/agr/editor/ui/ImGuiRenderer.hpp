#pragma once
#include <imgui.h>

#include "agr/rhi/Rhi.hpp"

namespace agr::editor {

/// Renders Dear ImGui draw data through the engine RHI, so the editor UI works
/// on every backend (Vulkan, OpenGL, OpenGL ES). Implements the ImGui 1.92
/// texture protocol (ImGuiBackendFlags_RendererHasTextures: dynamic font atlas).
class ImGuiRenderer {
public:
    explicit ImGuiRenderer(rhi::Device& device) : m_device(device) {}
    ~ImGuiRenderer();
    ImGuiRenderer(const ImGuiRenderer&) = delete;
    ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

    /// Creates GPU objects and declares the backend capabilities to ImGui.
    Status initialize();
    /// Records the UI pass into the backbuffer. Call between Device::beginFrame / endFrame.
    void render(ImDrawData* data, bool clear);
    /// Destroys the textures ImGui asked for (call before ImGui::DestroyContext).
    void shutdown();

    static ImTextureID toImTexture(rhi::TextureHandle h) {
        return h ? (static_cast<ImTextureID>(h.generation) << 32) | h.index : ImTextureID_Invalid;
    }
    static rhi::TextureHandle fromImTexture(ImTextureID id) {
        return rhi::TextureHandle{static_cast<u32>(id & 0xFFFFFFFFu), static_cast<u32>(id >> 32)};
    }

private:
    void updateTexture(ImTextureData* tex);

    rhi::Device& m_device;
    rhi::PipelineHandle m_pipeline;
    rhi::BufferHandle m_vertices, m_indices, m_uniforms;
    usize m_vertexCapacity = 0, m_indexCapacity = 0;
    std::vector<ImDrawVert> m_vertexStaging;
    std::vector<ImDrawIdx> m_indexStaging;
    bool m_initialized = false;
};

} // namespace agr::editor
