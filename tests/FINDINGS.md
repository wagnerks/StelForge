# Findings from the RHI suite (engine side, not test work)

Raised by the tests, out of scope for the agent that hit them. Each needs a decision before it
becomes a contract the passes rely on by accident.

- Both OpenGL bind paths need the pipeline first. `execBindVertexBuffer`
  (`src/renderModule/OpenGLRHI.cpp:633`) reads the stride from the currently bound pipeline, and
  `execBindIndexBuffer` (`:664`) calls `glVertexArrayElementBuffer` on that pipeline's VAO, so a
  bind recorded *before* `cmdBindPipeline` is silently dropped — the index path still stashes the
  type but resets the offset to 0, so a non-zero bind offset is lost quietly too. Vulkan has no such
  requirement, so that order works on one backend and draws nothing on the other, with no
  diagnostic. Either the interface documents the order or OpenGL defers the bind.
- The same function treats `cmdBindVertexBuffer`'s `slot` as an *index into* `layout.bindings`,
  while Vulkan passes it straight through as the binding number. These agree only while
  `bindings[i].binding == i`.
- `firstInstance` was accepted and dropped on OpenGL: `execDraw` and `execDrawIndexed` called
  `glDrawArraysInstanced` / `glDrawElementsInstanced[BaseVertex]`, none of which has a base
  instance, while Vulkan passed it to `vkCmdDraw`. Fixed — both now take the `*BaseInstance` entry
  points when `firstInstance != 0`, and the instancing test pins it.
- `OpenGLRHI::execDrawIndexedIndirect` (`src/renderModule/OpenGLRHI.cpp:809`) ignores
  `m_currentIndexOffset`, the byte offset given to `cmdBindIndexBuffer`. `execDrawIndexed` folds it
  into the index pointer and Vulkan applies it on both paths, so an indexed *indirect* draw with a
  non-zero bind offset reads the wrong indices on OpenGL and the right ones on Vulkan. Not a
  one-liner: GL's indirect entry points take no index pointer at all, so the offset would have to
  move into the record's `firstIndex`, which is GPU memory the RHI does not own. Decide whether the
  interface forbids the combination or the backend patches around it.
- Vulkan created its device without `drawIndirectFirstInstance`, which the spec requires for a
  non-zero `firstInstance` inside an indirect record (the direct `vkCmdDraw` path does not need it).
  The indirect instancing test passed only because this driver honoured it anyway. Fixed — enabled
  alongside `multiDrawIndirect` in `VulkanRHI.cpp`, gated on support as the others are.
- `BlendStateDesc` has no separate alpha factors and no blend op, so both backends blend alpha with
  the colour factors and a standard over-draw cannot preserve the destination alpha. The backends
  agree, so nothing fails; it is a gap in what the interface can express, not a bug.
- `CompareOp::None` means opposite things in the two structs that take it. In `SamplerDesc` it means
  "comparison off" and both backends honour that (`GL_NONE` / `compareEnable = VK_FALSE`); in
  `DepthStencilStateDesc` both backends fall through to always-pass. Nothing sets it there today, so
  nothing fails, but one enumerator reading "off" in one struct and "on, unconditionally" in another
  is a trap. Either the depth state should reject it or it should mean `depthTest = false`.
- `DepthStencilStateDesc{ depthTest = false, depthWrite = true }` is a silently ineffective request:
  OpenGL stops updating the buffer once `GL_DEPTH_TEST` is disabled regardless of `glDepthMask`, and
  Vulkan ignores `depthWriteEnable` when `depthTestEnable` is false. The struct can express
  depth-only writes with no test; neither backend does it. Same shape as the `CompareOp::None` trap.
- A degenerate `cmdSetScissor` diverged: OpenGL rejects everything (`glScissor(0,0,0,0)` with the
  test on), while the Vulkan path substituted a 1x1 rect at the origin, so "draw nothing" drew one
  pixel there. Fixed — Vulkan now uses a zero extent. Needs the test that pins it.
- `cmdSetPolygonFillMode` worked on OpenGL and was an empty override on Vulkan, where fill is baked
  into the PSO — so the editor's wireframe toggle, its only caller, silently rendered solid geometry
  on Vulkan. Removed from the interface and both backends; `GeometryPass` now keys its pipeline cache
  by fill mode and binds a second, `Line`-baked pipeline instead. That also retired an OpenGL-only
  ordering trap: `glPolygonMode` was applied from inside `cmdBindPipeline`, so `GeometryPass`'s call
  before the first bind had never done anything.
- `PrimitiveTopology::LineLoop` mapped to `GL_LINE_LOOP` on OpenGL and to
  `VK_PRIMITIVE_TOPOLOGY_LINE_STRIP` on Vulkan, which has no loop topology — so the closing segment
  existed on one backend only. Nothing created a pipeline with it (`DebugPass` draws every line batch
  through one `Lines` pipeline and ignores `LineData::renderType`), so no rendering was actually
  wrong, contrary to the first read of this. Replaced with `LineStrip`, which both backends have;
  closing a loop is now the caller's job.
- The suite was intermittently failing: Vulkan's device creation faulted a couple of dozen tests into
  a run, and the failures landed on whichever tests happened to run last, so they read as leaks in
  unrelated code. Cause was the fixture calling `glfwInit`/`glfwTerminate` around *every* test —
  terminating unloads the Vulkan loader the next instance is about to use. GLFW now comes up once per
  process through a `::testing::Environment`. Three consecutive full runs green afterwards.
