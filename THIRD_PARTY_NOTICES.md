# Third-party software

AGR ENGINE is proprietary software of Superessence Studio. It uses the following
third-party components, fetched at configure time at pinned versions
(see `cmake/AGRDependencies.cmake`). Their licenses permit commercial, closed-source use.

| Component | Version | License | Used by |
|---|---|---|---|
| [SDL](https://github.com/libsdl-org/SDL) | 3.4.16 | zlib | `agr::platform` (window, input, touch, lifecycle) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | MIT | `agr::scene`, `agr::runtime` (scene & config files) |
| [stb_image / stb_image_write](https://github.com/nothings/stb) | 2.30 / 1.16 (commit `2c980bb`) | Public domain or MIT | `agr::assets` (image decoding, PNG screenshots) |
| [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) | vulkan-sdk-1.4.350.0 | Apache-2.0 OR MIT | `agr::rhi` Vulkan backend |
| [volk](https://github.com/zeux/volk) | vulkan-sdk-1.4.350.0 | MIT | Vulkan function loader |
| [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | 3.4.0 | MIT | Vulkan memory allocation |
| [glslang](https://github.com/KhronosGroup/glslang) | vulkan-sdk-1.4.350.0 | BSD-3-Clause / BSD-2-Clause / MIT / Apache-2.0 (per file) | GLSL ➜ SPIR-V compilation |
| [doctest](https://github.com/doctest/doctest) | 2.4.11 | MIT | unit tests only (not shipped) |

MIT-licensed components require their copyright notice to be included with
distributed binaries; the release packaging step (Phase 10) bundles their license texts.
