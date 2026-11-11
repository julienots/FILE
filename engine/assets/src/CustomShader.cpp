#include "agr/assets/CustomShader.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <format>
#include <regex>
#include <unordered_set>

namespace agr {

namespace {

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

/// Names a parameter cannot take: GLSL keywords, common built-in functions (a `#define` would
/// break every call to them) and the names of the engine's own hook API.
const std::unordered_set<std::string_view>& reservedNames() {
    static const std::unordered_set<std::string_view> names = {
        // keywords and types
        "attribute", "const", "uniform", "varying", "layout", "centroid", "flat", "smooth", "break", "continue", "do",
        "for", "while", "switch", "case", "default", "if", "else", "in", "out", "inout", "float", "int", "void", "bool",
        "true", "false", "invariant", "discard", "return", "mat2", "mat3", "mat4", "vec2", "vec3", "vec4", "ivec2",
        "ivec3", "ivec4", "bvec2", "bvec3", "bvec4", "uint", "uvec2", "uvec3", "uvec4", "lowp", "mediump", "highp",
        "precision", "sampler2D", "samplerCube", "struct", "color", "texture2D",
        // built-in functions
        "radians", "degrees", "sin", "cos", "tan", "asin", "acos", "atan", "pow", "exp", "log", "exp2", "log2",
        "sqrt", "inversesqrt", "abs", "sign", "floor", "ceil", "fract", "mod", "min", "max", "clamp", "mix", "step",
        "smoothstep", "length", "distance", "dot", "cross", "normalize", "reflect", "refract", "texture", "textureLod",
        "dFdx", "dFdy", "fwidth", "round", "trunc", "transpose", "inverse", "faceforward", "matrixCompMult",
        // engine API
        "main", "surface", "vertex", "Surface", "Vertex", "TIME", "param", "range", "srgb"};
    return names;
}

bool reservedPrefix(std::string_view name) {
    for (std::string_view p : {"gl_", "u_", "a_", "v_", "i_", "o_", "agr", "AGR", "__"})
        if (name.starts_with(p)) return true;
    return false;
}

/// Same text with comments replaced by spaces (newlines kept): offsets match the original.
std::string stripComments(std::string_view code) {
    std::string out(code);
    usize i = 0;
    while (i < out.size()) {
        if (out[i] == '/' && i + 1 < out.size() && out[i + 1] == '/') {
            while (i < out.size() && out[i] != '\n') out[i++] = ' ';
        } else if (out[i] == '/' && i + 1 < out.size() && out[i + 1] == '*') {
            out[i] = out[i + 1] = ' ';
            i += 2;
            while (i < out.size() && !(out[i] == '*' && i + 1 < out.size() && out[i + 1] == '/')) {
                if (out[i] != '\n') out[i] = ' ';
                ++i;
            }
            if (i < out.size()) out[i++] = ' ';
            if (i < out.size()) out[i++] = ' ';
        } else {
            ++i;
        }
    }
    return out;
}

struct FunctionRange {
    usize begin = 0, end = 0;  // [begin, end): from `void` to the closing brace
};

/// Finds the definition of the top-level function `void name(...) { ... }` (not a prototype).
bool findFunction(const std::string& clean, std::string_view name, FunctionRange& out) {
    int depth = 0;
    for (usize i = 0; i < clean.size(); ++i) {
        const char c = clean[i];
        if (c == '{') ++depth;
        else if (c == '}') --depth;
        if (depth != 0 || c != 'v' || clean.compare(i, 4, "void") != 0) continue;
        if (i > 0 && isIdentChar(clean[i - 1])) continue;
        usize j = i + 4;
        if (j >= clean.size() || !std::isspace(static_cast<unsigned char>(clean[j]))) continue;
        while (j < clean.size() && std::isspace(static_cast<unsigned char>(clean[j]))) ++j;
        if (clean.compare(j, name.size(), name) != 0) continue;
        j += name.size();
        if (j < clean.size() && isIdentChar(clean[j])) continue;
        while (j < clean.size() && std::isspace(static_cast<unsigned char>(clean[j]))) ++j;
        if (j >= clean.size() || clean[j] != '(') continue;
        const usize close = clean.find(')', j);
        if (close == std::string::npos) return false;
        usize k = close + 1;
        while (k < clean.size() && std::isspace(static_cast<unsigned char>(clean[k]))) ++k;
        if (k >= clean.size() || clean[k] != '{') continue;  // prototype
        int d = 0;
        for (; k < clean.size(); ++k) {
            if (clean[k] == '{') ++d;
            else if (clean[k] == '}' && --d == 0) break;
        }
        if (k >= clean.size()) return false;  // unbalanced: the compiler will say where
        out = {i, k + 1};
        return true;
    }
    return false;
}

class LineParser {
public:
    explicit LineParser(std::string_view s) : m_s(s) {}
    void skipSpace() {
        while (m_i < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i;
    }
    bool eof() {
        skipSpace();
        return m_i >= m_s.size();
    }
    bool accept(char c) {
        skipSpace();
        if (m_i < m_s.size() && m_s[m_i] == c) {
            ++m_i;
            return true;
        }
        return false;
    }
    std::string_view identifier() {
        skipSpace();
        const usize start = m_i;
        if (m_i < m_s.size() && isIdentStart(m_s[m_i]))
            while (m_i < m_s.size() && isIdentChar(m_s[m_i])) ++m_i;
        return m_s.substr(start, m_i - start);
    }
    bool number(float& out) {
        skipSpace();
        // [+-]digits[.digits][e[+-]digits], parsed with strtof on that token only (no "inf",
        // "nan" or hex). Not std::from_chars: Android's libc++ has no floating-point overloads;
        // strtof uses the C locale, which the engine never changes ('.' decimal point).
        usize j = m_i;
        if (j < m_s.size() && (m_s[j] == '+' || m_s[j] == '-')) ++j;
        usize digits = 0;
        while (j < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[j]))) ++j, ++digits;
        if (j < m_s.size() && m_s[j] == '.') {
            ++j;
            while (j < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[j]))) ++j, ++digits;
        }
        if (digits == 0) return false;
        if (j < m_s.size() && (m_s[j] == 'e' || m_s[j] == 'E')) {
            usize k = j + 1;
            if (k < m_s.size() && (m_s[k] == '+' || m_s[k] == '-')) ++k;
            if (k < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[k]))) {
                while (k < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[k]))) ++k;
                j = k;
            }
        }
        char buf[64];
        const usize n = std::min<usize>(j - m_i, sizeof(buf) - 1);
        std::memcpy(buf, m_s.data() + m_i, n);
        buf[n] = '\0';
        out = std::strtof(buf, nullptr);
        m_i = j;
        if (m_i < m_s.size() && (m_s[m_i] == 'f' || m_s[m_i] == 'F')) ++m_i;  // 1.0f
        return true;
    }
    bool quoted(std::string& out) {
        skipSpace();
        if (m_i >= m_s.size() || m_s[m_i] != '"') return false;
        const usize close = m_s.find('"', m_i + 1);
        if (close == std::string_view::npos) return false;
        out = std::string(m_s.substr(m_i + 1, close - m_i - 1));
        m_i = close + 1;
        return true;
    }
    bool peekIdentifier(std::string_view word) {
        skipSpace();
        return m_s.compare(m_i, word.size(), word) == 0 && (m_i + word.size() >= m_s.size() || !isIdentChar(m_s[m_i + word.size()]));
    }