- Handles carry no generation counter (`PipelineHandle` is a bare `uint32_t id`), and both backends
  recycle ids through a free list, so a handle held across a destroy is byte-identical to the next
  resource created in that slot. A caller keeping a stale handle does not get a detectable error; it
  silently addresses someone else's pipeline. Worth deciding whether that is acceptable or whether
  handles should carry a generation.
- Drawing with a genuinely dead pipeline handle diverges: OpenGL's `cmdBindPipeline` finds no live
  resource and returns early, leaving the previously bound program current, so the draw quietly
  succeeds with the wrong pipeline; Vulkan faults. Invalid API use either way, but not equally loud.
- Neither backend defers freeing a pipeline: `destroyGraphicsPipeline` calls straight through to
  `glDeleteProgram` / `vkDestroyPipeline`, with no deletion queue and no fence wait, so on Vulkan the
  caller must have waited. `ImGuiRHI.cpp:113` (font-atlas rebuild) and `TextRenderer.h:417` both
  destroy a pipeline outside init and neither calls `waitIdle` — confirmed absent in both files.
  Whether either is reachable while a frame is in flight is unverified; if it is, it is a real
  Vulkan crash.
- Descriptor *set* indices exist on Vulkan and nowhere on OpenGL. `OpenGLRHI::cmdBindDescriptorSets`
  (`src/renderModule/OpenGLRHI.cpp:1588`) names neither the set index nor `firstSet`: it flattens
  every bound set's bindings into one list and binds by binding number alone. Two sets that each use
  `binding = 0` work on Vulkan and collide on OpenGL, last bind winning, silently. Measured, with a
  distinct colour in each set and a shader reading two channels from each: Vulkan
  `(24, 72, 136, 200)`, OpenGL `(136, 200, 136, 200)` — the second set's buffer answered both reads,
  with no error from either the GL driver or the shader compiler. The loss happens twice over: the
  RHI's second `glBindBufferBase` overwrites the first, and `SFE_SET` collapses both shader blocks to
  `layout(binding = 0)`, which GLSL accepts. Sets with distinct binding numbers, and a single set
  bound at `firstSet = 1`, both work on both backends and are pinned by
  `TwoDescriptorSetsBoundAtOnceEachReachItsOwnBinding`. The fork: give OpenGL a per-set binding
  stride, or state that set indices are Vulkan-only and binding numbers must be unique across every
  set a pipeline binds. Until then the second is what the code does and what callers must assume.
