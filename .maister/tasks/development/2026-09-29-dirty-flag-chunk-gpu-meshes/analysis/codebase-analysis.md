# Codebase Analysis Report

**Date**: 2026-09-29
**Task**: Dirty-flag per-chunk GPU mesh design for SDL_3DR
**Description**: Implement the researched dirty-flag per-chunk GPU mesh design (in-app FrameProfiler timers + fixed benchmark camera path; dirty-flag chunk mesh replacing Chunk::cache Face cache; per-chunk persistent SDL_GPUBuffers with pooled transfer buffer + regrow; neighbor-aware cross-chunk hidden-face culling; keep optional<Object>). Type: enhancement/refactor.
**Analyzer**: codebase-analyzer skill (2 Explore agents: File Discovery + Code Analysis, Context Discovery)
**Research context**: `.maister/tasks/development/2026-09-29-dirty-flag-chunk-gpu-meshes/analysis/research-context/`

---

## TL;DR
- The whole render path is one ~450-line function, `App::OnRender` (App.cpp:1311-1758). It meshes every frame, copies all faces into one vertex array, and uploads through a fresh transfer buffer. The redesign restructures this function.
- The only runtime block mutation is the E-key removal (App.cpp:639-660), so there is a single explicit dirty hook plus boundary-neighbour dirtying.
- Existing cache (`ChunkCache.h`) and `chunkFaceCulling` are buggy or inert and are deleted, not migrated.
- No tests and no profiler exist. Build type is likely unoptimised, so benchmarks must state their build config.
- Pre-existing worldgen bugs (z-position, duplicate chunk keys) undermine neighbour-culling verification.

## Key Decisions
- Remove `ChunkMeshCache`/`LODDBLock` and the LOD cell-merging path (unused at LOD0) — the cache is invalid-by-design (uninitialised presence, inverted condition at 1418) and LOD is hard-coded 0.
- Keep GPU handles outside `Chunk` (store by chunk index or Int3 in App), or make the handle move-only — `Chunk` is copied by `worldChunks.push_back` (1214) and moved on `emplace_back` reallocation, so a raw `SDL_GPUBuffer*` member would be double-owned.
- No custom deferred-release queue — SDL defers `SDL_ReleaseGPUBuffer`/`TransferBuffer` until command buffers using them finish. This contradicts the deferred-release risk in the design.
- Emit chunk-local vertices (0..16) plus a per-chunk offset via the vertex uniform. Recommended option (b): combined `{mat4 viewProjection; vec4 chunkOffset;}` (80B), keeping `num_uniform_buffers=1` (App.cpp:264). Requires rebuilding the committed `vertex.spv`.
- Remesh single-threaded (or one task per chunk with read-only neighbour access) before the render pass. Edits run on the main thread and never overlap `OnRender`, so a plain `bool meshDirty` suffices.
- Neighbour culling uses `blocks[chunkIndex]` in-chunk and `chunkMap.find(atPosition ± N)` cross-chunk. Add a lowerCamel neighbour helper on `ChunkManager`.
- Use `SDL_GetPerformanceCounter` for FrameProfiler; `SDL_GetTicks` (1ms) is too coarse. Existing SLog timing is compiled out (debugLevel=0), so profiler output must use unconditional logging or its own reporter.

