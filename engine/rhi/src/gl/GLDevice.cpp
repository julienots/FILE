// OpenGL 3.3 core / OpenGL ES 3.0 implementation of rhi::Device.
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <unordered_map>

#include "GL.hpp"
#include "agr/core/Log.hpp"
#include "agr/rhi/Rhi.hpp"

namespace agr::rhi {
namespace {

using namespace gl;

constexpr u32 kMaxAttributes = 16;  // minimum guaranteed by GL 3.3, GLES 3.0 and Vulkan
constexpr u32 kMaxTextureUnits = 8;
constexpr u32 kMaxUniformSlots = 8;
constexpr u32 kTimerQueries = 4;

struct GLBuffer {
    GLuint id = 0;
    GLenum target = GL_ARRAY_BUFFER;
    usize size = 0;
    BufferMemory memory = BufferMemory::Static;
};

struct GLTexture {
    GLuint id = 0;
    u32 width = 0, height = 0;
    TextureFormat format = TextureFormat::RGBA8;
    bool mips = false;
    u64 bytes = 0;
    bool ownedByTarget = false;
};

struct GLPipeline {
    GLuint program = 0;
    VertexLayout layout;
    VertexLayout instanceLayout;
    Topology topology = Topology::Triangles;
    CullMode cull = CullMode::Back;
    BlendMode blend = BlendMode::Opaque;
    bool depthTest = true, depthWrite = true;
    CompareOp depthCompare = CompareOp::LessEqual;
};

struct GLRenderTarget {
    GLuint fbo = 0;
    GLuint colorRenderbuffer = 0;  // MSAA color
    GLuint depthRenderbuffer = 0;
    TextureHandle colorTexture;    // single-sampled color
    TextureHandle depthTexture;    // sampleable depth (shadow maps)
    u32 width = 0, height = 0, samples = 1;
    TextureFormat colorFormat = TextureFormat::RGBA8;
    bool color = true;
    bool depth = false;
};

struct FormatInfo {
    GLenum internal, format, type;
};

FormatInfo toGL(TextureFormat f) {
    switch (f) {
        case TextureFormat::RGBA8: return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
        case TextureFormat::RGBA8_sRGB: return {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE};
        case TextureFormat::R8: return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
        case TextureFormat::RGBA16F: return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
        case TextureFormat::Depth24Stencil8: return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
        case TextureFormat::Depth16: return {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT};
    }
    return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
}

GLenum toGL(CompareOp op) {
    switch (op) {
        case CompareOp::Never: return GL_NEVER;
        case CompareOp::Less: return GL_LESS;
        case CompareOp::LessEqual: return GL_LEQUAL;
        case CompareOp::Equal: return GL_EQUAL;
        case CompareOp::GreaterEqual: return GL_GEQUAL;
        case CompareOp::Greater: return GL_GREATER;
        case CompareOp::Always: return GL_ALWAYS;
    }
    return GL_LEQUAL;
}

GLenum toGL(Wrap w) {
    switch (w) {
        case Wrap::Repeat: return GL_REPEAT;
        case Wrap::Clamp: return GL_CLAMP_TO_EDGE;
        case Wrap::Mirror: return GL_MIRRORED_REPEAT;
    }
    return GL_REPEAT;
}

struct AttribFormat {
    GLint components;
    GLenum type;
    GLboolean normalized;
};
AttribFormat toGL(VertexFormat f) {
    switch (f) {
        case VertexFormat::Float2: return {2, GL_FLOAT, GL_FALSE};
        case VertexFormat::Float3: return {3, GL_FLOAT, GL_FALSE};
        case VertexFormat::Float4: return {4, GL_FLOAT, GL_FALSE};
        case VertexFormat::UByte4Norm: return {4, GL_UNSIGNED_BYTE, GL_TRUE};
    }
    return {4, GL_FLOAT, GL_FALSE};
}

class GLDevice final : public Device {
public:
    GLDevice(Functions fns, bool gles) : m_gl(fns) { m_info.backend = gles ? Backend::OpenGLES : Backend::OpenGL; }

    ~GLDevice() override {
        m_targets.forEach([&](RenderTargetHandle h, GLRenderTarget&) { m_pendingTargets.push_back(h); });
        for (auto h : m_pendingTargets) destroyRenderTarget(h);
        m_textures.forEach([&](TextureHandle, GLTexture& t) { m_gl.glDeleteTextures(1, &t.id); });
        m_buffers.forEach([&](BufferHandle, GLBuffer& b) { m_gl.glDeleteBuffers(1, &b.id); });
        m_pipelines.forEach([&](PipelineHandle, GLPipeline& p) { m_gl.glDeleteProgram(p.program); });
        if (m_vao) m_gl.glDeleteVertexArrays(1, &m_vao);
        if (m_info.gpuTimers) m_gl.glDeleteQueries(kTimerQueries, m_queries.data());
    }

    Status initialize() {
        auto str = [&](GLenum e) {
            const GLubyte* s = m_gl.glGetString(e);
            return s ? std::string(reinterpret_cast<const char*>(s)) : std::string();
        };
        m_info.apiVersion = str(GL_VERSION);
        m_info.renderer = str(GL_RENDERER);
        m_info.vendor = str(GL_VENDOR);
        GLint v = 0;
        m_gl.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
        m_info.maxTextureSize = static_cast<u32>(std::max(v, 64));
        m_info.renderTargetsFlipY = true;
        v = 1;
        m_gl.glGetIntegerv(GL_MAX_SAMPLES, &v);
        m_info.maxSamples = static_cast<u32>(std::max(v, 1));
        v = 256;
        m_gl.glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);
        m_info.uniformBufferAlignment = static_cast<u32>(std::max(v, 1));
        v = 16384;
        m_gl.glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &v);
        m_info.maxUniformBlockSize = static_cast<u32>(std::max(v, 1024));