- A wrong `firstSet` is invisible in the attachment on both backends. Binding the set at `0` instead
  of `1` for `rhi_test_set_one.fs` leaves OpenGL unchanged, since it never reads the offset, and on
  Vulkan the validation layer rejects the frame (`VUID-vkCmdDraw-None-08600`, "statically uses
  descriptor set 1 ... the set (1) is out of bounds for the number of sets bound (1)") while the draw
  still produces the correct pixels — reading an unbound set is undefined and this driver's undefined
  answer happened to be right. So `firstSet` is a parameter the suite can only check through the
  validation log, which is another instance of "a frame that trips validation can still pass gtest".
- Binding 15 is reserved on OpenGL only: `sfe.glsl` expands `SFE_PUSH_BLOCK` to
  `layout(std140, binding = 15)` and the backend binds its push-constant UBO there. A descriptor at
  binding 15 in a pipeline that also uses push constants is fine on Vulkan and overwritten on OpenGL.
- One RHI descriptor set can become two Vulkan sets and shift every later set index.
  `descriptorLayoutNeedsSplit` (`VulkanRHIVkUtil.h:266`) splits a layout when an image and a buffer
  descriptor share a binding number, `expandSetLayouts` pushes both, so an RHI set 0 occupies Vulkan
  sets 0 and 1 and an RHI set 1 lands at Vulkan set 2 — while OpenGL is unaffected, having no set
  index. A shader written with `SFE_SET(1, ...)` against such a pipeline is right on OpenGL and wrong
  on Vulkan, and nothing in `DescriptorSetLayoutDesc` says the layout will split.
- `VulkanRHI::cmdBindDescriptorSets` returns early, abandoning the sets already collected, when any
  handle in the array is dead; OpenGL skips the dead one and binds the rest.
- Three `AddressMode` values are unexercised and one of them is under-specified: `SamplerDesc` has no
  border colour, so `ClampToBorder` clamps to whatever each backend happens to default to.
- `updateDescriptorSets` has no documented synchronisation contract, and the two backends answer
  "does a rewrite affect an already-recorded draw?" differently: OpenGL copies the set's bindings into
  the recorded command at record time, Vulkan records only the `VkDescriptorSet` handle. Today a
  single scene fence means the question never bites, but nothing in the interface says the caller must
  have waited, and that must be revisited if frames-in-flight are added.
- A descriptor write whose type disagreed with the layout was the worst kind of split: OpenGL matched
  its slot on binding **and** type, so the mismatched write appended a second slot, the original stayed
  authoritative, and the rewrite was silently invisible; Vulkan passed the caller's type to the driver,
  which read `pImageInfo` where the layout promised a buffer and took an access violation. A caller
  developing against OpenGL would have shipped a Vulkan crash. Fixed — both backends now take the type
  from the set's layout, which is the only thing that can define it, and reject a disagreeing write with
  a named error instead. `WriteDescriptor::type` is now advisory; removing the field would make the
  mistake unexpressible rather than merely detectable, at the cost of 39 call sites.
- `AddressMode::ClampToBorder` diverges and cannot be fixed in a backend: OpenGL returns
  RGBA(0, 0, 0, 0) (it never sets `GL_TEXTURE_BORDER_COLOR` — the call is commented out around
  `OpenGLRHI.cpp:2586`), Vulkan returns RGBA(0, 0, 0, 255) (`VulkanRHIResources.cpp:855` hard-codes
  `VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK`). `SamplerDesc` has no border colour, so neither value is
  anything a caller asked for. Needs a border-colour field before it can be pinned.
- `AddressMode::MirrorClampToEdge` was used without enabling `samplerMirrorClampToEdge`, a validation
  violation on a Vulkan 1.2 instance (`VUID-VkSamplerCreateInfo-addressModeU-01079`) that this driver
  happened to honour. Fixed — device creation now queries and enables `VkPhysicalDeviceVulkan12Features`
  (which also carries `hostQueryReset`, so the separate promoted struct it used to chain was replaced;
  chaining both the aggregate and a struct it promotes is itself illegal).
- Array descriptors (`DescriptorSetLayoutBinding::count > 1`) were accepted by the interface and
  delivered by neither backend: OpenGL bound only `images[0]`, so `tex[1]` sampled an unbound unit and
  came back black, silently; Vulkan declared the array in the layout but wrote `descriptorCount = 1`
  from `images.front()`, so element 1 was never written and the first draw reading it **lost the
  device** — silently through the interface, which reported success and handed back zeros. Both fixed:
  OpenGL loops over the elements using `binding + i` as the unit or buffer point, Vulkan writes the
  whole array in one `VkWriteDescriptorSet` over a contiguous info span. That also removed a latent
  dangling pointer on the Vulkan side, which took `&infos.back()` while the vector could still grow.
  `WriteDescriptor` still has no `dstArrayElement`, so only whole-array writes from element 0 are
  expressible.
- Rewriting a descriptor set that is bound into a *recording* command buffer is invalid on Vulkan
  (the command buffer is invalidated unless the set is UPDATE_AFTER_BIND, which this RHI never uses)
  and harmless on OpenGL, which snapshots the bindings at record time. A test was doing it and passing;
  validation rejected the frame it recorded. The case has been removed rather than pinned. The
  interface documents none of this — a caller must rewrite between frames.
- The suite used to fail intermittently on Vulkan — roughly two runs in ten, landing on whichever tests
  ran late, which made them read as bugs in unrelated code. Two causes were found and fixed: the fixture
  cycled `glfwInit`/`glfwTerminate` around every test, and `terminate` unloads the Vulkan loader the next
  instance is about to use (GLFW now comes up once per process); and `VulkanRHI::cleanup` swept
  framebuffers without freeing their `ownedViews`, leaking an image view per pinned attachment layer on
  every one of the ~40 device teardowns a run performs (`destroyFramebuffer` already freed them; the
  shutdown sweep did not). 32 consecutive full runs are now clean with zero validation output. That is
  evidence, not proof: it would still be consistent with a residual failure rate of a few percent.
- `mapBuffer` could not read a buffer on either backend, and the two `CpuAccess` values that worked
  were exact opposites. OpenGL returned a pointer for `None`/`Read` and null for `Write`/`ReadWrite`;
  Vulkan the reverse. `OpenGLRHI::toGLMapFlags` ORed in `GL_MAP_INVALIDATE_RANGE_BIT |
  GL_MAP_UNSYNCHRONIZED_BIT` from the buffer's `cpuAccess` alone, and either is an error alongside
  `GL_MAP_READ_BIT`, so `glMapBufferRange` failed; `VulkanRHI::createBuffer` treated only
  `Write`/`ReadWrite` as host-mapped and put `Read` in device-local memory `vmaMapMemory` cannot reach
  — so the one enumerator that names a GPU-to-CPU transfer was the one Vulkan could not map. Both
  fixed: the OpenGL flags now key off the *map mode* (invalidate/unsynchronized only for a write-only
  mapping, never one that reads), and Vulkan treats any `cpuAccess` other than `None` as host-mapped.
  This was also why `BufferInitialDataAndUpdateAreVisibleThroughMap` skipped on OpenGL from the suite's
  first run; that skip is now gone and the suite has no skips left.
- The engine's only two-set bind (`LightingPass.cpp:435`) is safe, but by an unstated invariant. Set 0
  holds only samplers (bindings 0,1,2,3,5,6,7,8,12,13,14) and set 1 only uniform buffers
  (3,5,9,10,11,15). Numbers 3 and 5 appear in both, and that is harmless on OpenGL *only* because
  texture units and uniform-buffer points are separate namespaces there. The real OpenGL rule is that
  binding numbers must be unique per namespace across every set a pipeline binds; two sets that both
  put a buffer at binding 0 collide silently, last bind winning. Nothing checks this.
- **The Vulkan backend does not reproduce its own frame.** `compare-backends.ps1 -SelfTest` renders the
  same backend twice and diffs the two captures. OpenGL: 0 differing pixels, exactly reproducible.
  Vulkan: 342 and 457 differing pixels across two attempts, max channel delta ~240. So a Vulkan capture
  carries ~400 pixels of run-to-run noise, which is also what makes the cross-backend number wobble
  (312 048 / 311 799 / 311 792 on one unchanged build). Consequences: any backend comparison finer than
  that noise floor is meaningless, and the frame diff cannot serve as a regression check for a change
  whose effect is smaller. Cause unknown — a race, an uninitialised read, or a frame-timing dependency
  are all consistent with it. `-SelfTest` now takes `-SelfTestBackend opengl|vulkan`; it only ever
  checked OpenGL before, which is why this went unnoticed.
- The descriptor-layout split is gone. It answered a repeated binding number by splitting the layout
  into two Vulkan sets, images in one and buffers in the other, which silently shifted the set index of
  every layout after it. Nothing used it: all 128 `SFE_SET` declarations and every layout in the engine
  keep their binding numbers distinct. `createDescriptorSetLayout` now rejects a duplicate binding
  number with a logged error instead of reinterpreting it. That also frees `WriteDescriptor::type`,
  which only existed to disambiguate the two halves of a split layout.
- `WriteDescriptor::type` is gone. It duplicated a fact the layout already fixed, and the only thing a
  caller could do with the second copy was contradict the first — silently ignored on OpenGL, an access
  violation on Vulkan. With the split removed, one binding number names exactly one descriptor, so both
  backends read the type from the set's layout. 67 assignments across 14 files went with it; nothing
  else changed shape. Verified by more than the suite: OpenGL renders its comparison frame bit-identically
  before and after (0 of 2 073 600 pixels differ), which is a real check because OpenGL is
  self-deterministic. No equivalent check exists for Vulkan until its own nondeterminism is fixed.

- `updateTexture` cannot address a mip level of a 2D texture. It hardcodes level 0 on both backends
  (`OpenGLRHI.cpp:2298`, `VulkanRHIResources.cpp:769`); the only mip-addressed writes in the whole
  interface are `updateCubeFace` / `updateCubeMip`, which are cube-only. So the contents of a 2D
  texture's lower levels can only ever be whatever `createTexture` generated. Either `updateTexture`
  gains a mip parameter or the interface says the chain is generated and not writable.
- `updateTexture` on an already-mipped 2D texture diverges, and nothing catches it today. Vulkan
  regenerates the whole chain on every call (`VulkanRHIResources.cpp:775-777`); OpenGL writes level 0
  and never regenerates (`OpenGLRHI.cpp:2296-2303`). After `createTexture(data)` + `updateTexture`,
  a sample at lod 1 sees the new data on Vulkan and the original data on OpenGL. Read from the
  source; the mip test deliberately avoids the combination, so no test fails yet.
- `FramebufferAttachmentDesc::mipLevel` is dropped by Vulkan for a `Tex2D`. `viewFor`
  (`VulkanRHIPipelines.cpp:767-768`) returns the texture's whole-chain view whenever
  `att.layer == UINT32_MAX || dimension == Tex2D`, ignoring `mipLevel`, while OpenGL passes it to
  `glFramebufferTexture2D` (`OpenGLRHI.cpp:2174`). Rendering into a specific mip of a 2D texture
  works on OpenGL and silently targets the base level on Vulkan.
- `SamplerDesc` has no mip-filter field. The mip mode is derived from `minFilter` alone, so only
  `LinearMipmapLinear` opens the chain: anything else leaves Vulkan's `maxLod` at 0
  (`VulkanRHIResources.cpp:849-851`) and OpenGL on a non-mipmap `GL_TEXTURE_MIN_FILTER`. Both
  backends agree — with `Nearest`, every lod returns level 0 — so this is a gap in what the
  interface can express, not a divergence. It does mean nearest-within-a-level plus
  nearest-between-levels is not requestable.
- `VulkanRHI::readbackTexture` used a dangling pointer. It cached
  `source = getLive(m_textures, handle.id)`, then created the scratch RGBA8 image through
  `createTexture`, which push_backs into `m_textures` and reallocates it, and then blitted from
  `*source`. Only reachable for a non-depth, non-RGBA8 texture, which nothing exercised until the
  format matrix did — it crashed ~10% of Vulkan runs, sometimes splashing onto whichever tests ran
  after it. This is the residual flakiness the suite has been carrying. Fixed: `source` is re-fetched
  after the scratch is created, next to the existing `tex` re-fetch.
- `VulkanRHI::publishDisplay` had the same bug on the editor's live path: `srcTex` was fetched, then
  `ensureDisplayTextures` created the two display textures, then `*srcTex` was blitted. It fires
  whenever the display textures are created — the first publish and every viewport resize. Found by
  reading, not by a test; no headless test reaches `publishDisplay`. Fixed the same way.
- `DEPTH_COMPONENT32` cannot be created with initial data on either backend, for two independent
  reasons. Vulkan's upload goes through `copyBufferToImage`, which hardcodes
  `aspectMask = VK_IMAGE_ASPECT_COLOR_BIT` (`VulkanRHIResources.cpp:608`) and cannot address a depth
  image; and the backends disagree what the bytes would mean anyway — OpenGL takes `GL_UNSIGNED_INT`
  into a 32-bit unorm (`OpenGLRHI.cpp:295`), Vulkan maps the format to `VK_FORMAT_D32_SFLOAT`
  (`VulkanRHIVkUtil.h:23`). One byte buffer cannot serve both.
- `TextureDimension::Tex3D` is an enumerator only one backend implements. OpenGL creates one through
  `glTexImage3D` (`OpenGLRHI.cpp:2486-2495`); Vulkan rejects the dimension outright and returns a null
  handle (`VulkanRHIResources.cpp:713-716`), and `createImage` could not serve one anyway —
  `imageType` is hardcoded to `VK_IMAGE_TYPE_2D` (`:638`) and the view type is picked from Cube /
  Array / 2D only (`:624-632`). The caller gets a null handle and no other signal. This is the
  suite's one legitimate skip.
- `updateTexture` cannot write a volume on either backend, and the signature has no z or depth
  parameter to carry one. OpenGL routes it to `glTexSubImage2D` against `GL_TEXTURE_3D`
  (`OpenGLRHI.cpp:2296-2303`), which is a silent GL error rather than a partial write; Vulkan pins
  `layerCount = 1`, `imageExtent.depth = 1` at `z = 0` (`VulkanRHIResources.cpp:770-773`). So
  `createTexture`'s initial data is the only way a volume's contents can ever be set.
- `readbackTexture`'s `layer` does not reach a 3D texture's depth. `OpenGLRHI.cpp:990` rejects any
  dimension that is not `Tex2D` or `Array` before the layer path at `:1006`, which is gated on
  `isArray` alone. Slices have to be read by sampling a `sampler3D` instead.
- `AttachmentStoreOp` never reaches the OpenGL backend. `createRenderPass` copies the whole
  `RenderPassDesc` into `GLRenderPass::desc` (`OpenGLRHI.cpp:839-850`) and the field is never read
  again; there is no `glInvalidateFramebuffer` anywhere in the backend, so `Store` and `DontCare` are
  one code path. Vulkan does honour it (`VulkanRHIPipelines.cpp:900` colour, `:922` depth,
  `toVkStoreOp` at `VulkanRHIVkUtil.h:282-284`), and this driver simply keeps the pixels. The two
  agree today by coincidence: the moment a driver or a future GL invalidate path actually discards,
  they stop agreeing, and no test on GL could tell the two ops apart.
- The suite's long-standing Vulkan flakiness (`SEH 0xc0000005` in `SetUp()`, landing on whichever
  tests happened to run next) was the fixture leaving the OpenGL context current on a window it then
  destroyed. The next Vulkan test created its device on a thread still carrying the GL driver's
  thread state. Measured: interleaved OpenGL/Vulkan failed 5 runs in 10, Vulkan-only passed 3 in 3
  (48 tests), and one Vulkan test repeated 40 times in-process never failed — so it was never
  repeated init/cleanup, always the alternation. Fixed by `glfwMakeContextCurrent(nullptr)` before
  the window is destroyed: 0 failures in 10. Note the rate is driver-dependent — the same binary ran
  20 clean runs a week earlier and 5-in-10 failures after, so a clean batch alone never proved
  anything here.