private:
    std::string_view m_s;
    usize m_i = 0;
};

u32 componentCount(ShaderParamType t) {
    switch (t) {
        case ShaderParamType::Float: return 1;
        case ShaderParamType::Vec2: return 2;
        case ShaderParamType::Vec3: return 3;
        case ShaderParamType::Vec4:
        case ShaderParamType::Color: return 4;
        case ShaderParamType::Texture: return 0;
    }
    return 0;
}

/// Parses the text after `param`. Returns an error message, empty on success.
std::string parseParam(std::string_view text, ShaderParam& p) {
    LineParser in(text);
    const std::string_view type = in.identifier();
    if (type == "float") p.type = ShaderParamType::Float;
    else if (type == "vec2") p.type = ShaderParamType::Vec2;
    else if (type == "vec3") p.type = ShaderParamType::Vec3;
    else if (type == "vec4") p.type = ShaderParamType::Vec4;
    else if (type == "color") p.type = ShaderParamType::Color;
    else if (type == "texture") p.type = ShaderParamType::Texture;
    else return std::format("unknown parameter type '{}' (float, vec2, vec3, vec4, color, texture)", type);
    const std::string_view name = in.identifier();
    if (name.empty()) return "parameter name expected";
    p.name = std::string(name);
    if (p.type == ShaderParamType::Color) p.defaultValue = {1.0f, 1.0f, 1.0f, 1.0f};
    if (in.accept('=')) {
        if (p.type == ShaderParamType::Texture) {
            if (!in.quoted(p.defaultTexture)) return "texture default must be a quoted asset path, e.g. \"assets://noise.png\"";
        } else {
            // number, (a, b, ...) or vecN(a, b, ...)
            const std::string_view ctor = in.identifier();
            if (!ctor.empty() && ctor != "vec2" && ctor != "vec3" && ctor != "vec4") return std::format("unexpected '{}' in the default value", ctor);
            float values[4]{};
            u32 count = 0;
            if (in.accept('(')) {
                do {
                    if (count == 4 || !in.number(values[count])) return "number expected in the default value";
                    ++count;
                } while (in.accept(','));
                if (!in.accept(')')) return "')' expected after the default value";
            } else if (!ctor.empty() || !in.number(values[count++])) {
                return "default value expected (a number or (x, y, ...))";
            }
            const u32 n = componentCount(p.type);
            if (count == 1) {
                for (u32 i = 0; i < 4; ++i) (&p.defaultValue.x)[i] = i < n || p.type != ShaderParamType::Color ? values[0] : 1.0f;
                if (p.type == ShaderParamType::Color) p.defaultValue.w = 1.0f;
            } else if (count == n || (p.type == ShaderParamType::Color && count == 3)) {
                for (u32 i = 0; i < count; ++i) (&p.defaultValue.x)[i] = values[i];
            } else {
                return std::format("{} values given, {} expected", count, n);
            }
        }
    }
    if (p.type == ShaderParamType::Texture && in.peekIdentifier("srgb")) {
        in.identifier();
        p.srgb = true;
    }
    if (in.peekIdentifier("range")) {
        in.identifier();
        if (p.type != ShaderParamType::Float) return "range() applies to float parameters only";
        if (!in.accept('(') || !in.number(p.rangeMin) || !in.accept(',') || !in.number(p.rangeMax) || !in.accept(')'))
            return "range(min, max) expected";
        if (!(p.rangeMax > p.rangeMin)) return "range: max must be greater than min";
    }
    in.accept(';');
    if (!in.eof()) return "unexpected text after the parameter (one parameter per line)";
    return {};
}

} // namespace

