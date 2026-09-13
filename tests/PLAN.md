# RHI test coverage plan

Goal: walk `RenderHardwareInterface` method by method and pin every one of them with a test that
runs on both backends. One expectation, written once, checked twice.

Run the suite with:

```
ctest --test-dir build/tests -C RelWithDebInfo -R rhi_tests --output-on-failure
```

## Ground rules

- **One agent per checkbox.** Agents that own the same file run one at a time; different files run
  in parallel.
- **Nobody edits `RhiTestFixture.h`.** Local helpers go in an anonymous namespace inside the
  agent's own file. Consolidation into the fixture happens later, in one pass, by hand.
- **Shaders**: GLSL under `engine/bin/shaders/`, named `rhi_test_<thing>.<vs|fs|cs>`, including
  `sfe.glsl` for the `SFE_SET` / `SFE_PUSH_BLOCK` / `SFE_VERTEX_ID` macros. Vulkan compiles them to
  `shaders/vulkan/*.spv` on load whenever the source is newer, so there is nothing to add to
  `scripts/compile_vulkan_shaders.bat`.
- **Pin what the backends do, not what they should do.** A divergence between OpenGL and Vulkan is
  a finding to report, not something to paper over with a per-backend `if`. Say so and stop.
- `GTEST_SKIP()` is for a capability a backend genuinely does not have, with the reason spelled out.

## Already covered

`setHost` `init` `cleanup` `waitIdle` `beginFrame` `present` `submit` `beginCommandList`
`endCommandList` `cmdBeginRenderPass` `cmdEndRenderPass` `createRenderPass` `destroyRenderPass`
`createFramebuffer` `destroyFramebuffer` `createTexture` `destroyTexture` `readbackTexture`
`createBuffer` `updateBuffer` `mapBuffer` `unmapBuffer` `destroyBuffer` `createSampler`
`destroySampler` `createDescriptorSetLayout` `destroyDescriptorSetLayout` `allocateDescriptorSet`
`freeDescriptorSet` `updateDescriptorSets` `createGraphicsPipeline` `loadShader` `cmdBindPipeline`
`cmdPushConstants` `cmdDraw` `frameInFlight` `framesInFlight`, plus winding, clip-space Y and the
depth range.

## 1. Geometry input — `RhiDrawTests.cpp`

- [x] `cmdBindVertexBuffer` — a vertex buffer with a `VertexLayoutDesc` draws where its positions say
- [x] `cmdBindVertexBuffer` — the `offset` argument shifts which vertices are read
- [x] `cmdBindIndexBuffer` + `cmdDrawIndexed` — `IndexType::UInt16` and `UInt32`, `firstIndex` and `vertexOffset`
- [x] `cmdDraw` instancing — `VertexInputRate::PerInstance` with `instanceCount` and `firstInstance`
- [x] `cmdDrawIndirect` — argument buffer at a non-zero `offset`
- [x] `cmdDrawIndexedIndirect` — same, with an index buffer bound
- [x] Indirect records against an instanced pipeline — the `instanceCount` and `firstInstance` fields of a `DrawArgs` record, which no direct-draw test reaches

## 2. Pipeline state — `RhiPipelineStateTests.cpp`

- [x] `BlendStateDesc` — disabled overwrites; `SrcAlpha`/`OneMinusSrcAlpha` and `One`/`Zero` produce the same numbers on both backends
- [x] `DepthStencilStateDesc.depthCompareOp` — `Less` `LessEqual` `Greater` `GreaterEqual` `Equal` `NotEqual` `Never` `Always` (and `None`) each keep or reject the fragment, at a nearer, equal and farther z
- [x] `DepthStencilStateDesc.depthWrite` — off leaves the depth buffer at its cleared value while the colour still lands
- [x] `cmdSetScissor` — pixels outside the rectangle are untouched, and the rectangle's origin corner is the same one on both backends
- [x] `cmdSetScissor` with an empty rectangle — a zero-width or zero-height scissor rejects every pixel on both backends
- [x] `cmdSetScissor` fully off the target — a rectangle that becomes empty only after clamping (`{100, 100, 4, 4}` on an 8x8) takes the general branch on both backends, not the literal-zero shortcut
- [x] `GraphicsPipelineDesc::fillMode` — baked `Line` leaves the triangle's interior unpainted, `Fill` fills it
- [x] `PrimitiveTopology` — `TriangleStrip`, `Lines`, `Points` each rasterise their own shape
- [x] `PrimitiveTopology::LineStrip` — a strip joins consecutive vertices and does not close back to the first
- [x] `destroyGraphicsPipeline` — destroy then recreate on the same render pass, and double destroy is harmless