        GLint extCount = 0;
        m_gl.glGetIntegerv(GL_NUM_EXTENSIONS, &extCount);
        bool disjointTimer = false;
        for (GLint i = 0; i < extCount; ++i) {
            const GLubyte* e = m_gl.glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i));
            if (e && std::strcmp(reinterpret_cast<const char*>(e), "GL_EXT_disjoint_timer_query") == 0) disjointTimer = true;
        }
        const bool gles = m_info.backend == Backend::OpenGLES;
        m_info.gpuTimers = m_gl.glGetQueryObjectui64v && (!gles || disjointTimer);
        for (const char* sw : {"llvmpipe", "softpipe", "SwiftShader", "Software Rasterizer", "Microsoft Basic"})
            if (m_info.renderer.find(sw) != std::string::npos) m_info.softwareRenderer = true;

        m_gl.glGenVertexArrays(1, &m_vao);
        m_gl.glBindVertexArray(m_vao);
        m_gl.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        m_gl.glPixelStorei(GL_PACK_ALIGNMENT, 1);
        m_gl.glFrontFace(GL_CCW);
        if (m_info.gpuTimers) m_gl.glGenQueries(kTimerQueries, m_queries.data());
        if (GLenum err = m_gl.glGetError(); err != GL_NO_ERROR)
            AGR_LOG_WARN("RHI", "GL error 0x{:x} during device initialization", err);

        AGR_LOG_INFO("RHI", "{} device: {} | {} | max texture {} | MSAA x{} | UBO align {} | GPU timers {}",
                     enumName(m_info.backend), m_info.apiVersion, m_info.renderer, m_info.maxTextureSize,
                     m_info.maxSamples, m_info.uniformBufferAlignment, m_info.gpuTimers ? "yes" : "no");
        return {};
    }

    const DeviceInfo& info() const override { return m_info; }

    // ----------------------------------------------------------- buffers
    Result<BufferHandle> createBuffer(const BufferDesc& d) override {
        if (d.size == 0) return makeError(Error::Code::InvalidArgument, "buffer size must be > 0");
        GLBuffer b;
        b.size = d.size;
        b.memory = d.memory;
        b.target = d.usage == BufferUsage::Vertex  ? GL_ARRAY_BUFFER
                   : d.usage == BufferUsage::Index ? GL_ELEMENT_ARRAY_BUFFER
                                                   : GL_UNIFORM_BUFFER;
        m_gl.glGenBuffers(1, &b.id);
        m_gl.glBindBuffer(b.target, b.id);
        m_gl.glBufferData(b.target, static_cast<GLsizeiptr>(d.size), d.initialData,
                          d.memory == BufferMemory::Static ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
        unbindAfterEdit(b.target);
        if (GLenum err = m_gl.glGetError(); err != GL_NO_ERROR) {
            m_gl.glDeleteBuffers(1, &b.id);
            return makeError(Error::Code::Unknown, "glBufferData failed (out of memory?) for '" + std::string(d.debugName) + "'");
        }
        m_bufferBytes += d.size;
        m_stats.uploadedBytes += d.initialData ? d.size : 0;
        return m_buffers.create(b);
    }

    Status updateBuffer(BufferHandle h, usize offset, const void* data, usize size) override {
        GLBuffer* b = m_buffers.get(h);
        if (!b) return makeError(Error::Code::InvalidArgument, "invalid buffer handle");
        if (offset + size > b->size) return makeError(Error::Code::InvalidArgument, "buffer update out of range");
        if (size == 0) return {};
        m_gl.glBindBuffer(b->target, b->id);
        if (b->memory == BufferMemory::Dynamic && offset == 0) {
            // Orphan: the driver hands us fresh storage while the GPU reads the old one.
            m_gl.glBufferData(b->target, static_cast<GLsizeiptr>(b->size), nullptr, GL_DYNAMIC_DRAW);
        }
        m_gl.glBufferSubData(b->target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
        unbindAfterEdit(b->target);
        m_stats.uploadedBytes += size;
        return {};
    }

    void destroyBuffer(BufferHandle h) override {
        if (GLBuffer* b = m_buffers.get(h)) {
            if (m_boundVertexBuffer == h) m_boundVertexBuffer = {};
            if (m_boundIndexBuffer == h) m_boundIndexBuffer = {};
            if (m_boundInstanceBuffer == h) m_boundInstanceBuffer = {};
            m_gl.glDeleteBuffers(1, &b->id);
            m_bufferBytes -= b->size;
            m_buffers.release(h);
        }
    }

    // ---------------------------------------------------------- textures
    Result<TextureHandle> createTexture(const TextureDesc& d) override {
        if (d.width == 0 || d.height == 0) return makeError(Error::Code::InvalidArgument, "texture size must be > 0");
        if (d.width > m_info.maxTextureSize || d.height > m_info.maxTextureSize)
            return makeError(Error::Code::Unsupported, "texture larger than GPU limit " + std::to_string(m_info.maxTextureSize));
        GLTexture t;
        t.width = d.width;
        t.height = d.height;
        t.format = d.format;
        t.mips = d.generateMips && !isDepthFormat(d.format);
        const u64 base = static_cast<u64>(d.width) * d.height * bytesPerPixel(d.format);
        t.bytes = t.mips ? base * 4 / 3 : base;
        m_gl.glGenTextures(1, &t.id);
        m_gl.glActiveTexture(GL_TEXTURE0);
        m_gl.glBindTexture(GL_TEXTURE_2D, t.id);
        const FormatInfo fi = toGL(d.format);
        m_gl.glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fi.internal), static_cast<GLsizei>(d.width),
                          static_cast<GLsizei>(d.height), 0, fi.format, fi.type, d.data);
        if (t.mips && d.data) m_gl.glGenerateMipmap(GL_TEXTURE_2D);
        applySampler(d.sampler, t.mips);
        m_textureUnits[0] = {};  // unit 0 binding changed behind the cache
        if (GLenum err = m_gl.glGetError(); err != GL_NO_ERROR) {
            m_gl.glDeleteTextures(1, &t.id);
            return makeError(Error::Code::Unknown, "glTexImage2D failed (0x" + std::to_string(err) + ") for '" +
                                                       std::string(d.debugName) + "'");
        }
        m_textureBytes += t.bytes;
        m_stats.uploadedBytes += d.data ? base : 0;
        return m_textures.create(t);
    }

    Status updateTexture(TextureHandle h, const void* data) override {
        GLTexture* t = m_textures.get(h);
        if (!t || !data) return makeError(Error::Code::InvalidArgument, "invalid texture or data");
        const FormatInfo fi = toGL(t->format);
        m_gl.glActiveTexture(GL_TEXTURE0);
        m_gl.glBindTexture(GL_TEXTURE_2D, t->id);
        m_gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(t->width), static_cast<GLsizei>(t->height),
                             fi.format, fi.type, data);
        if (t->mips) m_gl.glGenerateMipmap(GL_TEXTURE_2D);
        m_textureUnits[0] = {};
        m_stats.uploadedBytes += static_cast<u64>(t->width) * t->height * bytesPerPixel(t->format);
        return {};
    }

    void destroyTexture(TextureHandle h) override {
        GLTexture* t = m_textures.get(h);
        if (!t) return;
        if (t->ownedByTarget) {
            AGR_LOG_WARN("RHI", "destroyTexture on a render-target texture ignored; destroy the target instead");
            return;
        }
        releaseTexture(h);
    }

    // --------------------------------------------------------- pipelines
    Result<PipelineHandle> createPipeline(const PipelineDesc& d) override {
        const bool gles = m_info.backend == Backend::OpenGLES;
        const std::string header = gles ? "#version 300 es\nprecision highp float;\nprecision highp int;\n"
                                          "precision highp sampler2D;\n#define AGR_GLES 1\n#line 1\n"
                                        : "#version 330 core\n#define AGR_GL_DESKTOP 1\n#line 1\n";
        auto compile = [&](GLenum stage, const std::string& body, GLuint& out) -> Status {
            out = m_gl.glCreateShader(stage);
            const std::string full = header + body;
            const GLchar* src = full.c_str();
            m_gl.glShaderSource(out, 1, &src, nullptr);
            m_gl.glCompileShader(out);
            GLint ok = 0;
            m_gl.glGetShaderiv(out, GL_COMPILE_STATUS, &ok);
            if (ok) return {};
            GLint len = 0;
            m_gl.glGetShaderiv(out, GL_INFO_LOG_LENGTH, &len);
            std::string log(static_cast<usize>(std::max(len, 1)), '\0');
            m_gl.glGetShaderInfoLog(out, len, nullptr, log.data());
            m_gl.glDeleteShader(out);
            out = 0;
            return makeError(Error::Code::ParseError, std::string(stage == GL_VERTEX_SHADER ? "vertex" : "fragment") +
                                                          " shader of '" + d.debugName + "' failed to compile:\n" + log);
        };
        GLuint vs = 0, fs = 0;
        if (Status s = compile(GL_VERTEX_SHADER, d.shader.vertex, vs); !s) return s.error();
        if (Status s = compile(GL_FRAGMENT_SHADER, d.shader.fragment, fs); !s) {
            m_gl.glDeleteShader(vs);
            return s.error();
        }
        GLPipeline p;
        p.program = m_gl.glCreateProgram();
        m_gl.glAttachShader(p.program, vs);
        m_gl.glAttachShader(p.program, fs);
        m_gl.glLinkProgram(p.program);
        m_gl.glDetachShader(p.program, vs);
        m_gl.glDetachShader(p.program, fs);
        m_gl.glDeleteShader(vs);
        m_gl.glDeleteShader(fs);
        GLint linked = 0;
        m_gl.glGetProgramiv(p.program, GL_LINK_STATUS, &linked);
        if (!linked) {
            GLint len = 0;
            m_gl.glGetProgramiv(p.program, GL_INFO_LOG_LENGTH, &len);
            std::string log(static_cast<usize>(std::max(len, 1)), '\0');
            m_gl.glGetProgramInfoLog(p.program, len, nullptr, log.data());
            m_gl.glDeleteProgram(p.program);
            return makeError(Error::Code::ParseError, "pipeline '" + std::string(d.debugName) + "' failed to link:\n" + log);
        }
        if (d.uniformBlocks.size() > kMaxUniformSlots || d.samplers.size() > kMaxTextureUnits ||
            d.layout.attributes.size() + d.instanceLayout.attributes.size() > kMaxAttributes) {
            m_gl.glDeleteProgram(p.program);
            return makeError(Error::Code::InvalidArgument, "too many uniform blocks, samplers or attributes");
        }
        // Explicit binding points (GLSL 3.30 / ES 3.00 have no layout(binding = N)).
        m_gl.glUseProgram(p.program);
        for (usize i = 0; i < d.uniformBlocks.size(); ++i) {
            const GLuint idx = m_gl.glGetUniformBlockIndex(p.program, d.uniformBlocks[i].c_str());
            if (idx != GL_INVALID_INDEX) m_gl.glUniformBlockBinding(p.program, idx, static_cast<GLuint>(i));
        }
        for (usize i = 0; i < d.samplers.size(); ++i) {
            const GLint loc = m_gl.glGetUniformLocation(p.program, d.samplers[i].c_str());
            if (loc >= 0) m_gl.glUniform1i(loc, static_cast<GLint>(i));
        }
        m_gl.glUseProgram(0);
        m_boundPipeline = {};
        p.layout = d.layout;
        p.instanceLayout = d.instanceLayout;
        p.topology = d.topology;
        p.cull = d.cull;
        p.blend = d.blend;
        p.depthTest = d.depthTest;
        p.depthWrite = d.depthWrite;
        p.depthCompare = d.depthCompare;
        return m_pipelines.create(std::move(p));
    }

    void destroyPipeline(PipelineHandle h) override {
        if (GLPipeline* p = m_pipelines.get(h)) {
            if (m_boundPipeline == h) m_boundPipeline = {};
            m_gl.glDeleteProgram(p->program);
            m_pipelines.release(h);
        }
    }

    // ---------------------------------------------------- render targets
    Result<RenderTargetHandle> createRenderTarget(const RenderTargetDesc& d) override {
        if (d.width == 0 || d.height == 0) return makeError(Error::Code::InvalidArgument, "render target size must be > 0");
        GLRenderTarget rt;
        rt.width = d.width;
        rt.height = d.height;
        rt.samples = std::clamp<u32>(d.samples, 1, m_info.maxSamples);
        rt.colorFormat = d.colorFormat;
        rt.color = d.color;
        rt.depth = d.depth || d.sampleableDepth;
        if (d.sampleableDepth && rt.samples > 1)
            return makeError(Error::Code::InvalidArgument, "sampleable depth requires a single-sampled target");
        if (!d.color && !rt.depth) return makeError(Error::Code::InvalidArgument, "render target without color nor depth");
        m_gl.glGenFramebuffers(1, &rt.fbo);
        m_gl.glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
        const FormatInfo fi = toGL(d.colorFormat);
        if (!d.color) {
            const GLenum none = GL_NONE;  // depth-only: no draw / read buffer (completeness on desktop GL)
            m_gl.glDrawBuffers(1, &none);
            m_gl.glReadBuffer(GL_NONE);
        } else if (rt.samples > 1) {
            m_gl.glGenRenderbuffers(1, &rt.colorRenderbuffer);
            m_gl.glBindRenderbuffer(GL_RENDERBUFFER, rt.colorRenderbuffer);
            m_gl.glRenderbufferStorageMultisample(GL_RENDERBUFFER, static_cast<GLsizei>(rt.samples), fi.internal,
                                                  static_cast<GLsizei>(d.width), static_cast<GLsizei>(d.height));
            m_gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rt.colorRenderbuffer);
        } else {
            TextureDesc td;
            td.width = d.width;
            td.height = d.height;
            td.format = d.colorFormat;
            td.generateMips = false;
            td.sampler = {Filter::Linear, Filter::Linear, false, Wrap::Clamp, Wrap::Clamp};
            auto tex = createTexture(td);
            if (!tex) {
                m_gl.glDeleteFramebuffers(1, &rt.fbo);
                return tex.error();
            }
            rt.colorTexture = tex.value();
            GLTexture* t = m_textures.get(rt.colorTexture);
            t->ownedByTarget = true;
            m_gl.glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
            m_gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->id, 0);
        }
        if (d.sampleableDepth) {
            TextureDesc td;
            td.width = d.width;
            td.height = d.height;
            td.format = TextureFormat::Depth16;
            td.generateMips = false;
            td.sampler = {Filter::Nearest, Filter::Nearest, false, Wrap::Clamp, Wrap::Clamp};
            td.debugName = d.debugName;
            auto tex = createTexture(td);
            if (!tex) {
                if (rt.colorTexture) releaseTexture(rt.colorTexture);
                if (rt.colorRenderbuffer) m_gl.glDeleteRenderbuffers(1, &rt.colorRenderbuffer);
                m_gl.glDeleteFramebuffers(1, &rt.fbo);
                return tex.error();
            }
            rt.depthTexture = tex.value();
            GLTexture* t = m_textures.get(rt.depthTexture);
            t->ownedByTarget = true;
            m_gl.glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
            m_gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, t->id, 0);
        } else if (d.depth) {
            m_gl.glGenRenderbuffers(1, &rt.depthRenderbuffer);
            m_gl.glBindRenderbuffer(GL_RENDERBUFFER, rt.depthRenderbuffer);
            // 0 (not 1) for single-sampled targets: a 1-sample renderbuffer is "multisampled"
            // for completeness rules and would not match the color texture.
            m_gl.glRenderbufferStorageMultisample(GL_RENDERBUFFER, rt.samples > 1 ? static_cast<GLsizei>(rt.samples) : 0,
                                                  GL_DEPTH24_STENCIL8,
                                                  static_cast<GLsizei>(d.width), static_cast<GLsizei>(d.height));
            m_gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt.depthRenderbuffer);
        }
        const GLenum status = m_gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);
        m_gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_gl.glBindRenderbuffer(GL_RENDERBUFFER, 0);
        const auto handle = m_targets.create(rt);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            destroyRenderTarget(handle);
            return makeError(Error::Code::Unsupported, "framebuffer incomplete (status 0x" + std::to_string(status) + ")");
        }
        // Single-sampled color is already counted as a texture; add renderbuffers only.
        const u64 pixels = static_cast<u64>(d.width) * d.height * rt.samples;
        m_targetBytes[handle.index] = (rt.samples > 1 && d.color ? pixels * bytesPerPixel(d.colorFormat) : 0) +
                                      (rt.depthRenderbuffer ? pixels * 4 : 0);
        return handle;
    }

    void destroyRenderTarget(RenderTargetHandle h) override {
        GLRenderTarget* rt = m_targets.get(h);
        if (!rt) return;
        if (rt->colorTexture) releaseTexture(rt->colorTexture);
        if (rt->depthTexture) releaseTexture(rt->depthTexture);
        if (rt->colorRenderbuffer) m_gl.glDeleteRenderbuffers(1, &rt->colorRenderbuffer);
        if (rt->depthRenderbuffer) m_gl.glDeleteRenderbuffers(1, &rt->depthRenderbuffer);
        m_gl.glDeleteFramebuffers(1, &rt->fbo);
        m_targetBytes.erase(h.index);
        m_targets.release(h);
    }

    TextureHandle renderTargetTexture(RenderTargetHandle h) const override {
        const GLRenderTarget* rt = m_targets.get(h);
        return rt ? rt->colorTexture : TextureHandle{};
    }
    TextureHandle renderTargetDepthTexture(RenderTargetHandle h) const override {
        const GLRenderTarget* rt = m_targets.get(h);
        return rt ? rt->depthTexture : TextureHandle{};
    }

    // ------------------------------------------------------------ frame
    void beginFrame(u32 w, u32 h) override {
        m_backbufferW = w;
        m_backbufferH = h;
        const f64 lastGpu = m_stats.gpuTimeMs;
        m_stats = {};
        m_stats.gpuTimeMs = lastGpu;
        if (m_info.gpuTimers) {
            m_gl.glBeginQuery(GL_TIME_ELAPSED, m_queries[m_frame % kTimerQueries]);
            m_queryActive = true;
        }
    }

    void endFrame() override {
        if (m_queryActive) {
            m_gl.glEndQuery(GL_TIME_ELAPSED);
            m_queryActive = false;
            // Read the oldest query in the ring if the GPU has finished it.
            if (m_frame + 1 >= kTimerQueries) {
                const GLuint q = m_queries[(m_frame + 1) % kTimerQueries];
                GLuint available = 0;
                m_gl.glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &available);
                if (available) {
                    GLuint64 ns = 0;
                    m_gl.glGetQueryObjectui64v(q, GL_QUERY_RESULT, &ns);
                    GLint disjoint = 0;
                    if (m_info.backend == Backend::OpenGLES) m_gl.glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
                    if (!disjoint) m_stats.gpuTimeMs = static_cast<f64>(ns) / 1e6;
                }
            }
        }
        ++m_frame;
    }

    void beginPass(RenderTargetHandle target, const ClearValues& clear) override {
        u32 w = m_backbufferW, h = m_backbufferH;
        GLuint fbo = 0;
        bool hasDepth = true;  // the default framebuffer is created with depth + stencil
        bool hasColor = true, keepDepth = false;
        if (target) {
            GLRenderTarget* rt = m_targets.get(target);
            if (!rt) {
                AGR_LOG_ERROR("RHI", "beginPass with an invalid render target");
                return;
            }
            fbo = rt->fbo;
            w = rt->width;
            h = rt->height;
            hasDepth = rt->depth;
            hasColor = rt->color;
            keepDepth = static_cast<bool>(rt->depthTexture);
        }
        m_gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        m_passTarget = target;
        m_passHasDepth = hasDepth;
        m_passHasColor = hasColor;
        m_passKeepsDepth = keepDepth;
        setViewport(0, 0, w, h);
        setScissor(false, 0, 0, 0, 0);
        if (clear.clearColor && hasColor) {
            m_gl.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            const GLfloat c[4] = {clear.color.x, clear.color.y, clear.color.z, clear.color.w};
            m_gl.glClearBufferfv(GL_COLOR, 0, c);
        }
        if (clear.clearDepth && m_passHasDepth) {
            m_gl.glDepthMask(GL_TRUE);
            m_state.depthWrite = true;
            if (keepDepth) m_gl.glClearBufferfv(GL_DEPTH, 0, &clear.depth);
            else m_gl.glClearBufferfi(GL_DEPTH_STENCIL, 0, clear.depth, 0);
        }
    }

    void endPass() override {
        // Tile-based mobile GPUs: tell the driver depth / stencil need not be stored to memory.
        if (m_gl.glInvalidateFramebuffer && m_passHasDepth && !m_passKeepsDepth) {
            if (m_passTarget) {
                const GLenum att[] = {GL_DEPTH_STENCIL_ATTACHMENT};
                m_gl.glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, att);
            } else {
                const GLenum att[] = {GL_DEPTH, GL_STENCIL};
                m_gl.glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, att);
            }
        }
        m_passTarget = {};
    }

    void setViewport(i32 x, i32 y, u32 w, u32 h) override {
        m_gl.glViewport(x, y, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
    }

    void setScissor(bool enabled, i32 x, i32 y, u32 w, u32 h) override {
        if (enabled) {
            m_gl.glEnable(GL_SCISSOR_TEST);
            m_gl.glScissor(x, y, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
        } else {
            m_gl.glDisable(GL_SCISSOR_TEST);
        }
    }

    void bindPipeline(PipelineHandle h) override {
        if (h == m_boundPipeline) return;
        GLPipeline* p = m_pipelines.get(h);
        if (!p) {
            AGR_LOG_ERROR("RHI", "bindPipeline with an invalid handle");
            return;
        }
        m_gl.glUseProgram(p->program);
        m_boundPipeline = h;
        m_attribsDirty = true;
        ++m_stats.pipelineBinds;
        applyState(*p);
    }

    void bindVertexBuffer(BufferHandle h, usize offset) override {
        if (h != m_boundVertexBuffer || offset != m_vertexOffset) {
            m_boundVertexBuffer = h;
            m_vertexOffset = offset;
            m_attribsDirty = true;
        }
    }

    void bindInstanceBuffer(BufferHandle h, usize offset) override {
        if (h != m_boundInstanceBuffer || offset != m_instanceOffset) {
            m_boundInstanceBuffer = h;
            m_instanceOffset = offset;
            m_attribsDirty = true;
        }
    }

    void bindIndexBuffer(BufferHandle h, IndexType type, usize offset) override {
        GLBuffer* b = m_buffers.get(h);
        if (!b) return;
        m_gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b->id);
        m_boundIndexBuffer = h;
        m_indexType = type;
        m_indexOffset = offset;
    }

    void bindUniformBuffer(u32 slot, BufferHandle h, usize offset, usize size) override {
        GLBuffer* b = m_buffers.get(h);
        if (!b || slot >= kMaxUniformSlots) return;
        AGR_ASSERT(offset % m_info.uniformBufferAlignment == 0, "uniform buffer offset not aligned");
        m_gl.glBindBufferRange(GL_UNIFORM_BUFFER, slot, b->id, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
    }

    void bindTexture(u32 unit, TextureHandle h) override {
        if (unit >= kMaxTextureUnits || m_textureUnits[unit] == h) return;
        GLTexture* t = m_textures.get(h);
        m_gl.glActiveTexture(GL_TEXTURE0 + unit);
        m_gl.glBindTexture(GL_TEXTURE_2D, t ? t->id : 0);
        m_textureUnits[unit] = h;
        ++m_stats.textureBinds;
    }

    void draw(u32 vertexCount, u32 firstVertex) override {
        GLPipeline* p = m_pipelines.get(m_boundPipeline);
        if (!p || !prepareAttributes(*p, 0)) return;
        m_gl.glDrawArrays(p->topology == Topology::Lines ? GL_LINES : GL_TRIANGLES, static_cast<GLint>(firstVertex),
                          static_cast<GLsizei>(vertexCount));
        count(*p, vertexCount);
    }

    void drawIndexed(u32 indexCount, u32 firstIndex, i32 baseVertex) override {
        GLPipeline* p = m_pipelines.get(m_boundPipeline);
        if (!p || !m_buffers.valid(m_boundIndexBuffer) || !prepareAttributes(*p, baseVertex)) return;
        const usize indexSize = m_indexType == IndexType::U16 ? 2 : 4;
        const auto* offset = reinterpret_cast<const void*>(m_indexOffset + static_cast<usize>(firstIndex) * indexSize);
        m_gl.glDrawElements(p->topology == Topology::Lines ? GL_LINES : GL_TRIANGLES, static_cast<GLsizei>(indexCount),
                            m_indexType == IndexType::U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, offset);
        count(*p, indexCount);
    }

    void drawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 baseVertex) override {
        GLPipeline* p = m_pipelines.get(m_boundPipeline);
        if (!p || instanceCount == 0 || !m_buffers.valid(m_boundIndexBuffer) || !prepareAttributes(*p, baseVertex)) return;
        const usize indexSize = m_indexType == IndexType::U16 ? 2 : 4;
        const auto* offset = reinterpret_cast<const void*>(m_indexOffset + static_cast<usize>(firstIndex) * indexSize);
        m_gl.glDrawElementsInstanced(p->topology == Topology::Lines ? GL_LINES : GL_TRIANGLES, static_cast<GLsizei>(indexCount),
                                     m_indexType == IndexType::U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, offset,
                                     static_cast<GLsizei>(instanceCount));
        count(*p, indexCount * instanceCount);
    }

    void blit(RenderTargetHandle src, RenderTargetHandle dst, Filter filter) override {
        GLRenderTarget* s = m_targets.get(src);
        if (!s) return;
        GLuint dstFbo = 0;
        u32 dw = m_backbufferW, dh = m_backbufferH;
        if (dst) {
            GLRenderTarget* d = m_targets.get(dst);
            if (!d) return;
            dstFbo = d->fbo;
            dw = d->width;
            dh = d->height;
        }
        if (s->samples > 1 && (dw != s->width || dh != s->height)) {
            AGR_LOG_ERROR("RHI", "MSAA resolve requires equal sizes; resolve into a same-size target first");
            return;
        }
        setScissor(false, 0, 0, 0, 0);
        m_gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, s->fbo);
        m_gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
        const bool sameSize = dw == s->width && dh == s->height;
        m_gl.glBlitFramebuffer(0, 0, static_cast<GLint>(s->width), static_cast<GLint>(s->height), 0, 0,
                               static_cast<GLint>(dw), static_cast<GLint>(dh), GL_COLOR_BUFFER_BIT,
                               (filter == Filter::Linear && !sameSize) ? GL_LINEAR : GL_NEAREST);
        m_gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    Status readBackbuffer(u32 x, u32 y, u32 w, u32 h, std::vector<u8>& out) override {
        if (x + w > m_backbufferW || y + h > m_backbufferH || w == 0 || h == 0)
            return makeError(Error::Code::InvalidArgument, "read rectangle outside the backbuffer");
        m_gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        m_gl.glReadBuffer(GL_BACK);
        out.resize(static_cast<usize>(w) * h * 4);
        const GLint glY = static_cast<GLint>(m_backbufferH - y - h);  // GL origin is bottom-left
        m_gl.glReadPixels(static_cast<GLint>(x), glY, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA,
                          GL_UNSIGNED_BYTE, out.data());
        const usize row = static_cast<usize>(w) * 4;
        std::vector<u8> tmp(row);
        for (u32 r = 0; r < h / 2; ++r) {
            u8* a = out.data() + r * row;
            u8* b = out.data() + (h - 1 - r) * row;
            std::memcpy(tmp.data(), a, row);
            std::memcpy(a, b, row);
            std::memcpy(b, tmp.data(), row);
        }
        if (GLenum err = m_gl.glGetError(); err != GL_NO_ERROR)
            return makeError(Error::Code::IOError, "glReadPixels failed (0x" + std::to_string(err) + ")");
        return {};
    }

    const FrameStats& frameStats() const override { return m_stats; }

    MemoryStats memoryStats() const override {
        MemoryStats m;
        m.bufferBytes = m_bufferBytes;
        m.textureBytes = m_textureBytes;
        for (const auto& [idx, bytes] : m_targetBytes) m.textureBytes += bytes;
        m.buffers = static_cast<u32>(m_buffers.size());
        m.textures = static_cast<u32>(m_textures.size());
        m.pipelines = static_cast<u32>(m_pipelines.size());
        m.renderTargets = static_cast<u32>(m_targets.size());
        return m;
    }

private:
    struct CachedState {
        bool valid = false;
        CullMode cull = CullMode::None;
        BlendMode blend = BlendMode::Opaque;
        bool depthTest = false, depthWrite = true;
        CompareOp depthCompare = CompareOp::Less;
    };

    void unbindAfterEdit(GLenum target) {
        // Element-array binding is VAO state: restore the one the draw path expects.
        if (target == GL_ELEMENT_ARRAY_BUFFER) {
            const GLBuffer* ib = m_buffers.get(m_boundIndexBuffer);
            m_gl.glBindBuffer(target, ib ? ib->id : 0);
        } else {
            m_gl.glBindBuffer(target, 0);
            if (target == GL_ARRAY_BUFFER) m_attribsDirty = true;
        }
    }

    void releaseTexture(TextureHandle h) {
        GLTexture* t = m_textures.get(h);
        if (!t) return;
        for (auto& unit : m_textureUnits)
            if (unit == h) unit = {};
        m_gl.glDeleteTextures(1, &t->id);
        m_textureBytes -= t->bytes;
        m_textures.release(h);
    }

    void applySampler(const SamplerDesc& s, bool hasMips) {
        GLenum minF;
        if (hasMips && s.mipmaps) minF = s.minFilter == Filter::Linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
        else minF = s.minFilter == Filter::Linear ? GL_LINEAR : GL_NEAREST;
        m_gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minF));
        m_gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s.magFilter == Filter::Linear ? GL_LINEAR : GL_NEAREST);
        m_gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(toGL(s.wrapU)));
        m_gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(toGL(s.wrapV)));
    }

    void applyState(const GLPipeline& p) {
        CachedState& c = m_state;
        if (!c.valid || c.cull != p.cull) {
            if (p.cull == CullMode::None) m_gl.glDisable(GL_CULL_FACE);
            else {
                m_gl.glEnable(GL_CULL_FACE);
                m_gl.glCullFace(p.cull == CullMode::Back ? GL_BACK : GL_FRONT);
            }
            c.cull = p.cull;
        }
        if (!c.valid || c.blend != p.blend) {
            switch (p.blend) {
                case BlendMode::Opaque: m_gl.glDisable(GL_BLEND); break;
                case BlendMode::Alpha:
                    m_gl.glEnable(GL_BLEND);
                    m_gl.glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                case BlendMode::PremultipliedAlpha:
                    m_gl.glEnable(GL_BLEND);
                    m_gl.glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                case BlendMode::Additive:
                    m_gl.glEnable(GL_BLEND);
                    m_gl.glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
                    break;
            }
            m_gl.glBlendEquation(GL_FUNC_ADD);
            c.blend = p.blend;
        }
        if (!c.valid || c.depthTest != p.depthTest) {
            if (p.depthTest) m_gl.glEnable(GL_DEPTH_TEST);
            else m_gl.glDisable(GL_DEPTH_TEST);
            c.depthTest = p.depthTest;
        }
        if (!c.valid || c.depthWrite != p.depthWrite) {
            m_gl.glDepthMask(p.depthWrite ? GL_TRUE : GL_FALSE);
            c.depthWrite = p.depthWrite;
        }
        if (!c.valid || c.depthCompare != p.depthCompare) {
            m_gl.glDepthFunc(toGL(p.depthCompare));
            c.depthCompare = p.depthCompare;
        }
        c.valid = true;
    }

    /// Re-specifies vertex attribute pointers when the buffer, offset, pipeline
    /// or base vertex changed. Base vertex is emulated through the pointer offset
    /// because glDrawElementsBaseVertex is not in OpenGL ES 3.0.
    bool prepareAttributes(const GLPipeline& p, i32 baseVertex) {
        GLBuffer* vb = m_buffers.get(m_boundVertexBuffer);
        if (!vb) {
            AGR_LOG_ERROR("RHI", "draw without a valid vertex buffer");
            return false;
        }
        if (!m_attribsDirty && baseVertex == m_lastBaseVertex) return true;
        m_gl.glBindBuffer(GL_ARRAY_BUFFER, vb->id);
        u32 enabledMask = 0;
        const usize base = m_vertexOffset + static_cast<usize>(static_cast<i64>(baseVertex) * p.layout.stride);
        u32 instancedMask = 0;
        for (const VertexAttribute& a : p.layout.attributes) {
            const AttribFormat f = toGL(a.format);
            m_gl.glVertexAttribPointer(a.location, f.components, f.type, f.normalized,
                                       static_cast<GLsizei>(p.layout.stride), reinterpret_cast<const void*>(base + a.offset));
            enabledMask |= 1u << a.location;
        }
        if (p.instanceLayout.stride > 0) {
            GLBuffer* ib = m_buffers.get(m_boundInstanceBuffer);
            if (!ib) {
                AGR_LOG_ERROR("RHI", "instanced pipeline drawn without a valid instance buffer");
                m_attribsDirty = true;
                return false;
            }
            m_gl.glBindBuffer(GL_ARRAY_BUFFER, ib->id);
            for (const VertexAttribute& a : p.instanceLayout.attributes) {
                const AttribFormat f = toGL(a.format);
                m_gl.glVertexAttribPointer(a.location, f.components, f.type, f.normalized, static_cast<GLsizei>(p.instanceLayout.stride),
                                           reinterpret_cast<const void*>(m_instanceOffset + a.offset));
                enabledMask |= 1u << a.location;
                instancedMask |= 1u << a.location;
            }
        }
        for (u32 i = 0; i < kMaxAttributes; ++i) {  // per-instance attributes advance once per instance
            const bool want = instancedMask & (1u << i), have = m_instancedAttribs & (1u << i);
            if (want != have) m_gl.glVertexAttribDivisor(i, want ? 1 : 0);
        }
        m_instancedAttribs = instancedMask;
        for (u32 i = 0; i < kMaxAttributes; ++i) {
            const bool want = enabledMask & (1u << i), have = m_enabledAttribs & (1u << i);
            if (want && !have) m_gl.glEnableVertexAttribArray(i);
            else if (!want && have) m_gl.glDisableVertexAttribArray(i);
        }
        m_enabledAttribs = enabledMask;
        m_attribsDirty = false;
        m_lastBaseVertex = baseVertex;
        return true;
    }

    void count(const GLPipeline& p, u32 vertices) {
        ++m_stats.drawCalls;
        m_stats.vertices += vertices;
        if (p.topology == Topology::Triangles) m_stats.triangles += vertices / 3;
    }

    Functions m_gl;
    DeviceInfo m_info;
    FrameStats m_stats;
    HandlePool<GLBuffer, BufferTag> m_buffers;
    HandlePool<GLTexture, TextureTag> m_textures;
    HandlePool<GLPipeline, PipelineTag> m_pipelines;
    HandlePool<GLRenderTarget, RenderTargetTag> m_targets;
    std::vector<RenderTargetHandle> m_pendingTargets;
    std::unordered_map<u32, u64> m_targetBytes;
    u64 m_bufferBytes = 0, m_textureBytes = 0;

    GLuint m_vao = 0;
    CachedState m_state;
    PipelineHandle m_boundPipeline;
    BufferHandle m_boundVertexBuffer, m_boundIndexBuffer, m_boundInstanceBuffer;
    usize m_vertexOffset = 0, m_indexOffset = 0, m_instanceOffset = 0;
    IndexType m_indexType = IndexType::U16;
    bool m_attribsDirty = true;
    i32 m_lastBaseVertex = 0;
    u32 m_enabledAttribs = 0;
    u32 m_instancedAttribs = 0;  // attributes with divisor 1
    std::array<TextureHandle, kMaxTextureUnits> m_textureUnits{};
    RenderTargetHandle m_passTarget;
    bool m_passHasDepth = true;
    bool m_passHasColor = true;
    bool m_passKeepsDepth = false;  // sampleable depth: must not be invalidated
    u32 m_backbufferW = 0, m_backbufferH = 0;

    std::array<GLuint, kTimerQueries> m_queries{};
    bool m_queryActive = false;
    u64 m_frame = 0;
};

} // namespace

