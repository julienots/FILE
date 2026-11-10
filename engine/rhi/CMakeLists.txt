agr_add_module(rhi
    SOURCES
        src/Rhi.cpp
        src/ShaderDialect.cpp
        src/gl/GLDevice.cpp
    PUBLIC_DEPS
        agr::core
        agr::math)

if(AGR_ENABLE_VULKAN)
    target_sources(agr_rhi PRIVATE
        src/vk/VkDevice.cpp
        src/vk/VkShaderCompiler.cpp
        src/vk/VkThirdParty.cpp)
    target_compile_definitions(agr_rhi PRIVATE AGR_RHI_VULKAN=1 VK_NO_PROTOTYPES=1)
    target_include_directories(agr_rhi SYSTEM PRIVATE
        ${volk_SOURCE_DIR}
        ${vulkanmemoryallocator_SOURCE_DIR}/include)
    target_link_libraries(agr_rhi PRIVATE Vulkan::Headers glslang::glslang glslang::glslang-default-resource-limits)
endif()
