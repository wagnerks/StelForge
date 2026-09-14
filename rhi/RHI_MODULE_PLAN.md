# Extracting the RHI into its own module

(This file moved with the module: engine/rhi/RHI_MODULE_PLAN.md. Paths below written before the move
say src/renderModule/.)

Goal: the RHI (interface + OpenGL and Vulkan backends) builds as its own CMake target that depends on
no engine code. The engine, the editor and `rhi_tests` link it; `rhi_tests` links only it.

Done when: `sfe_rhi` builds alone; nothing under the RHI directory includes `core/`, `assetsModule/`,
`windowsModule/`, `systemsModule/` or `multithreading/`; `rhi_tests` links `sfe_rhi` + gtest only;
editor and runtime build; `rhi_tests` is green; `compare-backends.ps1` shows no new divergence.

After every section: build `rhi_tests`, `StelForgeRuntime`, `StellarForgeEditor`, run `rhi_tests`.

## 1. Remove what is already dead

- [x] Drop the unused `#include <core/Engine.h>` from `OpenGLRHI.cpp`, `VulkanRHI.cpp`,
      `VulkanRHIPipelines.cpp`, `VulkanRHIResources.cpp` (no `Engine` symbol is used there)
- [x] Drop `windowsModule/Window.h` from the backends once section 3 lands
- [x] Delete the dead Vulkan tutorial code in `core/ECSHandler.cpp` (~63-169: debug messenger,
      `vkDeviceWaitIdle`) if it is unreferenced, so no raw Vulkan remains outside the backend
- [x] Remove the commented GL query calls in `RenderSystem.cpp` (~204, ~701)

## 2. Shader loading goes through the host, not the asset system

Today `RenderHardwareInterface::loadShader` reads files through `ShaderBase::loadShaderCode`
(`RenderHardwareInterface.cpp:13`), and Vulkan's `SpirvCompiler` uses `Engine::instance()`,
`FileSystem::resolve`, `ShaderBase` and a hardcoded `shaders/vulkan/*.spv` layout.

- [x] Add to `RhiHost`: `std::string loadShaderSource(std::string_view path)` (include-expanded GLSL)
      and `std::filesystem::path shaderCacheDir()` (where SPIR-V is cached)
- [x] `loadShader` in the interface calls the host instead of `ShaderBase`
- [x] `SpirvCompiler` takes source text, a cache directory and a staleness key as parameters; no
      `Engine`, `FileSystem`, `ShaderBase` or window access inside it
- [x] Decide what `SpirvCompiler` reports instead of reaching for the window (its progress UI):
      a callback on the host, or nothing
- [x] Engine host implements both with today's `ShaderBase` / `FileSystem` code; the test host with
      the tests' own reader (`engine/bin/shaders`)
- [x] Make a failed SPIR-V compile an error instead of silently loading the stale `.spv`
      (`VulkanRHIPipelines.cpp:19-21`), so both backends fail the same way