## Open Questions / Risks
- Worldgen bug at App.cpp:1192 (z uses `atPos.y`) puts z=16 chunks at z=0. `chunkMap.insert` silently drops duplicates, so map and vector disagree. Effective world is 2 distinct positions per y-level, so cross-chunk border culling is hard to verify until fixed. Decide whether to fix (in scope for correctness) or work around.
- UV dependence on vertex position (`chooseFaceUV` in Face.cpp). Chunk-local vertices change UVs unless the sampler repeats (chunk origins are multiples of 16, so probably identical). Verify sampler address mode in `UploadDirtTexturesToGPU` (App.cpp:791-1085).
- Chunk frustum AABB (1367-1380) uses atPosition..+N, but blocks are centred on integers, so the AABB should be offset by -0.5.
- Empty chunks: skip buffer creation and draw when the vertex count is 0.
- Buffer regrow: currently guarded by `canCreateVertexBufferEveryFrame=false` (App.cpp:42), so growth overflows. Per-chunk regrow must be correct, and `SDL_WaitForGPUIdle` (1636) must not be reused per chunk.
- Benchmark noise: swapchain acquire (1686) happens after CPU work; VSYNC caps FPS. Set MAILBOX or a fixed present mode, and note the build type (no CMAKE_BUILD_TYPE, no flags).
- FPS bookkeeping bugs: `totalFPS`/`numFPS` uninitialised (App.h:147-148), integer accumulation, and div-by-zero at Quit:725 if `numFPS==0`.
- Toolchain uncertainty: MinGW/libstdc++ vs MSVC. `std::execution::par` may be serial without TBB (not in vcpkg.json).
- Fixed 1920x1080 viewport/depth (1721, 406-426): benchmark must not depend on resizing.
- Zero tests and no test framework: verification is by profiler numbers, visual comparison and optional debug validation (`SDL_CreateGPUDevice` debug=false at 217).
- Stray build logs in repo root (build-last.txt, app_compile.err, manual_cxx.out): do not commit.

---

## Summary

SDL_3DR is a monolithic SDL3 GPU voxel renderer whose `App` owns a `vector<Chunk<16>>` and `std::map<Int3,size_t>`. Every frame it frustum-culls, builds `array<Face,12>` per block into a per-chunk cache (which freezes frustum culling per block), flattens all faces into one CPU vertex vector, and re-uploads it via a new transfer buffer. Replacing this with dirty-flag per-chunk meshes and persistent GPU buffers is a contained but invasive refactor of `OnRender`, `Chunk`, and one shader.

---

## Files Identified

### Primary Files (src/)

**App.cpp** (1758 lines)
- Init/pipeline 150-545; events and E-key edit 563-719; Quit 721-783; OnUpdate 1087-1118; `ConstructChunkAtLine` worldgen 1120-1217; raycast 1219-1305; OnRender 1311-1758.
- Contains all integration points: mesher (1435-1545), flatten (1583-1612), upload (1614-1682), render pass (1685-1750), edit hook (after 660), chunk creation (649-652, 1213-1216), camera and Iterate hooks (547-561, 1093), and release in Quit (732-734, before 773).

**App.h** (149 lines)
- GPU members 127-140 (sceneVertexBuffer, lastSceneVertexBufferDataSize, sceneVertexBufferSize; unused uniformBuffer at 133); FPS fields 147-148; `deltaTimeMS` at 54.

**core/Chunk.h** (94 lines)
- Template `Chunk<N>`: atPosition, `array<optional<Object>,N^3>`, `didUserEditChunk`, `ChunkMeshCache cache` (line 26), `chunkIndex=(y*N+z)*N+x` (38-40), `tryInsert` (42-46), mutable `operator[]`/`tryGet` (57-70), `isEmpty(int LOD)` reads cache (77-81).
- Add `meshDirty` (default true); remove cache and the LOD overload.

**core/ChunkCache.h** (30 lines) — `LODDBLock` and `ChunkMeshCache`. Delete (also CMakeLists.txt:32 and Chunk.h:6, 25-26, 77-81).

**core/ChunkManager.h** (44 lines)
- `chunkSizeXYZ=16`, LOD constants (20-35), `worldChunks` (38), `chunkMap` (41). No methods; add a neighbour/lookup helper here.

**core/Face.h / Face.cpp**
- `GetFaceDrawCallVerticies` (Face.cpp:3-81) produces vertices with rotation, normal, tangent and UV. `GetFaceLineCallVerticies` is unused.

**core/Vertex3D.h** (11-19) — 60B packed layout: Position, Color, Normal, TexCoordU/V, Tangent. Add a `static_assert` on size.

**shaders/vertex.glsl** (79 lines; live code 55-79, lines 1-53 are commented old shader) — UBO at 63-65. Must gain a chunk offset. `.spv` files are committed and rebuilt via glslangValidator (CMakeLists.txt:46-67).

