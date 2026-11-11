// AGR Engine — OpenGL ES 3.0 resource wrappers.
#pragma once
#include <GLES3/gl3.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "../core/Math.h"

namespace agr {

// ---------------------------------------------------------------------------
// Vertex layout shared by every lit mesh (28 bytes):
//   position (3f) | normal (4 x snorm8) | albedo rgba8 | material rgba8 | skin (bone0, bone1, weight0, extra)
// material: r = roughness, g = metallic, b = emissive, a = wind weight (or terrain splat)
// For terrain the material/skin bytes carry splat weights (see TerrainShader).
// ---------------------------------------------------------------------------
struct Vertex {
    float px, py, pz;
    int8_t nx, ny, nz, nw;
    uint8_t r, g, b, a;
    uint8_t rough, metal, emis, wind;
    uint8_t b0, b1, w0, ex;
};
static_assert(sizeof(Vertex) == 28, "Vertex must be 28 bytes");

// Per-instance data for instanced foliage / props (24 bytes).
struct InstanceData {
    float x, y, z, rotY;
    float scale;
    uint8_t r, g, b, a;  // tint
};

class Shader {
public:
    GLuint prog = 0;
    std::string name;
    bool build(const std::string& name, const std::string& vs, const std::string& fs, const std::string& defines);
    void destroy();
    GLint loc(const char* uniform);
    void use() const { glUseProgram(prog); }
    void set(const char* u, float v) { GLint l = loc(u); if (l >= 0) glUniform1f(l, v); }
    void set(const char* u, int v) { GLint l = loc(u); if (l >= 0) glUniform1i(l, v); }
    void set(const char* u, vec2 v) { GLint l = loc(u); if (l >= 0) glUniform2f(l, v.x, v.y); }
    void set(const char* u, vec3 v) { GLint l = loc(u); if (l >= 0) glUniform3f(l, v.x, v.y, v.z); }
    void set(const char* u, vec4 v) { GLint l = loc(u); if (l >= 0) glUniform4f(l, v.x, v.y, v.z, v.w); }
    void set(const char* u, const mat4& m) { GLint l = loc(u); if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, m.m); }
    void setMat4Array(const char* u, const float* data, int count) { GLint l = loc(u); if (l >= 0) glUniformMatrix4fv(l, count, GL_FALSE, data); }
    void setVec4Array(const char* u, const float* data, int count) { GLint l = loc(u); if (l >= 0) glUniform4fv(l, count, data); }
private:
    std::unordered_map<std::string, GLint> cache;
};

class Mesh {
public:
    GLuint vao = 0, vbo = 0, ibo = 0, instVbo = 0;
    int indexCount = 0, vertexCount = 0, instanceCount = 0, instanceCapacity = 0;
    bool index32 = false;
    AABB bounds;
    void upload(const std::vector<Vertex>& v, const std::vector<uint32_t>& idx, bool dynamic = false);
    void updateVertices(const std::vector<Vertex>& v);  // for dynamic meshes (cape, trails)
    void setInstances(const std::vector<InstanceData>& inst);
    void draw() const;
    void destroy();
    bool valid() const { return vao != 0 && indexCount > 0; }
    size_t gpuBytes() const { return (size_t)vertexCount * sizeof(Vertex) + (size_t)indexCount * (index32 ? 4 : 2) + (size_t)instanceCapacity * sizeof(InstanceData); }
private:
    bool dynamic_ = false;
};

struct Texture {
    GLuint id = 0;
    int w = 0, h = 0, layers = 1;
    GLenum target = GL_TEXTURE_2D;
    void create2D(int w, int h, GLenum internalFmt, GLenum fmt, GLenum type, const void* data, bool mips, bool repeat, bool linear = true);
    void createArray(int w, int h, int layers, const std::vector<std::vector<uint8_t>>& rgbaLayers, bool repeat);
    void bind(int unit) const { glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(target, id); }
    void destroy() { if (id) glDeleteTextures(1, &id); id = 0; }
};

struct RenderTarget {
    GLuint fbo = 0;
    Texture color, depth;
    int w = 0, h = 0;
    bool create(int w, int h, GLenum colorFmt, bool withDepth, bool depthCompare = false, bool colorOnly = true);
    void destroy();
    void bind() const;
};

// Run-time capability detection.
struct GpuCaps {
    bool floatColorBuffer = false;
    bool halfFloatColorBuffer = false;
    int maxTextureSize = 2048;
    std::string renderer, version;
    void detect();
};
extern GpuCaps gGpu;

void glCheck(const char* where);

}  // namespace agr