- `overrideClearColors` has no length and no per-entry fallback. Both backends read element `i` for
  every `i < colorAttachmentCount` (`OpenGLRHI.cpp:1359-1363` copies that many at record time,
  `VulkanRHIPipelines.cpp:1346` indexes the same range), so the `size >= colorAttachmentCount` note at
  `RenderHardwareInterface.h:350` is load-bearing, not advisory: a caller who sizes the array to the
  attachments they actually meant to override silently overreads their own buffer on both backends.
  There is no way to override one attachment and leave the others at their baked clears.
- `vkCmdPushConstants` is called with `VK_SHADER_STAGE_FRAGMENT_BIT` against a pipeline layout that
  has no fragment push-constant range (VUID-vkCmdPushConstants-offset-01795), and a pipeline is
  created whose fragment shader writes location 0 with no colour attachment bound. Both show up in
  `engine/bin/sfe_fatal.log` on every suite run. Neither fails a test today; both are validation
  errors the suite should not be producing.
- `getBackbufferFramebuffer` returns opposite things to the same headless host. OpenGL returns an
  *invalid* handle unconditionally (`OpenGLRHI.cpp:1721-1724`), where null is not an error value —
  `execBeginRenderPass` reads it as "bind FBO 0" (`:1404-1413`) and `execBlit` resolves it to GL
  framebuffer 0 (`:2714-2733`). Vulkan returns a *valid* handle (`VulkanRHIPipelines.cpp:1028-1030`),
  created in `init()` regardless of `presentToScreen()`, carrying `swapchainTarget = true` and no
  `VkFramebuffer`. So `if (rhi.getBackbufferFramebuffer())` answers differently on the two backends
  for the identical host.