**CMakeLists.txt** (80 lines) — explicit source list (6-33, no glob): add new files (FrameProfiler, mesher) here and remove ChunkCache.h.

### Related Files

- **GlobalVariables.h** (75): `debugLevel=0` constexpr, SLog1/SLog2 macros (compiled out), constants `kMoveSpeed` and `kBlockHalfExtent`.
- **core/Camera.h/.cpp**: benchmark path drives `Position`, pitch/yaw, `UpdateDirectionVectors()`. Forward vector at Camera.cpp:121.
- **Int3.h**: lexicographic `operator<`, weak hash.
- **Object.h, Mesh.h, Ray.h**: rendering uses block presence only.
- **fragment.glsl**: 2 samplers, unaffected apart from UV check.
- **CMakePresets.json**: single "vcpkg" preset with no build type.

---

## Current Functionality

Per frame, `OnRender`:
1. Acquires a command buffer and updates frustum corners (1312-1318).
2. Builds viewProjection (row_major) into `float16Array` (1320-1325).
3. Runs `std::for_each(par)` per chunk (1363-1558):
   - chunk AABB vs frustum;
   - the cache-hit path copies `lodBlocks`;
   - otherwise it computes presence, iterates y/x/z, per-cell frustum tests (which freeze into the cache), and builds `array<Face,12>` per block in world space (`+ chunk.atPosition`).
4. Flattens Faces into `vector<Face>` then `vector<Vertex3D>` (zero-initialised each frame) with a `par_unseq` loop using an atomic index (1583-1612). This is nondeterministic in order and technically UB.
5. Uploads: a persistent scene buffer sized to the first frame (regrow path waits GPU idle); a fresh UPLOAD transfer buffer each frame, released immediately (1614-1682).
6. Acquires the swapchain texture (1686), runs one render pass with one draw for all vertices (1724-1748), then submits (1752).

### Key Components/Functions

- **Face order per block**: indices 0-1 top +Y, 2-3 bottom -Y, 4-5 +X, 6-7 -X, 8-9 +Z, 10-11 -Z. The new mesher must preserve winding and normals, since cull mode is NONE but normals are used in lighting.
- **`chunkFaceCulling`** (constexpr false, line 41; block 1443-1478): only sets neighbour presence true and has z-guard typos (1467, 1473). It is not real culling. Replace with per-direction neighbour tests.
- **Helpers** (App.cpp anonymous namespace 64-86): `BlockCoordinateFromPoint`, `ChunkCoordinateFromBlockCoordinate`, `BlockPositionFromPoint`, `ChunkLookupFromBlockPosition`. Reuse them for neighbour resolution.
- **Mutation paths**: E-key edit is the only runtime mutation (`blockAt->reset()` at 659-660). It may lazily create chunks (648-652, `emplace_back` can reallocate and move all chunks). The add path is commented out (671-698). Worldgen goes through `tryInsert` (1205) and copies chunks in at 1213-1216. `operator[]`/`tryGet` hand out mutable access and cannot mark dirty.

### Data Flow

Worldgen -> `worldChunks` and `chunkMap` -> (per frame) cull -> face cache -> flatten -> one vertex buffer -> one draw.
Target flow: mutation/creation -> `meshDirty` -> remesh dirty chunks (plus boundary neighbours) into chunk-local `Vertex3D` -> pooled transfer buffer -> per-chunk persistent GPU buffer -> per-chunk draw with offset uniform.

---

## Dependencies

### Imports (What This Depends On)

- Chunk.h: cassert, optional, ChunkCache.h, GlobalVariables.h (SDL log macros), Object.h, Vector.h.
- ChunkCache.h: Face.h only.
- SDL3, SDL3_image, SDL3_ttf (linked, unused in code read), glslangValidator (REQUIRED at configure).
- SDL is confined to App.cpp, TextureManager.cpp and GlobalVariables.h (log macros). The core types are SDL-free.
- App.cpp uses `<execution>`; `<chrono>` is included but unused.

### Consumers (What Depends On This)

