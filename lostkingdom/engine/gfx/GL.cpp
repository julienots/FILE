#include "GL.h"
#include "../core/Platform.h"
#include <cstring>

namespace agr {

GpuCaps gGpu;

void glCheck(const char* where) {
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) AGR_WARN("GL error 0x%x at %s", e, where);
}

void GpuCaps::detect() {
    renderer = (const char*)glGetString(GL_RENDERER);
    version = (const char*)glGetString(GL_VERSION);
    GLint n = 0; glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (GLint i = 0; i < n; i++) {
        const char* e = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (!e) continue;
        if (!strcmp(e, "GL_EXT_color_buffer_float")) floatColorBuffer = true;
        if (!strcmp(e, "GL_EXT_color_buffer_half_float")) halfFloatColorBuffer = true;
    }
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    AGR_LOG("GPU: %s | %s | float RT=%d half RT=%d", renderer.c_str(), version.c_str(), floatColorBuffer, halfFloatColorBuffer);
}

static GLuint compileStage(GLenum type, const std::string& src, const std::string& name) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096]; glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        AGR_ERR("Shader '%s' (%s) compile error:\n%s", name.c_str(), type == GL_VERTEX_SHADER ? "vs" : "fs", log);
        // print numbered source for debugging
        int line = 1; std::string cur;
        for (char ch : src) { if (ch == '\n') { AGR_ERR("%4d: %s", line++, cur.c_str()); cur.clear(); } else cur += ch; }
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool Shader::build(const std::string& n, const std::string& vs, const std::string& fs, const std::string& defines) {
    name = n;
    std::string head = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision mediump sampler2DArray;\nprecision mediump sampler2DShadow;\n" + defines + "\n";
    GLuint v = compileStage(GL_VERTEX_SHADER, head + vs, n);
    GLuint f = compileStage(GL_FRAGMENT_SHADER, head + fs, n);
    if (!v || !f) return false;
    prog = glCreateProgram();
    glAttachShader(prog, v); glAttachShader(prog, f);
    glBindAttribLocation(prog, 0, "aPos");
    glBindAttribLocation(prog, 1, "aNormal");
    glBindAttribLocation(prog, 2, "aColor");
    glBindAttribLocation(prog, 3, "aMat");
    glBindAttribLocation(prog, 4, "aSkin");
    glBindAttribLocation(prog, 5, "iPosRot");
    glBindAttribLocation(prog, 6, "iScale");
    glBindAttribLocation(prog, 7, "iTint");
    glBindAttribLocation(prog, 8, "iP0");
    glBindAttribLocation(prog, 9, "iP1");
    glBindAttribLocation(prog, 10, "iP2");
    glBindAttribLocation(prog, 11, "iP3");
    glLinkProgram(prog);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096]; glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        AGR_ERR("Shader '%s' link error: %s", n.c_str(), log);
        glDeleteProgram(prog); prog = 0;
        return false;
    }
    GLuint blk = glGetUniformBlockIndex(prog, "Frame");
    if (blk != GL_INVALID_INDEX) glUniformBlockBinding(prog, blk, 0);
    return true;
}

void Shader::destroy() { if (prog) glDeleteProgram(prog); prog = 0; cache.clear(); }

GLint Shader::loc(const char* u) {
    auto it = cache.find(u);
    if (it != cache.end()) return it->second;
    GLint l = glGetUniformLocation(prog, u);
    cache[u] = l;
    return l;
}

void Mesh::upload(const std::vector<Vertex>& v, const std::vector<uint32_t>& idx, bool dynamic) {
    dynamic_ = dynamic;
    if (!vao) { glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ibo); }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(), dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    const GLsizei st = sizeof(Vertex);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_BYTE, GL_TRUE, st, (void*)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)16);
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)20);
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_FALSE, st, (void*)24);
    index32 = v.size() > 65535;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    if (index32) {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STATIC_DRAW);
    } else {
        std::vector<uint16_t> s(idx.begin(), idx.end());
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, s.size() * 2, s.data(), dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    }
    glBindVertexArray(0);
    indexCount = (int)idx.size();
    vertexCount = (int)v.size();
    bounds = AABB();
    for (auto& p : v) bounds.add({p.px, p.py, p.pz});
}