- Drawing into the backbuffer faults on Vulkan. `cmdBeginRenderPass` rejects a swapchain-target
  framebuffer *silently* (`VulkanRHIPipelines.cpp:1317-1319`), so a following `cmdBindPipeline` +
  `cmdDraw` is recorded outside any render pass instance, and `cmdDraw` has no guard
  (`VulkanRHIPipelines.cpp:1519-1523`). Measured: `SEH 0xC0000005`, not a validation message. OpenGL
  draws into the window's default framebuffer instead. Reproduced deterministically with a probe,
  which is deliberately not kept in the suite. Needs a decision: guard and log, or make the silent
  reject an error the caller can see.
- On Vulkan a blit into the backbuffer is a latch, not a copy: it stores the source as the present
  source (`VulkanRHIPipelines.cpp:1582`), so every later `present()` takes the present-blit path. It
  survives the source being destroyed (`recordPresentBlit` falls back on a dead handle, `:606-622`),
  but the handle is never cleared, so a recycled texture id could be presented by accident.
- `framesInFlight()` is 1 on OpenGL and 2 on Vulkan. OpenGL never overrides it and takes the interface
  default (`RenderHardwareInterface.h:496`); Vulkan returns `kMaxFramesInFlight` (`VulkanRHI.h:129`,
  `:348`). Anything a pass sizes by it gets half as many slots on OpenGL. Not a bug by itself, but it
  means per-frame aliasing is only ever exercised on Vulkan.
- Vulkan's `drawFrame` can return early at `VulkanRHI.cpp:1172`, before the
  `m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight` at `:1259`. If that path is hit the
  frame index does not advance, and the next frame reuses the previous frame's per-frame resources.
  Read from the source; no test reaches that branch.