Done as: `RhiHost::loadShaderSource` / `shaderCacheDir` / `onShaderCompile(name, finished)` (the engine
only logs -- retitling the window mid-frame crashed in ImGui's GLFW hook). Both backends compile
`ShaderModuleDesc::source`. `compileSpirv` caches `<basename>.spv` keyed by a hash of the expanded source
(`.spv.hash` sidecar), so an include change rebuilds too; startup batch precompile is gone, shaders
compile when first loaded. The present blit's shaders are embedded in the Vulkan backend.

## 3. No engine `Window` in the RHI

`RhiHost::window()` returns the engine's `Window*`; backends call `getWindow()` (GLFW handle) and
`getScreenData().renderW/H/width/height`.

- [x] Replace `Window* window()` with what the backends actually use: a native surface handle
      (`GLFWwindow*` for now, behind a small `RhiSurface` struct), framebuffer size and render size
- [x] OpenGL: context creation/`glfwMakeContextCurrent` stays in the backend, fed from the surface
- [x] Vulkan: surface creation and swapchain extent from the surface struct
- [x] Engine host adapts `Window`; test host passes the hidden GLFW window it already creates

Done as: `RhiHost::surface()` returns `RhiSurface` (GLFW window, visibility, window / render /
backbuffer size). Vulkan no longer hooks `Window::onFramebufferResize`; it recreates the swapchain when
the surface's backbuffer size differs from the swapchain extent. The test host still builds its
surface from the engine `Window` it creates -- switching the fixture to a bare GLFW window is in 9.

## 4. Logging

- [x] Add a log sink to `RhiHost` (`log(level, message)`) or a tiny `rhi/Log.h` with a settable
      callback; replace `LogsModule::Logger` in the backends (82 uses in Vulkan alone)
- [x] Engine routes it to `LogsModule`; tests route it to stdout and can assert on errors

Done as: `renderModule/RhiLog.h` -- a process-wide sink (`setRhiLogSink`) and `RhiLog::info/warning/
error/fatal/check`; unset, messages go to stderr. A per-host sink was not used because free functions
(SPIR-V compiler, validation callback, static helpers) log without an RHI instance. The engine installs
the sink in `Engine::Engine`; `rhi_tests` captures messages and fails a test on any Vulkan validation
error (logged as Fatal). Validation warnings do not fail tests.

## 5. Math and small types

- [x] Check what `RenderHardwareInterface.h` needs from `mathModule/Forward.h`; replace with plain
      arrays/structs in the interface (e.g. clear colour as `float[4]`) or move the few types in
- [x] Keep `GraphicsBackend.h` with the module (it has no engine includes)

Done as: the interface only used `Math::Vec4` for clear colours. It now uses `RhiVec4` from
`renderModule/RhiTypes.h`. A host extends it ImGui-style: the compile definition `SFE_RHI_USER_CONFIG`
names a header that defines `SFE_RHI_VEC4_EXTRA`; the engine's `core/RhiConfig.h` adds an implicit
constructor from `Math::Vec4` and a conversion back. Without a config `RhiVec4` is four floats.

## 6. Take editor streaming out of the interface

`publishDisplay`, `editorViewNativeId`, `capturePublishedFrame`, `setPresentBlitSource`,
`presentSourceTexture`, `readbackPresentSource`, and `RhiHost::publishEditorDisplay` are the
editor/runtime frame stream, not hardware abstraction.

- [x] Rebuild the stream on generic calls the RHI already has (render target, `cmdBlit`,
      `readbackTexture`, `getNativeTextureId`) in an engine-side `FrameStream` class
- [x] Keep in the RHI only what cannot be expressed generically (e.g. present of a texture to the
      window); document each survivor
- [x] Move `FrameCapture.cpp/.h` (PNG writing, CRC) to the engine; it uses `Engine` and `FileSystem`
- [x] Update `RenderSystem`, `RuntimeBridge`, runtime `main.cpp` and the capability tests

Done as: `publishDisplay`, `editorViewNativeId` (unused), `capturePublishedFrame`, `readbackPresentSource`
and `RhiHost::publishEditorDisplay` are gone, with the OpenGL display slots, fences and stream PBO
targets and Vulkan's display textures and per-list inline display blit. `RuntimeBridge` streams by
`readbackTexture(presentSourceTexture())` plus a CPU bilinear downscale to 1280 wide, rows bottom-up as
before -- one path for both backends instead of two. `setPresentBlitSource`/`presentSourceTexture` stay:
putting a texture on the window is presentation, not streaming. `FrameCapture` reads through
`readbackTexture` and is engine code; it stays out of the module directory in 9. The streaming path has
not been exercised against a running editor yet.

## 7. GL loading context out of the thread pool

`ThreadPool::ensureLoadingContext` creates a shared GLFW window for GL uploads, and
`TextureHandler.cpp:88` calls `glFinish` — backend-specific code outside the backend.

- [x] Add an RHI call for "upload from a worker thread" (e.g. `RenderHardwareInterface::
      beginWorkerUpload()/endWorkerUpload()` or a `WorkerContext` object) that OpenGL implements with
      the shared context and Vulkan with a no-op or its own queue
- [x] `ThreadPool` and `TextureHandler` call that instead of GLFW/GL directly

Done as: `RenderHardwareInterface::runOnWorkerThread(work)` -- serialised; OpenGL creates a hidden shared
context in `init()` and makes it current around `work`, then `glFinish`; the default just runs `work`.
`ThreadPool` has no GLFW or GL left (`ensureLoadingContext` and the loading window are gone, the loading
pool runs plain tasks); `TextureHandler` wraps its worker uploads in the call. Pinned by
`TextureCreatedOnAWorkerThreadIsUsableOnTheMainThread`.

## 8. Backend construction

- [x] Add `std::unique_ptr<RenderHardwareInterface> createRhi(GraphicsBackend)` in the module;
      `core/Engine.cpp` stops including `OpenGLRHI.h` / `VulkanRHI.h` (~14-15, ~174-177)
- [x] `rhi_tests` fixture uses the same factory

Done as: `renderModule/RhiFactory.h`; only `RhiFactory.cpp` includes the backend headers.

## 9. Directory and CMake target

- [x] Move interface + backends + `SpirvCompiler` + `GraphicsBackend` + `VulkanRHIVkUtil.h` to
      `engine/src/rhi/` (public headers under `rhi/include/sfe/rhi/` if a clean include root is wanted)
- [x] `add_library(sfe_rhi STATIC ...)` with its own dependencies: glad, glfw, Vulkan, shaderc_combined,
      VMA; move those `find_package`/`target_link_libraries` from the engine target
- [x] Engine target links `sfe_rhi` (PUBLIC where engine headers expose RHI types)
- [x] `rhi_tests` links `sfe_rhi` + gtest only; its fixture stops including `windowsModule/Window.h`
      and `NativeLibraries` from the engine (stage DLLs for the new target instead)
- [x] Fix include paths in the engine (`renderModule/RenderHardwareInterface.h` → new path), and the
      mixed-case include `<RenderModule/RenderHardwareInterface.h>` in `OpenGLRHI.h`
- [x] Add a CI/script check that fails if `engine/src/rhi` includes anything outside itself

Done as: the module lives in `engine/rhi/` (not `engine/src/rhi/`), target `sfe_rhi` with its own
Vulkan / shaderc / VMA / glad / OpenGL / GLFW dependencies; the engine target links it PUBLIC and no
longer names Vulkan, shaderc or VMA itself. Includes are `rhi/<File>.h` everywhere. The isolation check is
the include root rather than a script: `sfe_rhi` sees `engine/`, where `core/`, `renderModule/` and the
other engine modules do not resolve, so an engine include in the module is a compile error (verified by
adding one). `rhi_tests` links `sfe_rhi`, gtest and the CRT only; it still reads the header-only math types
(engine/src on its include path, `SFE_RHI_USER_CONFIG="core/RhiConfig.h"`) for its push structs, creates
its windows with bare GLFW, and carries its own copy of `Vertex3D`.

Note: the `SFE_RHI_VEC4_EXTRA` members exist in engine and test translation units but not in `sfe_rhi`'s
own. Like ImGui's `IM_VEC4_CLASS_EXTRA`, that is only safe while the extra members are inline functions
that add no data.

## 10. Close behaviour gaps the module should not ship with

From `engine/tests/FINDINGS.md`, not required for the split but part of "a complete interface":

- [ ] `TextureDimension::Tex3D` on Vulkan, or remove it from the interface
- [ ] Stencil load/store on Vulkan (`VulkanRHIPipelines.cpp:905-906`)
- [ ] `cmdBlit` honouring attachment layer/mip on Vulkan; `SamplerDesc` border colour
- [ ] `RenderPassBeginInfo` with zero size, and clear region vs render area: one meaning on both
- [ ] `getBackbufferFramebuffer` returning the same kind of handle on both backends
- [ ] Handle generation counters, so a stale handle never aliases a new resource

## Order

1 → 2 → 3 → 4 → 5 can land one by one with the engine unchanged in behaviour. 6 and 7 change engine
call sites. 8 and 9 are the actual split and are mechanical once 1-7 are done. 10 is independent.