## 3. Shader resources — `RhiDescriptorTests.cpp`

- [x] `cmdBindDescriptorSets` — a `UniformBuffer` written through `updateDescriptorSets` reaches the fragment shader
- [x] `cmdBindDescriptorSets` — `CombinedImageSampler`: a 2x2 texture sampled at known UVs returns its texels
- [x] `SamplerDesc` — `Nearest` vs `Linear` and `Repeat` vs `ClampToEdge` change the sampled value as expected
- [x] `AddressMode` — `MirrorRepeat` and `MirrorClampToEdge`
- [ ] `AddressMode::ClampToBorder` — blocked: the two backends clamp to different colours and `SamplerDesc` has no border-colour field, so there is nothing to assert until the interface gains one
- [x] A descriptor with `descriptorCount > 1` — an array binding delivers every element: three `CombinedImageSampler` textures read as `tex[0..2]`, and two `UniformBuffer` blocks read as `blocks[0..1]`
- [x] `updateDescriptorSets` aimed at a binding the layout does not declare — rejected on both backends, the set keeps what it had, and a valid write afterwards still lands. (A *type* that disagrees with the layout is no longer expressible: `WriteDescriptor` carries no type.)
- [x] `DescriptorType::StorageBuffer` — read in a fragment shader
- [x] `updateDescriptorSets` — rewriting a live set to a second buffer changes what the next draw reads
- [ ] `firstSet` — two sets bound at once, each reaching its own binding — partly covered by `TwoDescriptorSetsBoundAtOnceEachReachItsOwnBinding`: two sets with *distinct* binding numbers bound in one call, and a single set bound at `firstSet = 1`, both pinned to exact bytes on both backends. Left open because two sets that each use `binding = 0` diverge, and because no pixel can catch a wrong `firstSet` on either backend — see the Divergences note below

## 4. Compute — `RhiComputeTests.cpp`

- [x] `createComputePipeline` + `cmdDispatch` — a compute shader fills a storage buffer, sixteen invocations each writing their own slot, and four slots past the dispatch still hold their sentinel. Read back through a fragment shader, **not** through `mapBuffer`: no single `CpuAccess` value gives a readable mapping of a storage buffer on both backends — see the note below
- [ ] `cmdDispatch` group counts — `groupCountX/Y/Z` map to the invocation ids the shader sees
- [ ] `CommandListType::Compute` — a dispatch recorded on a compute list executes

## 5. Textures — `RhiTextureTests.cpp`

- [x] `updateTexture` — a sub-rectangle upload lands at the given x/y and leaves the rest alone. `y` is a row index in upload space on both backends, so the sub-rect keeps the same corner; readback still hands rows back flipped, as `TextureInitialDataReadsBackUnchanged` already pins
- [x] `TextureDesc.mipLevels` — sampling an explicit lod returns that mip. *Per-mip upload could not be
      tested: no interface reaches a 2D mip. See the three mip findings below*
- [ ] `updateTexture` on a mipped texture — divergence read from the source, not yet pinned: Vulkan
      regenerates the whole chain per call, OpenGL writes level 0 only. See the findings below
- [x] `updateCubeFace` + `updateCubeMip` — six faces keep their own contents, in the same face order on
      both backends: 0..5 = +X -X +Y -Y +Z -Z, identical on both, level 0 and level 1. `updateCubeMip`
      takes `packed6Faces` and cannot address one face, so the test pins all six at once
- [x] `TextureFormat` matrix — R8, RG8, RGBA8, R16F, RG16F, RGBA16F round-trip through create and
      readback, identical on both backends: unwritten colour channels read 0, unwritten alpha reads
      255. `DEPTH_COMPONENT32` cannot be created with initial data at all, so it is pinned by
      clearing it in a pass and reading that back — see the findings below
- [x] `getNativeTextureId` — non-zero for a live texture, stable across calls, different for two
      textures, and 0 for both an invalid and a destroyed handle. Identical contract on both
      backends; only the numeric meaning differs (GL texture name vs `VkImageView`)
- [x] `TextureDimension::Tex3D` — create, upload, read back a slice. OpenGL only: depth 0..3 each
      return their own slice, no swap or reversal. **Skipped on Vulkan, which has no 3D textures at
      all** — see the findings below

## 6. Render passes and framebuffers — `RhiRenderPassTests.cpp`