const ShaderParam* CustomShader::find(std::string_view name) const {
    for (const ShaderParam& p : params)
        if (p.name == name) return &p;
    return nullptr;
}

CustomShader parseCustomShader(std::string_view source) {
    CustomShader out;
    out.code.reserve(source.size());
    u32 lineNumber = 0;
    usize pos = 0;
    while (pos <= source.size()) {
        const usize nl = source.find('\n', pos);
        const usize end = nl == std::string_view::npos ? source.size() : nl;
        std::string_view line = source.substr(pos, end - pos);
        ++lineNumber;
        const std::string_view t = trim(line);
        if (t.starts_with("param") && t.size() > 5 && std::isspace(static_cast<unsigned char>(t[5]))) {
            ShaderParam p;
            p.line = lineNumber;
            std::string body(t.substr(5));
            bool quoted = false;  // strip a trailing comment ("//" inside a quoted path is kept)
            for (usize c = 0; c + 1 < body.size(); ++c) {
                if (body[c] == '"') quoted = !quoted;
                if (!quoted && body[c] == '/' && body[c + 1] == '/') {
                    body.resize(c);
                    break;
                }
            }
            if (std::string err = parseParam(body, p); !err.empty()) {
                out.errors.push_back({lineNumber, err});
            } else if (!isIdentStart(p.name[0]) || reservedNames().contains(p.name) || reservedPrefix(p.name)) {
                out.errors.push_back({lineNumber, std::format("'{}' is reserved: choose another parameter name", p.name)});
            } else if (out.find(p.name)) {
                out.errors.push_back({lineNumber, std::format("parameter '{}' is declared twice", p.name)});
            } else if (p.type == ShaderParamType::Texture) {
                if (out.textureCount == kMaxShaderTextures)
                    out.errors.push_back({lineNumber, std::format("at most {} texture parameters", kMaxShaderTextures)});
                p.slot = out.textureCount++;
                out.params.push_back(std::move(p));
            } else {
                if (out.vectorCount == kMaxShaderVectors)
                    out.errors.push_back({lineNumber, std::format("at most {} value parameters", kMaxShaderVectors)});
                p.slot = out.vectorCount++;
                out.params.push_back(std::move(p));
            }
            line = {};  // blanked: the GLSL compiler never sees it
        }
        out.code.append(line);
        if (nl == std::string_view::npos) break;
        out.code.push_back('\n');
        pos = nl + 1;
    }

    const std::string clean = stripComments(out.code);
    FunctionRange r;
    out.hasSurface = findFunction(clean, "surface", r);
    out.hasVertex = findFunction(clean, "vertex", r);
    if (!out.hasSurface && !out.hasVertex)
        out.errors.push_back({0, "no 'void surface(inout Surface s)' or 'void vertex(inout Vertex v)' function"});

    // Global declarations the engine generates itself.
    int depth = 0;
    lineNumber = 1;
    bool lineStart = true;
    for (usize i = 0; i < clean.size(); ++i) {
        const char c = clean[i];
        if (c == '\n') {
            ++lineNumber;
            lineStart = true;
            continue;
        }
        if (lineStart && depth == 0 && !std::isspace(static_cast<unsigned char>(c))) {
            const std::string_view rest(clean.data() + i, std::min<usize>(16, clean.size() - i));
            for (std::string_view word : {"uniform", "layout", "in", "out", "varying", "attribute", "precision", "#version"}) {
                if (rest.starts_with(word) && (rest.size() == word.size() || !isIdentChar(rest[word.size()]))) {
                    out.errors.push_back({lineNumber, std::format("'{}' declarations are generated by the engine: use 'param' for inputs", word)});
                    break;
                }
            }
        }
        if (!std::isspace(static_cast<unsigned char>(c))) lineStart = false;
        if (c == '{') ++depth;
        else if (c == '}') --depth;
    }
    std::stable_sort(out.errors.begin(), out.errors.end(), [](const ShaderDiagnostic& a, const ShaderDiagnostic& b) { return a.line < b.line; });
    return out;
}

