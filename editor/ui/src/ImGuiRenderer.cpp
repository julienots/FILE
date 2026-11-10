#include "agr/editor/ui/ImGuiRenderer.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <unordered_set>

#include "agr/core/Log.hpp"

namespace agr::editor {

namespace {

const char* kVertex = R"(
layout(std140) uniform ImGuiData { vec4 u_scaleTranslate; };
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;
out vec2 v_uv;
out vec4 v_color;
void main() {
    v_uv = a_uv;
    v_color = a_color;
    gl_Position = vec4(a_pos * u_scaleTranslate.xy + u_scaleTranslate.zw, 0.0, 1.0);
}
)";

const char* kFragment = R"(
uniform sampler2D u_texture;
in vec2 v_uv;
in vec4 v_color;
layout(location = 0) out vec4 o_color;
void main() { o_color = v_color * texture(u_texture, v_uv); }
)";

static_assert(sizeof(ImDrawIdx) == 2, "AGR's ImGui renderer expects 16-bit indices");

} // namespace

ImGuiRenderer::~ImGuiRenderer() {
    m_device.destroyPipeline(m_pipeline);
    m_device.destroyBuffer(m_vertices);
    m_device.destroyBuffer(m_indices);
    m_device.destroyBuffer(m_uniforms);
}

Status ImGuiRenderer::initialize() {
    rhi::PipelineDesc p;
    p.debugName = "agr.editor.imgui";
    p.shader = {kVertex, kFragment};
    p.layout.stride = sizeof(ImDrawVert);
    p.layout.attributes = {{0, rhi::VertexFormat::Float2, offsetof(ImDrawVert, pos)},
                           {1, rhi::VertexFormat::Float2, offsetof(ImDrawVert, uv)},
                           {2, rhi::VertexFormat::UByte4Norm, offsetof(ImDrawVert, col)}};
    p.cull = rhi::CullMode::None;
    p.blend = rhi::BlendMode::Alpha;
    p.depthTest = false;
    p.depthWrite = false;
    p.uniformBlocks = {"ImGuiData"};
    p.samplers = {"u_texture"};
    auto pipe = m_device.createPipeline(p);
    if (!pipe) return pipe.error();
    m_pipeline = pipe.value();
    auto ubo = m_device.createBuffer({rhi::BufferUsage::Uniform, rhi::BufferMemory::Dynamic, 256, nullptr, "agr.editor.imgui.ubo"});
    if (!ubo) return ubo.error();
    m_uniforms = ubo.value();

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "agr_rhi";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
    ImGui::GetPlatformIO().Renderer_TextureMaxWidth = ImGui::GetPlatformIO().Renderer_TextureMaxHeight =
        static_cast<int>(m_device.info().maxTextureSize);
    m_initialized = true;
    return {};
}