- [x] `AttachmentLoadOp` — `Load` preserves the previous contents, `Clear` replaces them. Byte-identical on both backends
- [x] `AttachmentStoreOp::DontCare` — pinned as measured, not guaranteed: on this GPU it discards
      nothing and leaves exactly what `Store` leaves, on both backends. They agree for different
      reasons — see the findings below
- [x] Multiple colour attachments — a fragment shader writing `location = 0..2` hits the right
      targets, and a partial draw leaves every attachment's untouched region at its *own* clear
      colour. Byte-identical on both backends. Note: `BlendStateDesc` is one state for the whole
      pipeline — Vulkan replicates it across the pass's attachment count
      (`VulkanRHIPipelines.cpp:1154-1162`), so per-attachment blend is not expressible
- [x] `overrideClearColors` / `overrideClearDepth` — per-begin clears beat the baked ones, the two are
      independent, and an override binds to one begin only. Identical on both backends. There is no
      *partial* override — see the findings below
- [x] Colour + depth in one pass — both attachments store and both read back, exactly equal on the
      two backends: covered depth 16384, cleared depth 65535, colour byte-identical
- [x] `cmdBlit` — `Color` and `Depth`, `Nearest` vs `Linear`, 1:1 and a 2:1 downscale. Byte-identical
      on both backends with no tolerance; `Depth` moves depth only and leaves the destination colour
      at its clear. Note the signature takes *framebuffers and no rectangles*
      (`RenderHardwareInterface.h:554`), so a downscale is expressible only by sizing the destination
      framebuffer smaller. Orientation agrees because two flips cancel: OpenGL blits in
      bottom-left-origin attachment space, Vulkan in top-left-origin image space
- [x] `getBackbufferFramebuffer` — the handle is stable, never collides with a created framebuffer,
      a clear-only pass and a blit both complete, and an ordinary offscreen draw afterwards still
      works. **The returned handle itself diverges** — see the findings below, so the test asserts
      only what both backends agree on

## 7. Frames and synchronisation — `RhiFrameTests.cpp`

- [x] `framesInFlight` — `framesInFlight()` consecutive frames each write a distinct value and all
      survive. The count itself diverges: OpenGL 1 (interface default), Vulkan 2 — see FINDINGS.md
- [x] `frameInFlight` — the index advances by one per frame and wraps: OpenGL 0,0,0; Vulkan 0,1,0,1,0. Only Vulkan's present path advances it
- [x] `submit` — lists submitted one after another in one frame execute in submission order. The
      interface was narrowed to one list per `submit`; see FINDINGS.md
- [x] `cmdUpdateBuffer` — writes at the call: every draw in the list sees the new contents, including
      draws recorded before the call. Pinned for a uniform and a storage buffer. Contract changed; see FINDINGS.md
- [x] `cmdPipelineBarrier` — write a texture then sample it in the same list: identical with and without the barrier on both backends. It changes nothing visible on either; see FINDINGS.md
- [x] `cmdHostCallback` — removed from the interface: it ran only on OpenGL (Vulkan was an empty function) and nothing in the engine called it
- [x] `lastGpuMs` — 0 before any result, non-negative always, above zero within 10 frames of real work. Lag differs (OpenGL 1 frame, Vulkan 2) and is not pinned

## 8. Capabilities and the present path — `RhiCapabilityTests.cpp`

- [x] `backend()` — reports the backend the fixture actually built, stable across calls. OpenGL relies on the interface default of `OpenGL`
- [x] `supportsFullPipeline` / `supportsGpuResources` / `supportsGeometryPass` / `supportsRenderPass` — all true on both backends, and the implications the engine relies on hold. The flags are effectively dead; see FINDINGS.md
- [x] `createShaderModule` / `destroyShaderModule` — built from a `ShaderModuleDesc`, destroyed after pipeline creation, the pipeline still draws on both backends. One desc serves both only because each reads a different field; see FINDINGS.md
- [x] `setPresentBlitSource` + `presentSourceTexture` — the texture handed in is reported back, a second call replaces it, a destroyed source stays reported but reads back false, and `present()` is safe with it on both backends
- [x] `publishDisplay` / `editorViewNativeId` / `capturePublishedFrame` — nothing before a publish; after one, a non-zero native id and a capture of exactly the published pixels, across size changes; the device stays usable. OpenGL capture made synchronous; see FINDINGS.md

## Findings

Engine-side issues raised by the tests live in `FINDINGS.md`. Agents do not need to read it.