std::string blankFunction(std::string_view code, std::string_view name) {
    std::string out(code);
    FunctionRange r;
    if (!findFunction(stripComments(code), name, r)) return out;
    for (usize i = r.begin; i < r.end; ++i)
        if (out[i] != '\n') out[i] = ' ';
    return out;
}

std::string shaderParamDefines(const CustomShader& shader, bool textures) {
    std::string out;
    for (const ShaderParam& p : shader.params) {
        switch (p.type) {
            case ShaderParamType::Float: out += std::format("#define {} (u_custom[{}].x)\n", p.name, p.slot); break;
            case ShaderParamType::Vec2: out += std::format("#define {} (u_custom[{}].xy)\n", p.name, p.slot); break;
            case ShaderParamType::Vec3: out += std::format("#define {} (u_custom[{}].xyz)\n", p.name, p.slot); break;
            case ShaderParamType::Vec4:
            case ShaderParamType::Color: out += std::format("#define {} (u_custom[{}])\n", p.name, p.slot); break;
            case ShaderParamType::Texture:
                if (textures) out += std::format("#define {} u_customMap{}\n", p.name, p.slot);
                break;
        }
    }
    return out;
}

std::vector<ShaderDiagnostic> parseShaderLog(std::string_view log) {
    // Mesa:    "1:12(5): error: ..."      glslang: "ERROR: 1:12: '...' : ..."
    // NVIDIA:  "1(12) : error C1008: ..."
    static const std::regex mesa(R"((\d+):(\d+)\(\d+\):\s*(.*))");
    static const std::regex glslang(R"((?:ERROR|WARNING):\s*(\d+):(\d+):\s*(.*))");
    static const std::regex nvidia(R"((\d+)\((\d+)\)\s*:\s*(.*))");
    std::vector<ShaderDiagnostic> out;
    usize pos = 0;
    while (pos < log.size()) {
        usize nl = log.find('\n', pos);
        if (nl == std::string_view::npos) nl = log.size();
        const std::string line(trim(log.substr(pos, nl - pos)));
        pos = nl + 1;
        if (line.empty() || line.find("compilation errors") != std::string::npos || line.find("compilation terminated") != std::string::npos)
            continue;
        std::smatch m;
        ShaderDiagnostic d;
        if (std::regex_search(line, m, glslang) || std::regex_search(line, m, mesa) || std::regex_search(line, m, nvidia)) {
            d.line = m[1].str() == "1" ? static_cast<u32>(std::strtoul(m[2].str().c_str(), nullptr, 10)) : 0;
            d.message = std::string(trim(m[3].str()));
            if (d.message.starts_with("error:")) d.message = std::string(trim(d.message.substr(6)));
            if (d.line == 0) d.message = "(engine code) " + d.message;
        } else if (line.find("rror") != std::string::npos) {
            d.message = line;
        } else {
            continue;
        }
        if (std::find(out.begin(), out.end(), d) == out.end()) out.push_back(std::move(d));
    }
    return out;
}

std::string defaultCustomShaderSource() {
    return R"(// Shader de surface AGR (.agrshader) : du GLSL branché sur le rendu PBR.
// Les lignes « param » deviennent des réglages dans l'inspecteur des matériaux qui l'utilisent.
param color tint = (1.0, 0.55, 0.25, 1.0);
param float speed = 2.0 range(0, 10);
param float glow = 0.6 range(0, 4);

// Facultatif : déplace les sommets (espace monde) ; aussi utilisé pour les ombres.
void vertex(inout Vertex v) {
    v.position += v.normal * sin(TIME * speed + v.position.y * 6.0) * 0.02;
}

// Modifie la surface avant l'éclairage : baseColor, alpha, metallic, roughness, normal,
// emissive, occlusion. Lecture : worldPos, viewDir, uv, color (couleur du sommet / de l'objet).
void surface(inout Surface s) {
    float bands = 0.5 + 0.5 * sin(s.worldPos.y * 12.0 - TIME * speed);
    s.baseColor *= tint.rgb;
    s.emissive += tint.rgb * bands * glow;
    s.roughness = mix(s.roughness, 0.2, bands);
}
)";
}

} // namespace agr