- `submit(lists, count)` was a lie on Vulkan: it used `lists[0]` only, and every `beginCommandList` of a
  frame reset the same `m_sceneCmds[m_currentFrame]`, so only the last recording survived — and a
  second `beginCommandList` right after a `submit` reset a buffer the GPU might still be executing.
  Every engine caller already submitted one list at a time. Fixed: `submit(CommandListHandle)` takes one
  list; beginning a list while another is unsubmitted logs an error on both backends; Vulkan's
  `beginCommandList` first waits for the pending scene through `finishPendingScene()`, which is now the
  one place that waits, recycles and resets — `drawFrame` and `publishDisplay` call it too, and
  `publishDisplay` no longer leaks the pending list's slot.
- Vulkan draws outside a render pass faulted (`SEH 0xC0000005`), e.g. after `cmdBeginRenderPass` silently
  rejected the backbuffer. Fixed: that rejection now logs, and `cmdDraw` / `cmdDrawIndexed` /
  `cmdDrawIndirect` / `cmdDrawIndexedIndirect` without an active pass log and are dropped. A real
  240-frame runtime capture produces none of these errors.
- Cross-backend frame diff moved from ~311 800 differing pixels to 319 032 (frame 240). Vulkan's own
  frame-to-frame noise is ~400, so this is a real change, but it is not attributed: the GPU driver
  changed in the same week, and there is no stored reference image from before.
- `cmdUpdateBuffer` applied at different moments. OpenGL recorded it and replayed it in list order at
  `submit`; Vulkan wrote host-mapped buffers at the call, recorded `vkCmdUpdateBuffer` for small
  device-local ones, queued large ones for the next list, and silently dropped device-local updates
  inside a render pass. `DebugPass` wrote each chunk of debug geometry to offset 0 of one buffer
  between draws, so on Vulkan every draw read the last chunk. Fixed by one contract on both backends:
  the write happens at the call. OpenGL writes immediately (the recorded command is gone); Vulkan
  copies device-local buffers through a staging buffer at once (`copyThroughStaging`, shared with
  `updateBuffer`). `DebugPass` writes each group to its own offset, draws with `firstVertex`, and
  grows its buffers to the frame's total.
- The runtime frame on Vulkan records draws into a command buffer that validation reports as
  invalidated: "VkDescriptorSet was destroyed or updated without UPDATE_AFTER_BIND". Some pass updates
  a descriptor set after binding it in the same list. OpenGL has no such restriction. 13 messages per
  240-frame capture, identical before and after the `cmdUpdateBuffer` change. Not yet traced to a pass;
  it may account for part of the cross-backend frame diff.
- `cmdPipelineBarrier` does nothing observable on either backend. Vulkan records a `vkCmdPipelineBarrier`
  with only a `VkMemoryBarrier` and never touches image layouts (`VulkanRHIResources.cpp:1223-1240`);
  `cmdEndRenderPass` already moves attachments to `SHADER_READ_ONLY`. OpenGL replays it as
  `glMemoryBarrier` built from `dstAccess` alone (`OpenGLRHI.cpp:2799-2806`, `:2754-2789`), dropping
  `srcStage`, `dstStage` and `srcAccess`. Callers cannot rely on those three fields.
- Vulkan validation reports timestamp queries used without a reset (8 per suite run). `beginCommandList`
  writes its timestamp to `m_currentFrame * kTimestampsPerFrame`, so a second list in the same frame
  reuses the slot. Likely introduced by the suite now submitting several lists per frame; not traced.
- `lastGpuMs` on Vulkan stays 0 forever on a device without a usable timestamp period, timestamp bits or
  host query reset: the timestamp pool is not created (`VulkanRHI.cpp:853`), with no signal to the caller.
  The lag also differs: OpenGL reports the previous submit, Vulkan the frame `framesInFlight()` ago.
- The capability flags are dead. `supportsFullPipeline`, `supportsGpuResources`, `supportsGeometryPass`
  and `supportsRenderPass` are true on both backends, so every reduced-pipeline path never runs:
  `RenderSystem.cpp:250-287` and `:682-735`, Vulkan's named-pass list at `VulkanRHIPipelines.cpp:193+`,
  the checks in `TextOverlayPass.cpp:20`, `:36`, `Engine.cpp:54` and `ChunksSystem.cpp:70`. Comments at
  `TextOverlayPass.cpp:18` and `SystemsPriority.h:32` still describe Vulkan as a reduced pipeline.
  `supportsRenderPass(nullptr)` is true on OpenGL and false on Vulkan; nothing passes null. The
  capability test keeps its own copy of the 19 `RENDER_PASSES` names because `SystemsPriority.h` does
  not compile in the test target. Candidate for removal, together with that test.
- `ShaderModuleDesc` means different things per backend. OpenGL compiles `source` as include-expanded GLSL
  and ignores `name` (`OpenGLRHI.cpp:2343`); Vulkan ignores `source` and loads
  `shaders/vulkan/<name>.spv` (`VulkanRHIPipelines.cpp:854-863`). The field comment promising "GLSL/HLSL
  text or SPIR-V bytes" (`RenderHardwareInterface.h:252`) matches neither.
- Vulkan caches shader modules by name (`VulkanRHIPipelines.cpp:858-871`) and `destroyShaderModule` never
  clears the cache (`:875-882`). Two creates with the same name share one handle, so destroying one
  destroys the other; after a destroy and slot reuse, creating the old name returns a different shader.
  OpenGL creates a separate shader each time. Read from the source, not pinned by a test.
