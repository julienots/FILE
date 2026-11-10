# Third-party software

AGR ENGINE is proprietary software of Superessence Studio. It uses the following
third-party components, fetched at configure time at pinned versions
(see `cmake/AGRDependencies.cmake`). Their licenses permit commercial, closed-source use.

| Component | Version | License | Used by |
|---|---|---|---|
| [SDL](https://github.com/libsdl-org/SDL) | 3.4.16 | zlib | `agr::platform` (window, input, touch, lifecycle) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | MIT | `agr::scene`, `agr::runtime` (scene & config files) |
| [stb_image / stb_image_write](https://github.com/nothings/stb) | 2.30 / 1.16 (commit `2c980bb`) | Public domain or MIT | `agr::assets` (image decoding, PNG screenshots) |
| [doctest](https://github.com/doctest/doctest) | 2.4.11 | MIT | unit tests only (not shipped) |

MIT-licensed components require their copyright notice to be included with
distributed binaries; the release packaging step (Phase 10) bundles their license texts.