- **App.cpp** OnRender (1338-1611): every use of `LODDBLock`, `chunkLODBlocks`, `Face`, `cache`.
- **Chunk.h:78**: `isEmpty(int LOD)` is the only non-App consumer of the cache.
- **Vertex layout**: pipeline and line-pipeline vertex input (354-448) use `offsetof(Vertex3D, ...)`; keep the layout unchanged.
- **Texture upload** (885-1063) uses separate transfer buffers; unaffected.

**Consumer Count**: about 2 files for the cache (App.cpp, Chunk.h); the vertex layout is used in 2 pipelines.
**Impact Scope**: Medium — contained to App.cpp/Chunk.h/ChunkManager.h/shader/CMake, but App.cpp changes are dense.

---

## Test Coverage

### Test Files

- None. No `enable_testing`/`add_test`, no test framework in vcpkg.json (sdl3 vulkan, sdl3-image jpeg, sdl3-ttf).

### Coverage Assessment

- **Test count**: 0 (0% coverage).
- **Gaps**: everything, including chunk indexing, raycast and meshing.
- Meshing logic is inline in the lambda at App.cpp:1363-1560. Extract it into a pure function (chunk plus neighbour accessor -> vertices) to make it testable. `Chunk`, `Face`, `Vector`, `Matrix`, `Int3` and `Vertex3D` are header-only and SDL-free, though `Chunk.h` includes `GlobalVariables.h` (SDL log macros). A test route needs a static lib plus Catch2/GTest via vcpkg and `enable_testing()`; this is optional scope, and the project standards favour risk-based testing of critical paths (mesher).

---

## Coding Patterns

### Naming Conventions

- **Types**: PascalCase; `template<int N> class Chunk`.
- **Members**: camelCase (only `m_Window` and `m_gpuDevice` are prefixed).
- **Methods**: PascalCase in App/Camera; camelCase in Chunk (`tryInsert`, `tryGet`, `isEmpty`). New Chunk/ChunkManager code is lowerCamel; new App methods are PascalCase.
- **Constants**: `kCamelCase` in GlobalVariables.h; file-local `inline constexpr` camelCase in App.cpp.
- **Files**: PascalCase; include guards `#ifndef SDL1_<NAME>_H` in core, `#pragma once` in App.h/GlobalVariables.h.
- **Identifier typos are preserved** (Frustrum, verticies, Dirt); copy the existing spelling for consistency.

### Architecture Patterns

- **Style**: procedural with heavy `static_cast`, 4-space indent, K&R braces, single-line early-return `if`. Doxygen `///` member docs.
- **Error pattern**: `SDL_LogError(APP_LOG_CATEGORY_GENERIC, "msg: %s", SDL_GetError())`, release partial resources, submit the open command buffer, return FAILURE.
- **Resources**: raw `SDL_GPU*` members initialised to nullptr and released explicitly in `App::Quit` after `SDL_WaitForGPUIdle`. Per-chunk buffers must be released before `chunkManager.reset()` (773).
- **Standards**: no commented-out code, no dead code, no speculative abstractions, minimal comments. Delete exploration artifacts (dead lambdas 1334-1353, unused `Face` line functions if touched).

---

## Complexity Assessment

| Factor | Value | Level |
|--------|-------|-------|
| File Size | App.cpp 1758 lines (OnRender ~450) | High |
| Dependencies | SDL GPU, shaders, CMake, glslang | Medium-High |
| Consumers | ~2 files (cache), 2 pipelines | Low-Medium |
| Test Coverage | 0 tests | High risk |

### Overall: Moderate-Complex

The change is mostly restructuring one large function, plus a shader change, an ownership design for GPU handles under `vector<Chunk>` reallocation, and new profiling infrastructure. The rest is small.

---

## Key Findings

### Strengths
- Clear single mutation point (E-key) and a single-threaded edit/render model.
- `chunkMap` already keyed by chunk position, so neighbour lookups are cheap.
- SDL's deferred release removes the need for a custom release queue.
- Vertex layout is stable and uses `offsetof`.