- Vulkan leaked every texture and buffer slot. `destroyTextureInternal` and `destroyBufferInternal` reset
  the resource, `live` included, and `recycleResource` then saw a dead slot and returned without adding
  the id to the free list (`VulkanRHI.h:219`). Destroy id 3, create again, get id 4 — forever. Fixed by
  no longer resetting in the `*Internal` functions; `recycleResource` resets the slot itself. Pinned in
  `TextureHandleSurvivesDestroyAndRecreate` for textures and buffers. Consequence: stale handles can now
  alias a new resource on Vulkan too, as they always could on OpenGL — see the generation-counter note.
- OpenGL `capturePublishedFrame` read the *other* display slot first and streamed through two PBOs, so
  it handed back the frame from one publish earlier, returned false on the first call after a resize,
  and right after the slots were created or resized read a slot nothing had been written to (garbage in
  the editor viewport after a resize). Vulkan copied the published slot synchronously. Fixed: OpenGL now
  blits the published slot and reads it with a synchronous `glReadPixels`, like Vulkan; the PBO ping-pong
  is gone.
- Remaining capture differences: OpenGL downscales the stream to at most 1280 wide
  (`ensureStreamCaptureResources`), Vulkan returns the full display size. `publishDisplay({})` after a
  publish re-publishes the last frame on Vulkan (its present source is the display texture) and does
  nothing on OpenGL. Vulkan's `publishDisplay` also changes the present blit source as a side effect.
- Removed the four dead capability flags (`supportsFullPipeline`, `supportsGpuResources`,
  `supportsGeometryPass`, `supportsRenderPass`) with the reduced-pipeline paths in `RenderSystem`,
  `Engine`, `ChunksSystem` and `TextOverlayPass`, and the flags test. Runtime capture after removal:
  319 490 differing pixels, same validation profile as before.
- Every Vulkan frame recorded into an invalidated command buffer. `CascadedShadowPass`, `PointLightPass`
  and `GlassPass` rewrite binding 4 of their set per draw to swap the diffuse/mask texture while that
  set is bound in the list being recorded (714 rewrites per 240-frame capture; validation showed only
  13 messages because it hit its duplicate limit). OpenGL was fine: it captures a set's contents when
  the bind is recorded. Fixed in the Vulkan backend, not the passes: each list remembers the
  `VkDescriptorSet`s it bound, and `updateDescriptorSets` on a set bound in an open, pending or in-flight
  list gives the handle a fresh copy (`detachBoundSet`, `VkCopyDescriptorSet` of every binding) and
  writes that. Replaced sets are freed at the next `beginFrame` of the same slot. Same contract on both
  backends now: a bind captures the set.
- The present blit used one `m_blitDescriptorSet` for both frame slots and rewrote it every present while
  the other slot's present could still be pending (VUID-vkUpdateDescriptorSets-None-03047, intermittent;
  one run showed a 10k-pixel jump in the cross-backend diff). Fixed: one blit set per slot.
- Vulkan timestamps: fixed. The first list of a frame writes the start, every list writes the end and
  resets that single query first if it was already written this frame.
- Vulkan shader module cache: fixed with a reference count per shared module. Every create adds one,
  every destroy drops one, the module and its cache entry go at zero.
- Still in the runtime capture: one `vkQueueSubmit` image-layout expectation (SHADER_READ_ONLY expected,
  COLOR_ATTACHMENT found), and `vkCreateGraphicsPipelines` vertex attributes not consumed by the vertex
  shader. Not investigated.
- The suite's own validation errors are gone: the MRT test pipeline declared push constants for `Vertex`
  only while the shared draw helper pushed `Vertex | Fragment` (engine passes all match their
  declarations), and the clip-space depth test drew a colour-writing shader into a depth-only pass (now
  `rhi_test_depth_only.fs`). A suite run now logs only the deliberate negative-test messages.
- A sampled texture sampled before anything rendered into it was in the wrong layout on Vulkan. Textures
  created without data and with `ColorAttachment` usage started in `COLOR_ATTACHMENT_OPTIMAL`, but every
  render pass leaves its attachments in `SHADER_READ_ONLY`. `TAAPass` samples its second history
  texture on its first frame, before it was ever a target (VUID-vkCmdDraw-None-09600, once per start and
  again after each resize, when TAA recreates its textures). Fixed: a `Sampled` texture starts in
  `SHADER_READ_ONLY`. A runtime capture now has no FATAL validation message.
- Also open, harmless: `vkCreateGraphicsPipelines` warns that some engine vertex layouts declare
  attributes the vertex shader does not read.
- `compare-backends.ps1` now compares every intermediate stage, strictly: a stage is identical only with
  zero pixels past `-Threshold` (default 0). The old "noise" class (mean delta under 1) passed a final
  image with 11% of its pixels different.
- Capture determinism. There is no random number generator in the engine or scene setup (tints, phases
  and material keys are hashes of ids and positions), so run-to-run variation is timing. Two sources
  fixed for captures: render-pool tasks (per-pass culling) now run on the calling thread
  (`ThreadPool::setRenderTasksInline`), which removed a close caster's shadow vanishing between runs
  (OpenGL self-test went from 96% differing to 0 on one run); and the runtime waits for every worker pool
  to drain before each captured frame (`ThreadPool::waitIdle`). Measured afterwards: an OpenGL
  self-test has every stage identical, but its ordinary (non-dumping) final capture still differed by
  5 576 pixels on a second run; a Vulkan self-test differs in shadow cascades 1-2, where a whole caster
  is present in one run and absent in the other, plus 1-step TAA rounding. The pools are already idle at
  every frame start (measured), so what remains is parallel work inside a frame or GPU-side. Open.

## RHI code review, backend against backend

