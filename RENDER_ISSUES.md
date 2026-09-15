# Render issues

Known problems in the GPU-driven renderer, most urgent first.

## 1. Debug code in `bin/shaders/animateBones.cs` (working copy)
- [x] The `dbg` matrix is written to `outBones[entityId * MAX_BONES + MAX_BONES - 1 - boneId]`, which overwrites other bones of the same character. Remove it before committing.

## 2. The bones buffer is addressed by entity id
- [x] `RenderSystem::createSceneBuffers` reserves `kMaxArmatureBones` (256) matrices, 16 KB, for every entity slot, skeleton or not. At 4096 entities that is 64 MB, and the capacity doubles: 65k entities take 1 GB, 130k take 2 GB, even with ten animated characters. This hits the storage buffer size limit (2–4 GB on desktop, much less on mobile).
- Fix: a pooled range per skinned entity, sized by its bone count. The offset goes into the instance record and skinning reads `bones[boneOffset + boneId]`. Same incremental scheme as `GpuScene`. This also removes limit 3.

## 3. At most 256 bones per armature
- [x] Bones past `kMaxArmatureBones` are silently not animated (`AnimationComputePass` clamps `boneCount`, the CPU upload clamps too). There is no warning at load.

## 4. Skinned bounds are a static inflated AABB
- [x] The box is the bind pose plus a margin. A pose that reaches past it (a wide swing, a fall) gets the part that sticks out culled by frustum culling now, and by occlusion culling once it exists.
- Fix: compute the AABB per frame in `animateBones.cs` from bone world positions. Add a per-bone radius (the farthest vertex weighted to the bone in bind pose, computed once at load) scaled by the bone's world axes, and write the result into a bounds buffer the cull reads. Costs a few percent of a 0.1–0.2 ms pass and 24 bytes per character.
- Tried (cec243f, reverted in 1fe15ae): one pose box per entity cost 0.5–0.8 ms of GPU time on the character scene, measured with old and new runtimes alternated in rotating order. The compute was free; the loss was culling. Every submesh took the whole body's box instead of its own inflated one, so small submeshes (eyes, hair, clothing) passed far more views, the shadow cascades above all. For the vampire the pose box was 135×130×91 against a 135×106×34 mesh box.
- Done, as a box per submesh: at load every bone gets the distance of its farthest weighted vertex (`Armature::boneRadius`) and a bit per skinned mesh it moves (`boneBoundsMask`); `animateBones.cs` grows one box per skinned mesh from the posed joints into the matrices the bone pool keeps before the entity's bones (`RenderSystem::BoneRange::bounds`), and the mesh table's `skinned` field carries `2 + slot` so the cull knows which box to read. A pose uploaded from the CPU writes an empty box and keeps the inflated fallback. Tighter boxes now cull better than they cost: 7.98 to 7.77 ms of GPU time (rotated A/B, 6 rounds), against 0.5-0.8 ms lost by the whole-body box.
- Watch out: the box must be built from the joint matrix read back out of the `worlds` buffer, not from the local `world` variable. A second consumer of that variable made the OpenGL compiler evaluate the bone chain differently, `precise` and all, and skinned vertices landed a few ULPs off the Vulkan ones.

