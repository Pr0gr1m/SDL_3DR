# GPU Resources and Frame Loop Findings

## TL;DR
- The whole visible world is re-flattened, converted to 60-byte, non-indexed Vertex3D data, and uploaded through a NEW transfer buffer every frame (App.cpp:1648-1681), even when nothing changed. Draws: 1 scene + 1 (empty) line draw for the whole world.
- The chunk mesh cache is CPU-only (`chunk.cache.lodBlocks`). It is deep-copied every frame (1396, 1554), re-flattened to std::vector<Face>, then to Vertex3D, then re-uploaded; the cache never reaches the GPU.
- Static GPU resources (pipelines, samplers, colour/normal textures, depth) are created once and are fine. Gaps: single texture pair for all blocks, no mipmaps, duplicate samplers, fixed-size depth/viewport, no frames-in-flight configuration.
- Cache correctness hazard: frustum-culled sub-blocks are baked into the cache (1506 then 1553), while cache hits (1394) ignore camera changes.

## Key Decisions
- Object.h, TextureManager.*, ShaderHelper.h are in src/core/ (not src/). Line numbers are for the current working tree.
- `canCreateVertexBufferEveryFrame = false` (App.cpp:42), so the scene vertex buffer is sized once (see overflow risk).

## Findings (file:line)

### Per-frame upload path (OnRender, App.cpp:1311-1756)
1. Command buffer: one per frame (acquire 1312, submit 1752). Swapchain via `SDL_WaitAndAcquireGPUSwapchainTexture` (1686), which blocks until an image is free. Present mode MAILBOX if supported else VSYNC (232-239). `SDL_SetGPUAllowedFramesInFlight` is not called (driver default). No per-frame resource ring.
2. Transfer buffer created and released every frame: Create 1651, Map (cycle=true) 1658, memcpy 1666, Unmap 1667, `SDL_UploadToGPUBuffer(cycle=true)` 1679, Release 1681. Size is the entire scene (`vertexDataSize`, 1585). Should be a persistent or ring transfer buffer, or upload only dirty chunks.
3. Double CPU copy: `std::vector<Vertex3D> verticies(N)` allocated every frame (1583), filled by par_unseq loop (1608-1612), then memcpy'd into mapped memory (1666). Could write straight into mapped memory.
4. Vertex layout: Vertex3D = Position 12B + SDL_FColor 16B + Normal 12B + UV 8B + Tangent 12B = 60B (core/Vertex3D.h:11-17). Colour is never read by the fragment shader (fragment.glsl:2 declares v_color input? no: it is declared in vertex.glsl:280,290 only). Normal and tangent are constant per face but stored 3x. No index buffer (`SDL_DrawGPUPrimitives` at 1745). Per block: 12 tris x 3 verts x 60B = 2160B.
5. `Face::GetFaceDrawCallVerticies` (core/Face.cpp:3-40) recomputes normal/tangent/UV for every face every frame; output order is nondeterministic (atomic `cpyIndex.fetch_add(3)`, 1609-1610) but harmless.
6. Buffer capacity: created once at first-frame size (1615-1621), never resized because of the flag at 42/1627. If a later frame needs more vertices than the first, the upload at 1679 exceeds the buffer (RISK). If the flag is enabled, the resize path calls `SDL_WaitForGPUIdle` (1636) = full GPU stall, and releases the old buffer.
7. No dirty flag: the upload happens every frame, even with an unchanged camera/world.

### Draw calls / batching
- Exactly 2 draws: scene `SDL_DrawGPUPrimitives(totalVertexNumber,1,0,0)` (1745) and line-pipeline draw (1746-1747). Not per chunk, so call count is minimal, but there are no per-chunk GPU buffers, so no chunk-level GPU cache, GPU-side culling, or partial update.
- Line pass is dead: `totalLineVertexNumber` is declared (1356), read (1573, 1747) and never incremented; the pipeline rebind plus a 0-vertex draw is wasted.
- Hidden-face culling disabled: `chunkFaceCulling = false` (App.cpp:41). The code at 1443-1480 sets neighbours present (does not test occlusion) and the z checks use `x != 0` / `x != N-1` (typo). Every visible block emits all 12 tris (1523-1535; `fetch_add(36)` at 1541). Greedy meshing is a stub (only a log, 1567-1569; timing label measures nothing).
- Uniforms: one `SDL_PushGPUVertexUniformData` of 64B viewProjection per frame (1727-1732). Fine. `uniformBuffer` (App.h:133) is only released (App.cpp:737), never created (dead member). `getMeshDrawCallVerticies` lambda (1334) unused.