Read-only review of both backends by area. Ordered by effect on the engine's current frame. The depth
range claim from the review ("Vulkan stores z_ndc unconverted") is wrong: the convention tests pin
identical depth on both backends (Vulkan uses depth-clip-control with [-1,1]).

Affects the current frame:
- OpenGL overrides sampler texture units by GLSL name after linking (`OpenGLRHI.cpp:1706-1732`,
  `glProgramUniform1i` from a fixed table), while descriptor binds use unit = binding. Shaders whose names
  sit at other bindings read the wrong texture on OpenGL only: `taa.fs` worldPos (binding 2, forced to 1
  = historyColor), `ssao_blur.fs` gPosition (1, forced to 0 = ssaoInput), `contact_gi.fs` gNormal (3,
  forced to 1), `volumetric.fs` worldPos (0, forced to 1, nothing bound). Matches the stage comparison:
  SSAO blur diverged 8.6% while raw SSAO matched.
- Seamless cube filtering is off on OpenGL (no `GL_TEXTURE_CUBE_MAP_SEAMLESS`), always on in Vulkan: seams
  in IBL irradiance/prefilter lookups and point-light cube shadows on OpenGL.
- `DEPTH_COMPONENT24`/`32` are unsigned-normalised on OpenGL (`OpenGLRHI.cpp:111-112,294-295`) and both
  `VK_FORMAT_D32_SFLOAT` on Vulkan (`VulkanRHIVkUtil.h:22-23`): different depth precision in the G-buffer
  (24) and the CSM and point shadow maps (32).
- `GL_DITHER` is left at its default (on); Vulkan has no dithering. Driver-dependent.

Latent (no current engine caller):
- OpenGL: array and cube textures created with data get no mips (`glGenerateMipmap` only for Tex2D,
  `OpenGLRHI.cpp:2345-2355`); a data-less Tex2D with mipLevels > 1 allocates level 0 only but sets
  MAX_LEVEL to the full chain (incomplete).
- Vulkan: `cmdBlit` hardcodes mip 0 / layer 0 and blits only colour attachment 0
  (`VulkanRHIPipelines.cpp:1596-1628`); stencil load/store is always DONT_CARE (`:905-906`); readback of a
  non-RGBA8 array layer > 0 reads layer 0 then copies an invalid subresource (`:92`, `:127`); depth readback
  casts D16/D24S8 bytes to float (`:131,153`); sRGB readback decodes to linear on Vulkan but not OpenGL.
- `RenderPassBeginInfo` width/height 0: whole framebuffer on OpenGL, 1x1 on Vulkan. Pass clears cover the
  whole attachment on OpenGL, only the render area on Vulkan.
- Border colour (0,0,0,0) on OpenGL, (0,0,0,1) on Vulkan. Anisotropy clamped to the device limit on
  OpenGL only. Array layer count from `depth > 1 ? depth : arrayLayers` on OpenGL, `depth` only on Vulkan.
  A missing sampler leaves the previous unit's sampler bound on OpenGL.
- Vulkan keeps running the old `.spv` when shaderc rejects a changed shader (`VulkanRHIPipelines.cpp:19-21`);
  OpenGL compiles the current text. No such failure in the current log.
- OpenGL `capturePublishedFrame` downscales to 1280 wide with a linear blit; Vulkan returns full size.

## RHI divergences fixed from the review, each pinned by a test

- OpenGL sampler unit overridden by GLSL name after link (`DescriptorSamplerUnitFollowsBindingNotName`):
  table removed, `layout(binding)` decides the unit as on Vulkan. SSAO blur diff 8.6% -> 0.07%.
- OpenGL seamless cube filtering (`CubeLinearSampleAtAFaceEdgeBlendsTheAdjacentFace`): enabled at init.
- Depth format precision (`DepthFormatsResolveTheSameSubStepDepthDifferenceOnBothBackends`):
  `DEPTH_COMPONENT24/32` are `GL_DEPTH_COMPONENT32F` on OpenGL, matching Vulkan's `D32_SFLOAT`.
- OpenGL VAO configured while unbound (`ADepthArrayLayerKeepsTheDepthOfADrawWithAnIntegerVertexAttribute`):
  on Intel GL 4.6 `glVertexArrayAttribIFormat` applies to the bound VAO, so integer bone ids stayed a
  float and skinned vertices collapsed. The VAO is bound during setup.
- Engine, but the cause of the shadow cascade divergence: `Batcher` cached per batcher which buffer it had
  written to binding 3 of a set that four cascade batchers share. With one frame slot (OpenGL) the cache
  hit and cascades drew with another batcher's entity ids; with two (Vulkan) it always missed. Removed.
  Shadow cascades now match; Lighting 10% / mean 8.8 -> 1.3% / mean 0.005.
- Shader math (`ShaderMathMatchesAcrossBackendsExactlyWhereItIsIntegerBased`): float arithmetic is not
  bit-identical between the OpenGL driver's GLSL compiler and the SPIR-V path (contraction and common
  subexpressions differ; one `precise` moved which quantities matched). `fract(sin(x) * 43758.5453)`
  differed on a quarter of the pixels. The G-buffer coverage dither used it; it now uses `sfeHash12`
  from `sfe.glsl`, integer-only and bit-exact. `sky.fs` still has the sin hash but is not loaded.

Ruled out by tests (identical on both backends): generated mips (power-of-two), edge rasterisation and
shared-edge ownership, bilinear sampling at sub-texel positions, depth array layer render + readback.

Open: an intermittent run-to-run difference of one whole character on both backends (engine timing,
see the determinism notes above) still lands in single cross-backend comparisons and has to be
separated by repeated runs.