Result<std::unique_ptr<Device>> createOpenGLDevice(const GLProcLoader& loader, bool gles) {
    if (!loader) return makeError(Error::Code::InvalidArgument, "no GL loader");
    gl::Functions fns;
    std::string missing;
#define AGR_GL_LOAD_REQUIRED(ret, name, args)                                   \
    fns.name = reinterpret_cast<ret(AGR_GLAPI*) args>(loader(#name));           \
    if (!fns.name) missing += std::string(missing.empty() ? "" : ", ") + #name;
#define AGR_GL_LOAD_OPTIONAL(ret, name, args) fns.name = reinterpret_cast<ret(AGR_GLAPI*) args>(loader(#name));
    AGR_GL_REQUIRED(AGR_GL_LOAD_REQUIRED)
    AGR_GL_OPTIONAL(AGR_GL_LOAD_OPTIONAL)
#undef AGR_GL_LOAD_REQUIRED
#undef AGR_GL_LOAD_OPTIONAL
    if (gles && !fns.glGetQueryObjectui64v)
        fns.glGetQueryObjectui64v =
            reinterpret_cast<void(AGR_GLAPI*)(gl::GLuint, gl::GLenum, gl::GLuint64*)>(loader("glGetQueryObjectui64vEXT"));
    if (!missing.empty()) return makeError(Error::Code::Unsupported, "OpenGL functions missing: " + missing);
    auto device = std::make_unique<GLDevice>(fns, gles);
    if (Status s = device->initialize(); !s) return s.error();
    return std::unique_ptr<Device>(std::move(device));
}

} // namespace agr::rhi