void Mesh::updateVertices(const std::vector<Vertex>& v) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if ((int)v.size() != vertexCount) glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(), GL_DYNAMIC_DRAW);
    else glBufferSubData(GL_ARRAY_BUFFER, 0, v.size() * sizeof(Vertex), v.data());
    vertexCount = (int)v.size();
}

void Mesh::setInstances(const std::vector<InstanceData>& inst) {
    glBindVertexArray(vao);
    if (!instVbo) glGenBuffers(1, &instVbo);
    glBindBuffer(GL_ARRAY_BUFFER, instVbo);
    if ((int)inst.size() > instanceCapacity) {
        glBufferData(GL_ARRAY_BUFFER, inst.size() * sizeof(InstanceData), inst.data(), GL_DYNAMIC_DRAW);
        instanceCapacity = (int)inst.size();
    } else if (!inst.empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, inst.size() * sizeof(InstanceData), inst.data());
    }
    const GLsizei st = sizeof(InstanceData);
    glEnableVertexAttribArray(5); glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, st, (void*)0); glVertexAttribDivisor(5, 1);
    glEnableVertexAttribArray(6); glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, st, (void*)16); glVertexAttribDivisor(6, 1);
    // tint packed in the same attribute location range: use location 6 as vec4 (scale, r, g, b) is not possible with mixed types,
    // so tint is read through a second pointer on location 7.
    glEnableVertexAttribArray(7); glVertexAttribPointer(7, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)20); glVertexAttribDivisor(7, 1);
    glBindVertexArray(0);
    instanceCount = (int)inst.size();
}

void Mesh::draw() const {
    if (!vao || !indexCount) return;
    glBindVertexArray(vao);
    GLenum it = index32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
    if (instVbo) { if (instanceCount > 0) glDrawElementsInstanced(GL_TRIANGLES, indexCount, it, nullptr, instanceCount); }
    else glDrawElements(GL_TRIANGLES, indexCount, it, nullptr);
}

void Mesh::destroy() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ibo) glDeleteBuffers(1, &ibo);
    if (instVbo) glDeleteBuffers(1, &instVbo);
    vao = vbo = ibo = instVbo = 0; indexCount = vertexCount = instanceCount = instanceCapacity = 0;
}

void Texture::create2D(int w_, int h_, GLenum internalFmt, GLenum fmt, GLenum type, const void* data, bool mips, bool repeat, bool linear) {
    w = w_; h = h_; target = GL_TEXTURE_2D;
    if (!id) glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, type, data);
    GLenum wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    if (mips) { glGenerateMipmap(GL_TEXTURE_2D); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); }
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
}

void Texture::createArray(int w_, int h_, int n, const std::vector<std::vector<uint8_t>>& L, bool repeat) {
    w = w_; h = h_; layers = n; target = GL_TEXTURE_2D_ARRAY;
    if (!id) glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D_ARRAY, id);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, w, h, n, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    for (int i = 0; i < n; i++) glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, L[i].data());
    GLenum wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
}

bool RenderTarget::create(int w_, int h_, GLenum colorFmt, bool withDepth, bool depthCompare, bool) {
    destroy();
    w = w_; h = h_;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (colorFmt) {
        GLenum fmt = GL_RGBA, type = GL_UNSIGNED_BYTE;
        if (colorFmt == GL_RGBA16F) type = GL_HALF_FLOAT;
        else if (colorFmt == GL_R11F_G11F_B10F) { fmt = GL_RGB; type = GL_HALF_FLOAT; }
        else if (colorFmt == GL_RGB10_A2) type = GL_UNSIGNED_INT_2_10_10_10_REV;
        color.create2D(w, h, colorFmt, fmt, type, nullptr, false, false, true);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color.id, 0);
    } else {
        GLenum none = GL_NONE; glDrawBuffers(1, &none); glReadBuffer(GL_NONE);
    }
    if (withDepth) {
        depth.create2D(w, h, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr, false, false, depthCompare);
        if (depthCompare) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth.id, 0);
    }
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) { AGR_ERR("FBO incomplete 0x%x (%dx%d fmt 0x%x)", st, w, h, colorFmt); destroy(); return false; }
    return true;
}

void RenderTarget::destroy() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    fbo = 0; color.destroy(); depth.destroy();
}

void RenderTarget::bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, w, h); }

}  // namespace agr