### Resource lifetime / caching (init ~217-540, textures 791-1085)
- Pipelines created once (483-484), shaders released after (493-495). SPIR-V only (App.h:37-40, Dxil unused).
- Depth texture created once at `defaultScreenWidth/Height` (405-421); viewport fixed (1721). No resize handling found.
- Samplers: two identical samplers from one info (1066-1076); one would do. NEAREST filter, `max_lod=1.0` but `num_levels=1` (847, 857): no mipmaps, aliasing and poor texture-cache locality at distance.
- Texture upload is one-shot at init: separate transfer buffers (893-894), two copy passes (974, 1010), one submit (1046), released (1059-1060). Not per frame.
- BUG: `SDL_DestroySurface(converted*)` runs at 951-952 and again at 1062-1063 (double free of the same surfaces).
- Single colour + single normal texture (src/img/colour.jpg, normal.jpg) for the whole world. TextureManager (core/TextureManager.cpp:5-53, .h:94-97 BlockType {Dirt, OakLog}) supports per-block paths but `UploadDirtTexturesToGPU` is hardcoded. No atlas / texture array; Vertex3D has no per-vertex texture index; UVs are world-projected (Face.cpp:20-40), tiling via REPEAT.
- TextureManager has no surface cache: every call re-reads and re-decodes a JPG (5-34) and builds strings. `blockTypeToPath[blockType]` (37) inserts on miss. Fine for init-only use; a gap if used at runtime.
- Object.h / Mesh.h are legacy CPU classes (raw pointers, commented-out rotation), not on the GPU path. ShaderHelper.h has only path constants that duplicate App.h:144-145.

### Shaders (src/shaders)
- vertex.glsl: pass-through with one row_major mat4 UBO (set 1, binding 0), lines ~1-65 are commented-out dead code, live code is the last ~25 lines (a_color, v_color unused downstream).
- fragment.glsl: 2 texture fetches, TBN build with 3 normalizes + cross per fragment (~lines 12-20), and `normalize(vec3(-0.35,0.85,0.45))` per fragment (constant; precompute). Normal/tangent are constant per face, so TBN could be per-vertex/precomputed.
- Colour target has blending enabled (App.cpp:395-403) though output alpha is 1.0: unnecessary blend cost.
- .spv binaries are committed alongside GLSL; must be rebuilt manually (build step is other category).

### CPU cache feeding the GPU path (OnRender 1363-1556)
- Hit path (1394-1398) still copies the vector: `chunkLODBlocks[idx] = chunk.cache.lodBlocks` (deep copy, each LODDBlock = 12 Faces). Miss path copies again (1554). Should reference or move.
- Allocation before cache check: `chunkLocalBlocks.reserve(chunkSize^3)` (1391) and the `blockPresence` array are created for every frustum-visible chunk even on a cache hit (1394 comes after).
- Suspected inverted logic: `if (isValidCache) rebuild presence else blockPresence = cachedBlockPresence` (1418-1432). On first build the cache is invalid and an unset cached array is copied.
- Cache keyed only on LOD (LOD hardcoded to 0 at 1388) but content depends on the camera frustum (1506 `continue` before `emplace_back`, cached at 1553): after camera moves, hits return stale visible-set (missing geometry) or, if invalidated elsewhere, caching is pointless.
- Chunk-level frustum test (1370-1385) is correct and cheap. Empty-chunk return (1414) is after the cache check.
- Per-frame aggregation: `nonFrustrumCulledFaces` rebuilt with insert loops (1592-1606): scene flatten -> Face vector -> Vertex3D vector -> transfer buffer -> GPU (4 full-scene passes).
- Iterate (547-560): Update then Render serially on main thread; CPU meshing and GPU are not overlapped. `totalFPS/numFPS` (App.h:147-148) lack initializers.
- Logging: SLog1/SLog2 are runtime-gated (GlobalVariables.h:17-18) but `SDL_GetTicks` timing and `GetLevelOfDetailFromDistance` SLog2 (App.h:108) run regardless.

## Ranked gaps (GPU/frame)
1. Full-scene re-flatten + re-upload every frame with a new transfer buffer; no dirty tracking or persistent buffers.
2. No per-chunk GPU meshes; CPU cache copied by value each frame.
3. Hidden-face culling off, greedy meshing absent, non-indexed 60B vertices (12 tris/block).
4. Cache stores camera-dependent data, and allocates before checking the cache.
5. Fixed-capacity vertex buffer (overflow risk) / WaitForGPUIdle if enabled.
6. Fragment shader recomputation, blend on, no mips, duplicate samplers, single texture (no atlas), no depth resize, double DestroySurface.

## Open Questions / Risks
- Chunk invalidation triggers (Chunk.h, ChunkCache.h, ChunkManager.h) are outside this category; needed to confirm the stale-cache issue.
- Static analysis only; no profiling data. Whether the buffer-overflow risk (item 6) manifests depends on world edits changing vertex counts after frame 1.
- fragment.glsl / vertex.glsl line numbers are approximate.
