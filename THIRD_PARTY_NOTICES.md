# Third-party software

AGR ENGINE is proprietary software of Superessence Studio. It uses the following
third-party components, fetched at configure time at pinned versions
(see `cmake/AGRDependencies.cmake`). Their licenses permit commercial, closed-source use.

| Component | Version | License | Used by |
|---|---|---|---|
| [SDL](https://github.com/libsdl-org/SDL) | 3.4.16 | zlib | `agr::platform` (window, input, touch, lifecycle) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | MIT | `agr::scene`, `agr::runtime` (scene & config files) |
| [stb_image / stb_image_write](https://github.com/nothings/stb) | 2.30 / 1.16 (commit `2c980bb`) | Public domain or MIT | `agr::assets` (image decoding, PNG screenshots) |
| [cgltf](https://github.com/jkuhlmann/cgltf) | 1.15 | MIT | `agr::assets` (glTF 2.0 / GLB import) |
| [ufbx](https://github.com/ufbx/ufbx) | 0.23.1 | MIT or public domain | `agr::assets` (FBX import) |
| [Box2D](https://github.com/erincatto/box2d) | 3.1.1 | MIT | `agr::physics2d` (2D physics) |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | 5.6.0 | MIT | `agr::physics3d` (3D physics) |
| [Lua](https://www.lua.org) | 5.4.9 | MIT | `agr::script` (gameplay scripts) |
| [Recast & Detour](https://github.com/recastnavigation/recastnavigation) | 1.6.0 | zlib | `agr::navigation` (navigation meshes, path queries) |
| [miniaudio](https://miniaud.io) | 0.11.25 | Public domain (Unlicense) or MIT-0 | `agr::audio` (mixing, audio output, WAV / MP3 / FLAC decoding) |
| [stb_vorbis](https://github.com/nothings/stb) (bundled with miniaudio) | 1.22 | Public domain or MIT | `agr::audio` (Ogg Vorbis decoding) |
| [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) | vulkan-sdk-1.4.350.0 | Apache-2.0 OR MIT | `agr::rhi` Vulkan backend |
| [volk](https://github.com/zeux/volk) | vulkan-sdk-1.4.350.0 | MIT | Vulkan function loader |
| [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | 3.4.0 | MIT | Vulkan memory allocation |
| [glslang](https://github.com/KhronosGroup/glslang) | vulkan-sdk-1.4.350.0 | BSD-3-Clause / BSD-2-Clause / MIT / Apache-2.0 (per file) | GLSL ➜ SPIR-V compilation |
| [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) | 1.92.9 | MIT | editor UI |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | commit `18cef5e` | MIT | editor gizmos |
| [Inter](https://github.com/rsms/inter) | 4.1 | SIL Open Font License 1.1 | editor font (`editor/resources/fonts`, license included) |
| [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) | 2.304 | SIL Open Font License 1.1 | editor console font (license included) |
| [doctest](https://github.com/doctest/doctest) | 2.4.11 | MIT | unit tests only (not shipped) |

MIT-licensed components require their copyright notice to be included with
distributed binaries; the release packaging step (Phase 10) bundles their license texts.
