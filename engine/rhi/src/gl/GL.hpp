#pragma once
// Self-contained OpenGL 3.3 core / OpenGL ES 3.0 declarations and loader.
// Only the subset used by the AGR backend; no system GL headers needed, so the
// same file builds on Windows, Linux and Android.
#include <cstddef>
#include <cstdint>

#if defined(_WIN32) && !defined(_WIN64)
    #define AGR_GLAPI __stdcall
#else
    #define AGR_GLAPI
#endif

namespace agr::rhi::gl {

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLint = int;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLchar = char;
using GLubyte = unsigned char;
using GLintptr = std::ptrdiff_t;
using GLsizeiptr = std::ptrdiff_t;
using GLuint64 = std::uint64_t;

// clang-format off
constexpr GLenum GL_NO_ERROR = 0, GL_NONE = 0, GL_ZERO = 0, GL_ONE = 1;
constexpr GLboolean GL_FALSE = 0, GL_TRUE = 1;
constexpr GLbitfield GL_DEPTH_BUFFER_BIT = 0x100, GL_STENCIL_BUFFER_BIT = 0x400, GL_COLOR_BUFFER_BIT = 0x4000;
constexpr GLenum GL_LINES = 0x0001, GL_TRIANGLES = 0x0004;
constexpr GLenum GL_NEVER = 0x0200, GL_LESS = 0x0201, GL_EQUAL = 0x0202, GL_LEQUAL = 0x0203,
                 GL_GREATER = 0x0204, GL_GEQUAL = 0x0206, GL_ALWAYS = 0x0207;
constexpr GLenum GL_SRC_ALPHA = 0x0302, GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_FRONT = 0x0404, GL_BACK = 0x0405, GL_CCW = 0x0901;
constexpr GLenum GL_CULL_FACE = 0x0B44, GL_DEPTH_TEST = 0x0B71, GL_BLEND = 0x0BE2, GL_SCISSOR_TEST = 0x0C11;
constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5, GL_PACK_ALIGNMENT = 0x0D05, GL_MAX_TEXTURE_SIZE = 0x0D33;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401, GL_UNSIGNED_SHORT = 0x1403, GL_UNSIGNED_INT = 0x1405,
                 GL_FLOAT = 0x1406, GL_HALF_FLOAT = 0x140B;
constexpr GLenum GL_COLOR = 0x1800, GL_DEPTH = 0x1801, GL_STENCIL = 0x1802;
constexpr GLenum GL_DEPTH_COMPONENT = 0x1902, GL_RED = 0x1903, GL_RGBA = 0x1908;
constexpr GLenum GL_VENDOR = 0x1F00, GL_RENDERER = 0x1F01, GL_VERSION = 0x1F02, GL_EXTENSIONS = 0x1F03;
constexpr GLenum GL_NEAREST = 0x2600, GL_LINEAR = 0x2601, GL_NEAREST_MIPMAP_NEAREST = 0x2700,
                 GL_LINEAR_MIPMAP_NEAREST = 0x2701, GL_NEAREST_MIPMAP_LINEAR = 0x2702, GL_LINEAR_MIPMAP_LINEAR = 0x2703;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800, GL_TEXTURE_MIN_FILTER = 0x2801,
                 GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803,
                 GL_TEXTURE_MAX_LEVEL = 0x813D;
constexpr GLenum GL_REPEAT = 0x2901, GL_CLAMP_TO_EDGE = 0x812F, GL_MIRRORED_REPEAT = 0x8370;
constexpr GLenum GL_FUNC_ADD = 0x8006, GL_RGBA8 = 0x8058;
constexpr GLenum GL_DEPTH_COMPONENT16 = 0x81A5, GL_DEPTH_ATTACHMENT = 0x8D00;
constexpr GLenum GL_DEPTH_STENCIL_ATTACHMENT = 0x821A, GL_NUM_EXTENSIONS = 0x821D, GL_R8 = 0x8229;
constexpr GLenum GL_TEXTURE0 = 0x84C0, GL_DEPTH_STENCIL = 0x84F9, GL_UNSIGNED_INT_24_8 = 0x84FA;
constexpr GLenum GL_QUERY_RESULT = 0x8866, GL_QUERY_RESULT_AVAILABLE = 0x8867;
constexpr GLenum GL_RGBA16F = 0x881A;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892, GL_ELEMENT_ARRAY_BUFFER = 0x8893;
constexpr GLenum GL_STATIC_DRAW = 0x88E4, GL_DYNAMIC_DRAW = 0x88E8;
constexpr GLenum GL_TIME_ELAPSED = 0x88BF, GL_DEPTH24_STENCIL8 = 0x88F0;
constexpr GLenum GL_UNIFORM_BUFFER = 0x8A11, GL_MAX_UNIFORM_BLOCK_SIZE = 0x8A30, GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT = 0x8A34;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30, GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81, GL_LINK_STATUS = 0x8B82, GL_INFO_LOG_LENGTH = 0x8B84;
constexpr GLenum GL_SRGB8_ALPHA8 = 0x8C43;
constexpr GLenum GL_READ_FRAMEBUFFER = 0x8CA8, GL_DRAW_FRAMEBUFFER = 0x8CA9, GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0, GL_FRAMEBUFFER = 0x8D40, GL_RENDERBUFFER = 0x8D41, GL_MAX_SAMPLES = 0x8D57;
constexpr GLenum GL_GPU_DISJOINT_EXT = 0x8FBB;
constexpr GLuint GL_INVALID_INDEX = 0xFFFFFFFFu;
// clang-format on

// X(return type, name, parameter list)
#define AGR_GL_REQUIRED(X)                                                                                  \
    X(const GLubyte*, glGetString, (GLenum))                                                                \
    X(const GLubyte*, glGetStringi, (GLenum, GLuint))                                                       \
    X(void, glGetIntegerv, (GLenum, GLint*))                                                                \
    X(GLenum, glGetError, ())                                                                               \
    X(void, glEnable, (GLenum))                                                                             \
    X(void, glDisable, (GLenum))                                                                            \
    X(void, glBlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum))                                          \
    X(void, glBlendEquation, (GLenum))                                                                      \
    X(void, glDepthFunc, (GLenum))                                                                          \
    X(void, glDepthMask, (GLboolean))                                                                       \
    X(void, glColorMask, (GLboolean, GLboolean, GLboolean, GLboolean))                                      \
    X(void, glCullFace, (GLenum))                                                                           \
    X(void, glFrontFace, (GLenum))                                                                          \
    X(void, glViewport, (GLint, GLint, GLsizei, GLsizei))                                                   \
    X(void, glScissor, (GLint, GLint, GLsizei, GLsizei))                                                    \
    X(void, glClearBufferfv, (GLenum, GLint, const GLfloat*))                                               \
    X(void, glClearBufferfi, (GLenum, GLint, GLfloat, GLint))                                               \
    X(void, glGenBuffers, (GLsizei, GLuint*))                                                               \
    X(void, glDeleteBuffers, (GLsizei, const GLuint*))                                                      \
    X(void, glBindBuffer, (GLenum, GLuint))                                                                 \
    X(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum))                                        \
    X(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void*))                                   \
    X(void, glBindBufferRange, (GLenum, GLuint, GLuint, GLintptr, GLsizeiptr))                              \
    X(void, glGenVertexArrays, (GLsizei, GLuint*))                                                          \
    X(void, glDeleteVertexArrays, (GLsizei, const GLuint*))                                                 \
    X(void, glBindVertexArray, (GLuint))                                                                    \
    X(void, glEnableVertexAttribArray, (GLuint))                                                            \
    X(void, glDisableVertexAttribArray, (GLuint))                                                           \
    X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))                \
    X(void, glGenTextures, (GLsizei, GLuint*))                                                              \
    X(void, glDeleteTextures, (GLsizei, const GLuint*))                                                     \
    X(void, glBindTexture, (GLenum, GLuint))                                                                \
    X(void, glActiveTexture, (GLenum))                                                                      \
    X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*))     \
    X(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*))  \
    X(void, glTexParameteri, (GLenum, GLenum, GLint))                                                       \
    X(void, glGenerateMipmap, (GLenum))                                                                     \
    X(void, glPixelStorei, (GLenum, GLint))                                                                 \
    X(GLuint, glCreateShader, (GLenum))                                                                     \
    X(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*))                          \
    X(void, glCompileShader, (GLuint))                                                                      \
    X(void, glGetShaderiv, (GLuint, GLenum, GLint*))                                                        \
    X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))                                       \
    X(void, glDeleteShader, (GLuint))                                                                       \
    X(GLuint, glCreateProgram, ())                                                                          \
    X(void, glAttachShader, (GLuint, GLuint))                                                               \
    X(void, glDetachShader, (GLuint, GLuint))                                                               \
    X(void, glLinkProgram, (GLuint))                                                                        \
    X(void, glGetProgramiv, (GLuint, GLenum, GLint*))                                                       \
    X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))                                      \
    X(void, glDeleteProgram, (GLuint))                                                                      \
    X(void, glUseProgram, (GLuint))                                                                         \
    X(GLuint, glGetUniformBlockIndex, (GLuint, const GLchar*))                                              \
    X(void, glUniformBlockBinding, (GLuint, GLuint, GLuint))                                                \
    X(GLint, glGetUniformLocation, (GLuint, const GLchar*))                                                 \
    X(void, glUniform1i, (GLint, GLint))                                                                    \
    X(void, glDrawArrays, (GLenum, GLint, GLsizei))                                                         \
    X(void, glDrawElements, (GLenum, GLsizei, GLenum, const void*))                                         \
    X(void, glDrawElementsInstanced, (GLenum, GLsizei, GLenum, const void*, GLsizei))                       \
    X(void, glVertexAttribDivisor, (GLuint, GLuint))                                                        \
    X(void, glDrawBuffers, (GLsizei, const GLenum*))                                                        \
    X(void, glGenFramebuffers, (GLsizei, GLuint*))                                                          \
    X(void, glDeleteFramebuffers, (GLsizei, const GLuint*))                                                 \
    X(void, glBindFramebuffer, (GLenum, GLuint))                                                            \
    X(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))                                \
    X(void, glFramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint))                                    \
    X(GLenum, glCheckFramebufferStatus, (GLenum))                                                           \
    X(void, glGenRenderbuffers, (GLsizei, GLuint*))                                                         \
    X(void, glDeleteRenderbuffers, (GLsizei, const GLuint*))                                                \
    X(void, glBindRenderbuffer, (GLenum, GLuint))                                                           \
    X(void, glRenderbufferStorageMultisample, (GLenum, GLsizei, GLenum, GLsizei, GLsizei))                  \
    X(void, glBlitFramebuffer, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
    X(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*))                          \
    X(void, glReadBuffer, (GLenum))                                                                         \
    X(void, glGenQueries, (GLsizei, GLuint*))                                                               \
    X(void, glDeleteQueries, (GLsizei, const GLuint*))                                                      \
    X(void, glBeginQuery, (GLenum, GLuint))                                                                 \
    X(void, glEndQuery, (GLenum))                                                                           \
    X(void, glGetQueryObjectuiv, (GLuint, GLenum, GLuint*))                                                 \
    X(void, glFlush, ())

// Optional: missing on some drivers / API versions. Always null-checked.
#define AGR_GL_OPTIONAL(X)                                                                                  \
    X(void, glInvalidateFramebuffer, (GLenum, GLsizei, const GLenum*))                                      \
    X(void, glGetQueryObjectui64v, (GLuint, GLenum, GLuint64*))

struct Functions {
#define AGR_GL_MEMBER(ret, name, args) ret(AGR_GLAPI* name) args = nullptr;
    AGR_GL_REQUIRED(AGR_GL_MEMBER)
    AGR_GL_OPTIONAL(AGR_GL_MEMBER)
#undef AGR_GL_MEMBER
};

} // namespace agr::rhi::gl