### Concerns
- Pre-existing bugs: worldgen z (1192), duplicate map keys, inclusive `PositionInBounds` (Chunk.h:89-91), noise ignoring the chunk offset, inverted presence condition (1418), uninitialised `cachedBlockPresence`.
- Per-frame heap churn: zero-inited vertex vector, ~72B Face copies, per-frame transfer buffer.
- Existing timing logs are compiled out or measure nothing (the "Greedy meshing" log at 1564).
- Possibly serial `par` policy and unoptimised build make the baseline unrepresentative.

### Opportunities
- Chunk-local vertices improve float precision and allow reusing meshes across frames.
- Only 4 x-z chunk positions at startup, so a dirty-flag scheme removes nearly all steady-state CPU meshing.
- Deterministic vertex order once the atomic `par_unseq` flatten is gone.

---

## Impact Assessment

- **Primary changes**: App.cpp (OnRender rewrite, edit hook, chunk creation, Quit release, camera/Iterate benchmark hook, FPS fixes), App.h (GPU store, profiler member), Chunk.h (dirty flag, remove cache), ChunkManager.h (neighbour helper), vertex.glsl and vertex.spv (chunk offset), App.cpp:264 (uniform buffer count if option (a)), CMakeLists.txt.
- **New files**: FrameProfiler, and optionally a mesher unit, registered in CMakeLists.txt.
- **Deleted**: ChunkCache.h, cache uses at OnRender 1390-1399 and 1547-1556.
- **Test updates**: none exist; optional mesher tests would require new CMake test infrastructure.

### Risk Level: Medium

Contained scope and a single mutation path lower the risk. Risk is raised by zero tests, GPU resource ownership under vector reallocation and copy, pre-existing worldgen bugs that hide border-culling errors, shader binary regeneration, and unreliable baseline timing (build type, present mode, timer resolution).

---

## Recommendations

**Approach (modifying existing code)**
1. Staging order: (a) FrameProfiler with `SDL_GetPerformanceCounter` timers around 1363-1558, remesh, 1648-1681, 1685, 1714-1750, 1752, plus a frame-counter-driven benchmark camera and fixed present mode. Capture the baseline first. (b) Add `meshDirty` and neighbour helper; extract the mesher to emit chunk-local `Vertex3D`. (c) Per-chunk persistent GPU buffers with pooled transfer buffer and grow-on-demand (release old buffer immediately, no idle wait). (d) Neighbour-aware culling and boundary dirtying. (e) Delete cache/LOD code and dead lambdas.
2. Dirty hooks: set in `tryInsert` (covers worldgen), initialise `meshDirty=true` for new chunks (649-652, 1213-1216), and mark explicitly after `blockAt->reset()` (line 660). Also mark neighbours when the local coordinate is 0 or N-1. Newly created chunks also dirty their existing neighbours, since a neighbour's boundary faces change when a chunk appears.
3. GPU handle ownership: keep the handles in an App-side container aligned with chunk identity (Int3 key or stable index), not inside `Chunk`. Release in `Quit` before `chunkManager.reset()`.
4. Transfer: one pooled transfer buffer, grown as needed, with a single copy pass for all dirty chunks per frame. Use cycle semantics deliberately; SDL cycles if the buffer is bound in an in-flight command.
5. Shader: add the chunk offset (option (b)), rebuild `vertex.spv`, and push the uniform before each chunk draw. Confirm the UV path (sampler repeat) before committing.
6. Culling: fix the chunk AABB half-block offset; treat a missing neighbour chunk per the research policy (documented in decision-log.md).
7. Follow standards: no commented-out code, no future stubs, lowerCamel in Chunk/ChunkManager, PascalCase App methods, error pattern with command-buffer submit on early return.
8. Verification: compare profiler numbers to baseline under the same build config, visually diff screenshots on the fixed camera path, and (temporarily) enable device debug for validation. Fix the FPS bookkeeping so results are trustworthy.

---

## Next Steps

Invoke gap analysis against the research context (high-level-design.md, decision-log.md). Priorities for the gap analysis: the worldgen z bug (fix vs work around), the GPU handle ownership location, the UV/sampler check, the border-culling policy for missing chunks, and whether mesher unit tests are in scope.