## 5. No occlusion culling
- [x] Nothing writes `OcclusionComponent::occluded`: the flag is always false. `AnimationComputePass` reads it, while `GeometryPass` and `CascadedShadowPass` only include the header.
- Fix: Hi-Z in `gpu_cull.cs`, camera views only. Use two-pass culling (cull with the last frame's pyramid, draw, build a pyramid from this frame, retest what was culled and draw what became visible) so moving occluders and fast camera turns do not pop. Delete the old `OcclusionComponent`.
- Done: `GpuScene::cullVisibleLastFrame` / `buildDepthPyramid` / `cullOccluded`, used by `GeometryPass` and, per cascade, by `CascadedShadowPass` -- a caster the light cannot see stands in another caster's shadow, so its own shadow adds nothing. The pyramid lives in a storage buffer (`hiz.cs`, the RHI has no storage images) and reads either a depth texture or one layer of the cascade array; the per-slot `lastVisible` flags need no CPU upkeep (a stale flag only picks the phase), and the second phase packs at the end of each batch's range so both phases share one visible buffer. Point lights are not occlusion culled.
- The default scene hides almost nothing behind anything (131 of 1284 instances visible either way), so there it costs about 0.05 ms and saves nothing. `--rooms` spawns the scene it is meant for -- walled rooms with a roof and characters inside -- and from the street 103 of 3042 instances survive against 394 without the test, the cascades cull from 938 down to 13 by cascade, and the frame's GPU time goes from 5.48 to 5.02 ms (rotated A/B, 6 rounds).

## 6. Animation still walks every character on the CPU each frame
- [x] `AnimationComputePass::render` iterates all animated entities, advances `mCurrentTime`, packs `AnimPush`, uploads it and dispatches once per clip.
- Fix: persistent per-entity records on the GPU (clip, start time, speed, loop, pause), updated on events only. The shader derives the time from one global clock. Put all clips in shared key buffers so one dispatch covers everyone. Anything on the CPU that reads `mCurrentTime` must use the same formula, and the start times need periodic rebasing to keep float precision.
- Done: every character has a record on the GPU (`AnimationComputePass::GpuRecord`) rewritten only on `ANIMATION_UPDATED`, `ARMATURE_UPDATED` or destroy, and the shader takes the time from `Engine::animationClock()` (`AnimationComponent::time()` is the same formula on the CPU). Clips share key buffers and one dispatch covers all records. The records are written in the new `RenderPass::prepare`, before the frame's first command list. Start times are relative to an epoch moved every 512 s. A start set off the main thread (`AnimationComponent::play`) is fixed on the first frame that draws it, so loader timing does not change the pose. Whoever edits the component sends `ANIMATION_UPDATED`. The occluded skip went with the per-frame walk; occlusion (item 5) has to bring its own.
- [x] One dispatch is capped at 65535 characters per clip (the minimum `maxComputeWorkGroupCount`). Now 16 records per invocation group over one dispatch for all clips.

## 7. `AnimationComputePass` leaks buffers on growth
- [x] When a clip's instance count grows, `instances` and `worlds` are replaced by new buffers and the old ones are never destroyed or retired. Clip buffers in `mClips` are never freed either, and the cache is keyed by `Animation*`.

## 8. The outline list is rebuilt from scratch
- [x] `GpuScene::rebuildOutline` repacks the whole list (`pack()`) instead of updating it through `markEntity` like the other lists. Usually only a few entities, but it is the last non-incremental list.

## 9. Portability (mobile, consoles)
- [ ] Hard requirements with no fallback: `multiDrawIndirect`, `drawIndirectFirstInstance`, storage buffers in vertex shaders (`vertexPipelineStoresAndAtomics`), sampler arrays with dynamic indexing, and compute.
  - Older and low-end Mali/Adreno lack some of these. Fallback: draw the indirect commands one by one in the RHI when the feature is missing.
  - The OpenGL backend needs 4.6, so there is no GLES path.
  - iOS only works through MoltenVK, where multi-draw-indirect is emulated. A native Metal backend would be better.
- Consoles need their own RHI backend (DX12, PS5) and shader translation (GLSL → SPIR-V → HLSL). The GPU-driven design itself maps directly.

## 10. Engine users cannot add their own render passes
- [x] The pass list is a compile-time array, `RENDER_PASSES` in `systemsModule/SystemsPriority.h`. `RenderSystem` builds its passes from it once, and the order is the array index. There is no public API to add a pass from game code or a plugin, or to put one before or after a named pass. `setPriority` is stored and never used for ordering.
- Already reusable: the `RenderPass` interface, `FrameBlackboard` (pass outputs shared by name), `RenderData`, and `GpuScene`'s public cull and draw calls.
- Fix: `RenderSystem::addPass(name, factory, after = "<pass name>")` on top of the default list. Blackboard names are plain strings, so a typo silently yields an empty handle; public names should be constants and a missing resource should warn.

## 11. `GpuScene` compacts and re-uploads every frame in scenes with many rarely used meshes
- [x] `GpuScene::update` checks holes on every frame (`GpuScene.cpp:161`), and `compact()` itself leaves `n/4 + 4` spare slots per batch (`:254`). With more than about 1024 batches averaging under ~5 instances each (a typical imported static level), the check fails right after compacting. GBuffer and Shadow then repack and re-upload their whole instance and batch buffers every frame. Batch slots emptied by `growGroup` keep their capacity and add to it.
- Fix: count holes against the spare that compact leaves, or check only after a frame that changed something.

## 12. Glass is sorted by the entity origin, not by its bounds
- [x] `gpu_glass.cs:118` uses `m[3]`. Every submesh of one entity gets the same key, and so does glass in imported models whose pivots sit at the origin. Ties fall back to the slot index, so blending order is wrong and flips after swap-remove.
- Fix: sort by the transformed centre of the mesh bounds (already computed a few lines above).

## 13. The cull runs one invocation per batch and loops over its instances
- [x] `gpu_cull.cs:109`. One mesh with 100k instances (foliage, debris) is a single GPU thread running a 100k loop, repeated for every cascade and the camera. No parallelism, and it risks a driver timeout.
- Fix: dispatch per instance and write through a per-batch offset, or split big batches.
- Measured: the serial loop was the most expensive part of the frame's GPU time. One cull of 1284 instances took 0.42 ms at 1024 instances per batch, 0.055 ms at 64, 0.02 ms at 16. `kMaxBatchInstances` is 16 now: with occlusion culling on, the default scene went from 10.24 to 8.89 ms of GPU time on Vulkan and from 12.73 to 10.49 ms a frame on OpenGL (rotated A/B, 6 and 4 rounds).

## 14. The material table only grows
- [x] `GpuMaterialTable.cpp:34`: entries are keyed by rounded parameter values and never removed. Dragging a roughness or tint slider in the editor adds an entry per step, and every step also calls `markEntity`.

## 15. The 8 glass texture slots are never freed
- [x] `GpuMaterialTable.cpp:48`: slots are taken per texture handle for the whole session. Once 8 are used, new glass draws untextured until restart, and a destroyed texture stays bound in its slot (`GlassPass.cpp:246`).

## 16. One dirty range per list
- [x] `GpuScene.cpp:377`: a remove in an early batch plus a batch growing at the end in the same update re-uploads nearly the whole instance buffer. Happens on edits, not every frame.

## 17. A new mesh re-uploads the whole mesh table
- [x] `GpuScene.cpp:191`: while meshes stream in, every frame that adds one copies and uploads all of them.

## 18. `TasksManager` listener lists are not thread-safe
- [x] `TasksManager.cpp:4-10`: `addWorker` appends to `workers[type]` on the main thread while `notify` iterates it on worker threads. `TransformSystem` is created first (`ECSHandler.cpp:56`) and can already send `TRAHSFORM_RELOADED` while `AABBSystem` and `RenderSystem` subscribe. A reallocation mid-loop reads freed memory, which is a possible crash at startup.

## 19. An entity can stay invisible when its mesh arrives before its transform
- [x] `GpuScene::addEntity` (`GpuScene.cpp:269`) silently skips an entity without `TransformMatComp`, and `TRAHSFORM_RELOADED` does not re-queue it (`RenderSystem.cpp:485`). Scene load through `fillTree` sends `MESH_UPDATED` before the transform worker adds the component. The entity is not drawn until its mesh or material changes. An entity left at the default transform never calls a setter and depends on the constructor's notify, which can be lost.
- Fix: `TRAHSFORM_RELOADED` for an entity without refs calls `markEntity`.

## 20. Data race on `TransformComponent`
- [x] The camera matrix was read on the main thread while the transform worker writes `mTransform` (`TransformComponent.cpp:94`); a pin prevents moves, not writes, so a moving camera could get a half-written view for a frame. RenderSystem now reads the camera from `TransformMatComp`, whose replacement waits for pins.
- [x] Setters, `reloadTransform` and a new `copyTransform` take a lock striped by entity (outside the component, which stays trivially copyable); cross-thread readers (RenderSystem, AABBSystem, Gizmo) use `copyTransform`, and spawning seeds the render copy from its own values instead of reloading the component beside the worker.

## 21. Descriptor caches compare handle ids, and Vulkan reuses ids
- [x] `CascadedShadowPass.cpp:336`, `PointLightPass.cpp:234`, `GpuScene.cpp:551/614` skip the set update when the ids are unchanged. `VulkanRHI::recycleResource` (`rhi/VulkanRHI.h:216`) reuses freed ids with no generation. A pass skipped while a buffer is replaced twice can see the same id and keep a set pointing at a destroyed buffer (undefined behaviour or device lost). Frequent outline buffer re-creation makes reuse faster.
- Fix: a generation counter in handles, or invalidate caches on retire.

## 22. Possible out-of-bounds transform read for one frame
- [x] `RenderSystem.cpp:456-481`: the dirty-list swap and `GpuScene::update` are not atomic against the transform worker. An id above `mGpuCapacity` that gets `TransformMatComp` after the swap is added to the scene in the same frame, so the cull reads past the transforms buffer. Fixed by the next frame's growth.

## 23. Shared mesh buffers never free anything
- [ ] `MeshVaoRegistry.cpp:57-79, 113-153`: allocation only appends, with no free or reuse. Offsets are `uint32` and wrap silently. Every growth re-uploads all meshes from their CPU copies, so all geometry stays in RAM forever. On Vulkan a growth mid-frame turns into one blocking submit per mesh.
- [x] Offsets past `uint32` are refused with an error instead of wrapping onto the start of the buffers.
- [x] A mid-frame growth would now show up in the log (see 30); meshes are uploaded by SYNC tasks outside frame recording.
- [ ] Freeing needs an asset lifecycle the engine does not have yet: models are never unloaded, so there is nothing to free. When they are, free ranges per mesh like the bone pool, and copy buffer to buffer on growth instead of keeping every mesh's CPU copy.

## 24. Retired buffers and the frames-in-flight constant
- [x] Retired buffers are freed only in `uploadDirtyGpuData`: never at shutdown (`~RenderSystem`, `RenderSystem.cpp:626`), and they pile up during long loads.
- [x] `VulkanRHI.cpp:22` defines an unused `kMaxFramesInFlight = 1`, shadowed by the class constant 2. `kMaxGpuFrames = 2` (`RenderSystem.h:284`) and the `min(2, …)` clamps silently assume 2. At 3, per-frame slots would be shared between frames in flight.

## 25. `GlassPass` creates a framebuffer every frame and never frees them
- [x] `GlassPass.cpp:124-140`: `ensureTargets` makes a new framebuffer whenever the Final or GBufferDepth texture id changes, and never destroys the old one. Both ids alternate every frame: Lighting ping-pongs its output (`LightingPass.cpp:322,338`), and on Vulkan the depth buffer alternates per frame slot (`GeometryPass.cpp:234,242`). That leaks one FBO/VkFramebuffer per frame, always on Vulkan and on GL with SSR/Sheen/SSS/ContactGI off.
- Fix: cache one framebuffer per (final, depth) pair.

## 26. `DebugPass` destroys a buffer the previous frame still uses (Vulkan)
- [x] `DebugPass.cpp:40`: `growDebugVBO` calls `destroyBuffer` directly, and `VulkanRHI::destroyBuffer` frees it immediately. Frame N-1 was submitted without waiting and still binds it, which means use-after-free or device lost when debug geometry outgrows the buffer. The RHI has no deferred destroy; only `RenderSystem::retireBuffer` works around it.
- Fix: deferred destroy in the Vulkan backend itself (free after `framesInFlight` frames), so no caller has to remember.

## 27. Shared CPU-writable buffers are overwritten while the previous frame reads them (Vulkan)
- [x] `VulkanRHI::updateBuffer` on a mapped buffer is a plain memcpy, and a frame only waits on its own slot's fence. RenderSystem and GpuScene keep one buffer per frame slot, but these do not: `CascadedShadowPass.cpp:298` (cascade matrices), `PointLightPass.cpp:227`, `LightingPass.cpp:245,293,302,320`, `GlassPass.cpp:257`, `AnimationComputePass.cpp:279` (instances). Frame N-1 can render with frame N's cascade matrices (shadows swim while the camera moves), or skin with the wrong entity ids. GL is unaffected.
- Fix: per-slot buffers for these, or have the RHI rotate per-frame copies of `CpuAccess::Write` buffers.

## 28. The Vulkan descriptor pool has a fixed size and fails silently
- [x] `VulkanRHIResources.cpp:248-258`: 2048 sets and 2048 descriptors per type, never grown. GeometryPass keeps a set per texture combination forever (`GeometryPass.cpp:409`, 6 images each, so ~340 combinations exhaust images), and shadow mask sets are never freed. When the pool is full, `allocateDescriptorSet` returns an empty handle without a log and the bind is skipped silently, so the draw uses the previous group's textures. `detachBoundSet` then writes a bound set in place, which invalidates the command buffer.
- Fix: a list of pools that grows, and a log on failure.

## 29. Sets rewritten every frame (Vulkan)
- [x] Writing a set that the previous frame still binds allocates a new set and copies it (`VulkanRHIResources.cpp:999-1046`). This happens every frame in `GlassPass.cpp:182-254` (12 writes, no change check), `GeometryPass.cpp:291-311` and `:344`, LightingPass `mTexSet`/`mUboSet` (inputs alternate per slot), and `AnimationComputePass.cpp:280` (per clip). Correct, but costly, and it feeds item 28.
- Fix: one set per frame slot where inputs alternate, and change checks.
- [x] Vulkan now skips a write that repeats what the binding holds (compared by handle id, offset and range), so passes without their own change check no longer copy sets. Measured on the character scene: 11 copies a frame before, 9 after.
- [x] The remaining 9 are post passes whose inputs really alternate every frame (Lighting's ping-pong output, the per-slot G-buffer and camera UBO). Each has one set per frame slot now -- Lighting, Glass, the Geometry outline sets, SSAO, Sheen, SSS, SSR, TAA, ContactGI, DoF and Final -- and the frame copies no sets at all (9 before).
- Fixing it turned up a bug: SSAO wrote the G-buffer textures into its sets once at init, so on Vulkan, with two frames in flight, every other frame read the other slot's G-buffer. With the sets per slot the backends agree far more closely than before: the final image went from 2.52% of pixels differing to 0.024%, TAA from 1.66% to 0.014%. The OpenGL capture is unchanged, the Vulkan one is what moved.

## 30. Vulkan buffer writes during recording stall the frame
- [x] After uploads are flushed at `beginCommandList`, `updateBuffer` to a device-local buffer and `cmdUpdateBuffer` run `executeImmediate`: submit and wait on a fence (`VulkanRHIResources.cpp:131-143`, `:1362`). `GpuMaterialTable::upload()` is called from inside passes (`GeometryPass.cpp:263`), so a new material makes the GPU wait mid-frame. Copies queued earlier run at submit, after the immediate one, so the order of two writes to the same range is inverted compared with GL (no current caller does that).

## 31. Allocations per draw and per frame
- [x] GL `cmdBindDescriptorSets` deep-copies the bindings' vectors on every bind (`OpenGLRHI.cpp:1420`), the command list starts from zero capacity every frame, and push constants allocate. Vulkan allocates a vector per bind (`VulkanRHIPipelines.cpp:1389`), per render pass (`:1236`) and 4 per `updateDescriptorSets`. Passes allocate too: GlassPass `writes`, AnimationComputePass `unordered_map`, PointLightPass `matrices`/`spheres`/`candidates`, `GpuScene::cullSpheres` `shapes`.

## 32. `GlassPass` writes bindings its layout does not have
- [x] `GlassPass.cpp:197-203, 232-238` write bindings 12 and 14, which the layout (`:77-87`) lacks. Both backends log "set has no binding" twice per frame, and the log spam hides real errors.

## 33. Smaller backend differences
- [x] `RenderSystem.cpp:573` binds the frame set with an empty pipeline. Vulkan drops it and GL applies it, so GL's persistent binding can hide a missing bind that only breaks on Vulkan.
- [x] Not verified: Vulkan `runOnWorkerThread` has no lock, and `TextureHandler.cpp:75` calls `createTexture` on the calling thread. If textures load off the main thread, the resource arrays and the upload command pool are used from two threads.

## 34. Housekeeping
- [x] `bin/compare/**` outputs are tracked in git, so every `compare-backends.ps1` run dirties the tree. Ignore them.
- [x] `scripts/compare-backends.ps1` (editor repo) is untracked.
