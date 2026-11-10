#include <mutex>

#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wshadow"
    #pragma GCC diagnostic ignored "-Wnull-dereference"
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #if !defined(__clang__)
        #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    #endif
#endif
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

#include "VkCommon.hpp"
#include "agr/rhi/ShaderDialect.hpp"

namespace agr::rhi::vk {
namespace {

void ensureGlslangInitialized() {
    static std::once_flag once;
    std::call_once(once, [] { glslang::InitializeProcess(); });
}

} // namespace

Result<SpirvPair> compileToSpirv(const PipelineDesc& desc) {
    ensureGlslangInitialized();
    std::unordered_map<std::string, u32> varyings;
    auto vsText = translateForVulkan(desc.shader.vertex, ShaderStage::Vertex, desc, varyings);
    if (!vsText) return makeError(vsText.error().code, std::string("vertex shader of '") + desc.debugName + "': " + vsText.error().message);
    auto fsText = translateForVulkan(desc.shader.fragment, ShaderStage::Fragment, desc, varyings);
    if (!fsText) return makeError(fsText.error().code, std::string("fragment shader of '") + desc.debugName + "': " + fsText.error().message);

    const auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
    glslang::TShader vs(EShLangVertex), fs(EShLangFragment);
    // glslang keeps a pointer to the string array until parse(): it must outlive setup().
    const char* vsSource = vsText.value().c_str();
    const char* fsSource = fsText.value().c_str();
    auto setup = [](glslang::TShader& sh, EShLanguage lang, const char* const* src) {
        sh.setStrings(src, 1);
        sh.setEntryPoint("main");
        sh.setSourceEntryPoint("main");
        sh.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 100);
        sh.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        sh.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
    };
    setup(vs, EShLangVertex, &vsSource);
    setup(fs, EShLangFragment, &fsSource);
    if (!vs.parse(GetDefaultResources(), 450, false, messages))
        return makeError(Error::Code::ParseError, std::string("vertex shader of '") + desc.debugName +
                                                      "' failed to compile:\n" + vs.getInfoLog());
    if (!fs.parse(GetDefaultResources(), 450, false, messages))
        return makeError(Error::Code::ParseError, std::string("fragment shader of '") + desc.debugName +
                                                      "' failed to compile:\n" + fs.getInfoLog());
    glslang::TProgram program;
    program.addShader(&vs);
    program.addShader(&fs);
    if (!program.link(messages))
        return makeError(Error::Code::ParseError, std::string("pipeline '") + desc.debugName + "' failed to link:\n" +
                                                      program.getInfoLog());
    SpirvPair out;
    glslang::SpvOptions options;
    options.generateDebugInfo = false;
    options.disableOptimizer = true;
    glslang::GlslangToSpv(*program.getIntermediate(EShLangVertex), out.vertex, &options);
    glslang::GlslangToSpv(*program.getIntermediate(EShLangFragment), out.fragment, &options);
    return out;
}

} // namespace agr::rhi::vk