void ImGuiRenderer::updateTexture(ImTextureData* tex) {
    if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) {
        std::vector<u8> rgba;
        const u8* pixels = static_cast<const u8*>(tex->GetPixels());
        if (tex->Format == ImTextureFormat_Alpha8) {  // expand to RGBA (white + coverage)
            rgba.resize(static_cast<usize>(tex->Width) * tex->Height * 4, 255);
            for (int i = 0; i < tex->Width * tex->Height; ++i) rgba[static_cast<usize>(i) * 4 + 3] = pixels[i];
            pixels = rgba.data();
        }
        if (tex->Status == ImTextureStatus_WantCreate) {
            rhi::TextureDesc d;
            d.width = static_cast<u32>(tex->Width);
            d.height = static_cast<u32>(tex->Height);
            d.format = rhi::TextureFormat::RGBA8;  // UI colors are already gamma encoded
            d.generateMips = false;
            d.sampler = {rhi::Filter::Linear, rhi::Filter::Linear, false, rhi::Wrap::Clamp, rhi::Wrap::Clamp};
            d.data = pixels;
            d.debugName = "agr.editor.imgui.texture";
            auto t = m_device.createTexture(d);
            if (!t) {
                AGR_LOG_ERROR("Editor", "ImGui texture creation failed: {}", t.error().message);
                return;
            }
            tex->SetTexID(toImTexture(t.value()));
        } else {
            (void)m_device.updateTexture(fromImTexture(tex->TexID), pixels);
        }
        tex->SetStatus(ImTextureStatus_OK);
    } else if (tex->Status == ImTextureStatus_WantDestroy) {
        m_device.destroyTexture(fromImTexture(tex->TexID));  // RHI destruction is deferred until the GPU is done
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

void ImGuiRenderer::render(ImDrawData* data, bool clear) {
    if (!m_initialized || !data) return;
    if (data->Textures)
        for (ImTextureData* tex : *data->Textures)
            if (tex->Status != ImTextureStatus_OK) updateTexture(tex);

    const u32 fbW = static_cast<u32>(data->DisplaySize.x * data->FramebufferScale.x);
    const u32 fbH = static_cast<u32>(data->DisplaySize.y * data->FramebufferScale.y);
    if (fbW == 0 || fbH == 0) return;

    // Gather geometry into one vertex and one index buffer.
    m_vertexStaging.clear();
    m_indexStaging.clear();
    std::unordered_set<u64> sampled;
    for (const ImDrawList* list : data->CmdLists) {
        m_vertexStaging.insert(m_vertexStaging.end(), list->VtxBuffer.begin(), list->VtxBuffer.end());
        m_indexStaging.insert(m_indexStaging.end(), list->IdxBuffer.begin(), list->IdxBuffer.end());
        for (const ImDrawCmd& cmd : list->CmdBuffer) sampled.insert(cmd.GetTexID());
    }
    // Render-target textures shown in the UI must be readable before the pass starts.
    for (u64 id : sampled) m_device.prepareTextureForSampling(fromImTexture(id));

    auto grow = [&](rhi::BufferHandle& buffer, usize& capacity, usize needed, rhi::BufferUsage usage, const char* name) {
        if (needed <= capacity) return true;
        m_device.destroyBuffer(buffer);
        capacity = std::max<usize>(std::bit_ceil(needed), 64 * 1024);
        auto b = m_device.createBuffer({usage, rhi::BufferMemory::Dynamic, capacity, nullptr, name});
        if (!b) {
            capacity = 0;
            return false;
        }
        buffer = b.value();
        return true;
    };
    const usize vbytes = m_vertexStaging.size() * sizeof(ImDrawVert), ibytes = m_indexStaging.size() * sizeof(ImDrawIdx);
    const bool haveGeometry = vbytes > 0 && ibytes > 0 &&
                              grow(m_vertices, m_vertexCapacity, vbytes, rhi::BufferUsage::Vertex, "agr.editor.imgui.vb") &&
                              grow(m_indices, m_indexCapacity, ibytes, rhi::BufferUsage::Index, "agr.editor.imgui.ib");
    if (haveGeometry) {
        (void)m_device.updateBuffer(m_vertices, 0, m_vertexStaging.data(), vbytes);
        (void)m_device.updateBuffer(m_indices, 0, m_indexStaging.data(), ibytes);
    }
    const float L = data->DisplayPos.x, R = L + data->DisplaySize.x, T = data->DisplayPos.y, B = T + data->DisplaySize.y;
    const float st[4] = {2.0f / (R - L), -2.0f / (B - T), -1.0f - L * 2.0f / (R - L), 1.0f + T * 2.0f / (B - T)};
    (void)m_device.updateBuffer(m_uniforms, 0, st, sizeof st);

    m_device.beginPass({}, {clear, {0.055f, 0.063f, 0.075f, 1.0f}, true, 1.0f});
    if (haveGeometry) {
        m_device.setViewport(0, 0, fbW, fbH);
        m_device.bindPipeline(m_pipeline);
        m_device.bindVertexBuffer(m_vertices);
        m_device.bindIndexBuffer(m_indices, rhi::IndexType::U16);
        m_device.bindUniformBuffer(0, m_uniforms, 0, sizeof st);
        const ImVec2 scale = data->FramebufferScale;
        u32 vtxBase = 0, idxBase = 0;
        for (const ImDrawList* list : data->CmdLists) {
            for (const ImDrawCmd& cmd : list->CmdBuffer) {
                if (cmd.UserCallback) {
                    if (cmd.UserCallback != ImGui::GetPlatformIO().DrawCallback_ResetRenderState) cmd.UserCallback(list, &cmd);
                    continue;
                }
                const float x0 = std::max(0.0f, (cmd.ClipRect.x - L) * scale.x);
                const float y0 = std::max(0.0f, (cmd.ClipRect.y - T) * scale.y);
                const float x1 = std::min(static_cast<float>(fbW), (cmd.ClipRect.z - L) * scale.x);
                const float y1 = std::min(static_cast<float>(fbH), (cmd.ClipRect.w - T) * scale.y);
                if (x1 <= x0 || y1 <= y0) continue;
                // RHI scissors use a bottom-left origin.
                m_device.setScissor(true, static_cast<i32>(x0), static_cast<i32>(static_cast<float>(fbH) - y1),
                                    static_cast<u32>(x1 - x0), static_cast<u32>(y1 - y0));
                m_device.bindTexture(0, fromImTexture(cmd.GetTexID()));
                m_device.drawIndexed(cmd.ElemCount, idxBase + cmd.IdxOffset, static_cast<i32>(vtxBase + cmd.VtxOffset));
            }
            vtxBase += static_cast<u32>(list->VtxBuffer.Size);
            idxBase += static_cast<u32>(list->IdxBuffer.Size);
        }
        m_device.setScissor(false, 0, 0, 0, 0);
    }
    m_device.endPass();
}

void ImGuiRenderer::shutdown() {
    if (!ImGui::GetCurrentContext()) return;
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
        if (tex->RefCount == 1 && tex->TexID != ImTextureID_Invalid) {
            m_device.destroyTexture(fromImTexture(tex->TexID));
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
    }
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    io.BackendRendererName = nullptr;
    m_initialized = false;
}

} // namespace agr::editor
