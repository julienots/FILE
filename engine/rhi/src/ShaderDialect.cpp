#include "agr/rhi/ShaderDialect.hpp"

#include <algorithm>
#include <regex>
#include <sstream>

namespace agr::rhi {
namespace {

u32 locationSlots(const std::string& type) {
    if (type == "mat2") return 2;
    if (type == "mat3") return 3;
    if (type == "mat4") return 4;
    return 1;
}

i32 indexOf(const std::vector<std::string>& list, const std::string& name) {
    const auto it = std::find(list.begin(), list.end(), name);
    return it == list.end() ? -1 : static_cast<i32>(it - list.begin());
}

} // namespace

Result<std::string> translateForVulkan(const std::string& source, ShaderStage stage, const PipelineDesc& desc,
                                       std::unordered_map<std::string, u32>& varyings) {
    static const std::regex kBlock(R"(^(\s*)layout\s*\(\s*std140\s*\)\s*uniform\s+(\w+)(.*)$)");
    static const std::regex kSampler(R"(^(\s*)uniform\s+(sampler2D)\s+(\w+)\s*;(.*)$)");
    static const std::regex kVarying(R"(^(\s*)((?:flat|smooth|noperspective)\s+)?(in|out)\s+(\w+)\s+(\w+)\s*;(.*)$)");
    static const std::regex kLooseUniform(R"(^\s*uniform\s+\w+\s+\w+.*$)");

    std::ostringstream out;
    out << "#version 450\n#define AGR_VULKAN 1\n#line 1\n";
    std::istringstream in(source);
    std::string line;
    u32 nextLocation = 0;
    u32 lineNo = 0;
    std::smatch m;
    while (std::getline(in, line)) {
        ++lineNo;
        if (std::regex_match(line, m, kBlock)) {
            const i32 slot = indexOf(desc.uniformBlocks, m[2]);
            if (slot < 0)
                return makeError(Error::Code::ParseError, "uniform block '" + m[2].str() + "' (line " + std::to_string(lineNo) +
                                                              ") is not listed in PipelineDesc::uniformBlocks");
            out << m[1] << "layout(std140, set = 0, binding = " << slot << ") uniform " << m[2] << m[3] << '\n';
            continue;
        }
        if (std::regex_match(line, m, kSampler)) {
            const i32 unit = indexOf(desc.samplers, m[3]);
            if (unit < 0)
                return makeError(Error::Code::ParseError, "sampler '" + m[3].str() + "' (line " + std::to_string(lineNo) +
                                                              ") is not listed in PipelineDesc::samplers");
            out << m[1] << "layout(set = 0, binding = " << (kTextureBindingBase + static_cast<u32>(unit)) << ") uniform "
                << m[2] << ' ' << m[3] << ';' << m[4] << '\n';
            continue;
        }
        if (std::regex_match(line, m, kLooseUniform))
            return makeError(Error::Code::ParseError, "line " + std::to_string(lineNo) +
                                                          ": plain uniforms are not allowed; use a std140 uniform block");
        if (std::regex_match(line, m, kVarying)) {
            const std::string dir = m[3], type = m[4], name = m[5];
            const bool isVarying = (stage == ShaderStage::Vertex && dir == "out") || (stage == ShaderStage::Fragment && dir == "in");
            if (isVarying) {
                u32 location;
                if (stage == ShaderStage::Vertex) {
                    location = nextLocation;
                    nextLocation += locationSlots(type);
                    varyings[name] = location;
                } else {
                    auto it = varyings.find(name);
                    if (it == varyings.end())
                        return makeError(Error::Code::ParseError, "fragment input '" + name + "' has no matching vertex output");
                    location = it->second;
                }
                out << m[1] << "layout(location = " << location << ") " << m[2] << dir << ' ' << type << ' ' << name << ';'
                    << m[6] << '\n';
                continue;
            }
        }
        out << line << '\n';
    }
    return out.str();
}

} // namespace agr::rhi
