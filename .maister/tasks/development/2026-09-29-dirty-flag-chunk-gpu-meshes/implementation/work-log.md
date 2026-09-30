# Work Log

## 2026-09-29T18:37:04Z - Implementation Started

**Total Steps**: 41
**Task Groups**: 1 Worldgen/Profiler/Benchmark/Baseline; 2 meshDirty/ChunkManager API/ChunkMesher; 3 ChunkMeshStore/shader/draws; 4 Neighbor culling; 5 Delete cache/LOD/dead code
**Execution**: sequential (all groups edit App.cpp/App.h)

## Standards Reading Log

### Loaded Per Group
(Entries added as groups execute)

## Group 1 - Worldgen Fix, FrameProfiler, Benchmark Path, Baseline (Stage 1)

### Standards Applied (Group 1)
- From plan: global/coding-style.md, global/minimal-implementation.md, global/commenting.md, global/error-handling.md, global/conventions.md
- From INDEX.md: project docs not needed beyond standards; testing/test-writing.md recorded as documented deviation (no tests by user decision)

### Build config (use for ALL later groups)
- CMAKE_BUILD_TYPE=Debug (flags `-g`, no optimisation, no NDEBUG), generator Ninja, compiler C:/msys64/ucrt64/bin/g++.exe (MSYS2 UCRT64), vcpkg triplet x64-mingw-dynamic
- Present mode: chosen at runtime (MAILBOX if supported, else VSYNC); the name is printed in the profiler header
- Build command: `MSYSTEM=UCRT64 C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/artur/code/sdl_3dr && cmake --build build"`

### 1.1 Verification checklist (stage 1)
1. Header shows `worldChunks.size() == chunkMap.size()` and 4 distinct chunk keys.
2. Two benchmark runs agree on avg Frame within ~5%.
3. Stage sum (Build+Upload+Acquire+Record+Submit) roughly equals Frame (remainder = update/event work).
4. `B` toggle-off restores the saved pose and look; repeated `B` on/off shows no drift.
5. Mouse motion ignored while benchmark is active.
6. Program exit with zero recorded frames does not crash or divide by zero.
7. Baseline sanity gate: hold-pose image plausible with all four chunks; vertices drawn == 36 x solid blocks (temporary block-count log, removed afterwards).

### Group 1 execution results

**Environment blocker**: `build/SDL1.exe` cannot be launched on this machine. Windows application control policy blocks it (PowerShell `Start-Process`: "Zasady kontroli aplikacji zablokowaly ten plik"; from MSYS bash: "Permission denied"). No bypass was attempted. Consequently every check that needs a running app is NOT RUN.

#### Done (verified by build / reading / offline computation)
- 1.2 Worldgen: `App.cpp` ConstructChunkAtLine new-chunk z now `atPos.z`; `Chunk::PositionInBounds` bounds exclusive (`<`). Static reasoning: each (atPos.x, atPos.z) column now finds its chunk via `PositionInBounds`, so 4 distinct keys are expected. NOT RUN: header check.
- 1.3 `src/FrameProfiler.h/.cpp` added (CMakeLists updated). API: `BeginFrame()` (closes previous frame; Frame stage = period between consecutive BeginFrame calls; logs report and clears when the window is full), `StageScope` RAII, `AddCount`, `StartWindow(frames, modeLabel)`, `Report()` (no-op with zero frames). Deviations from plan wording: no `EndFrame` (Frame is the BeginFrame period so it has no purpose) and `StartWindow` replaces `ResetWindow` (also sets window length and mode label).
- 1.4 Old `OnRender` instrumented: Build (chunk cull/mesh/flatten), Upload (buffer create + transfer + copy pass), Acquire, Record, Submit; `Iterate` calls `BeginFrame`. Counters for OLD code: vertices drawn = `totalVertexNumber` (when drawn), chunks drawn = chunks passing the chunk frustum test, chunks remeshed = chunks whose old mesh cache was rebuilt, bytes uploaded = `vertexDataSize` when an upload happens. Header logged (mode, present mode, NDEBUG, compiler, debugLevel, `worldChunks`, `chunkMap`) after worldgen and on benchmark toggle-on.
- 1.5 `totalFPS`/`numFPS`, their update and the Quit average removed; Quit calls `frameProfiler.Report()` (no-op with zero frames, so no division by zero by construction).
- 1.6 `B` (KEY_DOWN, non-repeat) toggles benchmark: saves position, resets profiler to 900-frame window; off: reports partial window, restores position, restores look via `RotateCameraLocal(degreesCameraEulerAngle)` (the mouse-driven global, untouched while active, so no separate saved look), zeroes `cameraVelocity`, back to 600-frame window. While active: `MoveCameraBasedOnStates` skipped, `OnUpdate` applies the pose and returns after time bookkeeping, mouse motion ignored.
- 1.7 Pose: pure function of frame counter; hold 300, revolution 600, loop 900; radius 28 around (15.5, 3, 15.5), height 30; pitch = asin(d.y), yaw = atan2(d.x, -d.z) matching `Camera::UpdateDirectionVectors`. Offline computation (perl, fov 80 vertical, aspect 16:9, near 0.1, far 100): at angles 0, 90, 180, 270 degrees all 8 corners of the 32x7x32 world box are inside the frustum (pitch -44 degrees at hold pose, max depth 52.8). NOT confirmed visually.
- 1.8 Build check: `cmake --build build` (Debug, Ninja, g++ UCRT64) succeeds with no warnings or errors; app cannot be started (see blocker).

#### NOT RUN - needs interactive session (pending user)
- 1.1 checks 1-7 (all runtime checks: 4 distinct chunk keys in header, two-run 5% agreement, stage sum vs Frame, `B` restore/no drift, mouse ignored, zero-frame exit, sanity gate).
- 1.9 Baseline sanity gate: hold-pose screenshot, temporary solid-block count vs vertices drawn (no temporary log was added since nothing could run), two-loop timing, repeat run.
- 1.10 Baseline numbers and hold-pose screenshot: NONE recorded. No baseline exists yet; do not fabricate. Label to use when the user records it: "old code, buggy cache".
- Unverified hazard for the user: the old code sizes `sceneVertexBuffer` once at the first frame with vertices and freezes the per-cell frustum cache per chunk on first visibility (plus uninitialised `cachedBlockPresence`). If the free camera at startup sees fewer chunks than the benchmark hold pose, a later larger upload may exceed the buffer. Suggest starting the app, pressing `B` immediately from the start position, and comparing vertices drawn with 36 x solid blocks; if implausible, use the plan's fallback (label baseline non-comparable, take reference screenshot from stage 2).

## Group 2 - meshDirty, ChunkManager Edit API, ChunkMesher (Stage 2)

### Standards Applied (Group 2)
- global/coding-style.md (lowerCamel for Chunk/ChunkManager/mesher, moved helpers instead of copying), global/minimal-implementation.md (no debug hook added, scaffold marked "STAGE 2 SCAFFOLD, deleted in stage 3"), global/commenting.md (sparse comments), global/error-handling.md (no new SDL error paths; scaffold reuses the existing upload error handling), global/conventions.md (CMakeLists source-list edit only).

### 2.1 Verification checklist (stage 2) - for the user to run
Use the recorded build config (Debug, Ninja, present mode printed in the header). Run the app, press `B` right away for benchmark, or use the free camera where stated.
1. Hold-pose image (benchmark frames 0-299) is identical to the reference (baseline screenshot from 1.9, or the fallback reference declared there).
2. Static window: after frame 1, `chunks remeshed` sum = 0 and `bytes uploaded` sum = 0; Build timer about 0.
3. Free-camera motion does not remesh (remeshed sum stays 0).
4. `E` on an interior block: next window shows remeshed = 1 and the block is visually gone, no stale geometry.
5. `E` on a block at local coordinate 0 or 15 on an axis with a loaded neighbor: remeshed = 2.
6. `E` repeated on the already-removed cell: no crash, remeshed unchanged.
7. Header still shows `worldChunks == chunkMap == 4`.

### Done (verified by build / desk-check)
- 2.2 `Chunk`: `meshDirty = true` default; `tryInsert` sets it; `tryRemove(localPos)` added; `cache`, `ChunkCache.h` include, `isEmpty(int LOD)`, non-const `tryGet` and non-const `operator[]` removed; const `tryGet` is now the primary implementation (no `const_cast`). `didUserEditChunk`, `numNonEmptyBlocks`, const `operator[]` left for group 5 (note: the mesher now calls `numNonEmptyBlocks()` for `reserve`, so group 5's caller check will find it).
- 2.3 `ChunkManager` (inline): static `chunkCoordinateFromBlockCoordinate`/`chunkLookupFromBlockPosition` moved out of App.cpp (App call site uses the statics), `findChunk` const/non-const, `removeBlock`, `setBlock`, private `markNeighborDirty`/`markBoundaryNeighborsDirty` (face neighbors only).
- 2.4 `static_assert(sizeof(Vertex3D) == 60)` compiles, so the layout assumption holds.
- 2.5 `src/core/ChunkMesher.h/.cpp` (`buildChunkMesh(chunk, const ChunkManager &)`), added to CMakeLists.txt: 12 triangles per block in the old order/winding, all faces, corners from the cell index +/- `kBlockHalfExtent`, faces built world-space then rebased by `atPosition`. The `ChunkManager` parameter is unnamed and unused until stage 4 (kept because the spec fixes the signature).
- 2.6 E-key body replaced by one `chunkManager->removeBlock(result.blockPosition)` after the raycast; commented-out blocks untouched.
- 2.7 `OnRender`: before the swapchain acquire, every `meshDirty` chunk is remeshed (Build timer), flag cleared, `chunks remeshed` counted. Scaffold: `App::sceneChunkMeshes` (per-chunk mesher output) and `sceneVertexCount`; flatten with `Position += chunk offset` and upload through the old single buffer only when a chunk was remeshed; the old cache/LOD/`std::execution` path is gone. Draw path unchanged except it draws `sceneVertexCount`, counts `chunks drawn` as non-empty meshes (no per-chunk frustum test in the scaffold), and the zero-vertex line draw is dropped. The two unused lambdas (`getMeshDrawCallVerticies`, `isFaceInCameraFrustrum`) were deleted now because `isFaceInCameraFrustrum` referenced `Face` through the removed `ChunkCache.h` include (they are also on the group 5 list).
- 2.9 (build part): `cmake --build build` (Debug, Ninja, g++ UCRT64) succeeds, no warnings or errors.

### setBlock / removeBlock desk-check (replaces the temporary 2.8 hook, see below)
- `removeBlock`: chunk resolved via `findChunk`; missing chunk returns false without creating anything. Local position = world - `atPosition`; `tryRemove` returns false for out-of-bounds or already-empty cell, so repeated removal is a no-op (no dirty flag, no neighbor dirtying). On success the chunk is dirty and, if a local coordinate is 0 or 15, only the loaded face neighbor on that axis is dirtied (chunkSizeXYZ - 1 vs 0 are exclusive, one direction per axis).
- `setBlock`, existing chunk: `findChunk` non-null, creation skipped, block inserted (dirty via `tryInsert`), boundary neighbors dirtied.
- `setBlock`, missing chunk: `emplace_back` (may reallocate) then `chunkMap.insert({key, size - 1})` (map is only updated after the vector grew, so a throwing `emplace_back` leaves no dangling index); the only thing derived before the emplace is the `Int3` key and a null comparison, no `Chunk` reference or pointer is held across it. Six neighbors are dirtied through fresh `findChunk` lookups (unloaded ones ignored); the new chunk is dirty by default; the chunk is then re-fetched with `findChunk` after creation before `tryInsert` and boundary dirtying.
- Neighbor lookup keys are `atPosition +/- 16` on one axis; unique keys hold after the worldgen fix.

### NOT RUN - needs interactive session (pending user)
The app cannot be launched here (Windows application control blocks `build/SDL1.exe`, no bypass attempted, see Group 1 blocker). No result below was observed.
- 2.1 checks 1-7 above (hold-pose identical image, remeshed = 0 and bytes uploaded = 0 on clean frames after frame 1, camera motion does not remesh, `E` interior -> remeshed 1, `E` on border -> remeshed 2, repeated `E` no crash and unchanged remeshed, header counts).
- 2.9 run checks (same list).
- 2.8 temporary `setBlock` debug hook and 2.10 its removal: skipped on purpose (the hook could not be run, so adding and deleting it has no value); `setBlock` is covered only by the desk-check above and has no runtime caller yet. If the user wants runtime coverage, temporarily call `setBlock` then `removeBlock` on the same cell in one handler call (scaffold buffer is sized once, so never leave an added block rendered), and once at a world position in a not-yet-created neighbor chunk, logging `chunkMap.size()` (expect +1) and dirty flags.
- Scaffold hazard for the user: the scene buffer is sized on the first upload and never regrown, so stage 2 verification must use removals only.

## Group 3 - Per-Chunk GPU Store, Chunk-Offset Shader, Per-Chunk Draws (Stage 3)

### Standards Applied (Group 3)
- global/coding-style.md (PascalCase `ChunkMeshStore` methods, lowerCamel untouched in core/), global/minimal-implementation.md (stage 2 scaffold and scene buffer deleted, no temporary exact-fit/debug code added because nothing can run it), global/commenting.md (sparse comments), global/error-handling.md (store methods return bool with `SDL_LogError(...: %s, SDL_GetError())`; failed creation releases the old buffer first and leaves no dangling handle; `OnRender` submits the command buffer and returns `FAILURE`), global/conventions.md (CMakeLists source-list edit only).

### 3.1 Verification checklist (stage 3) - for the user to run
Recorded build config (Debug, Ninja, present mode from the header).
1. Hold-pose image identical to stage 2 / reference.
2. Clean frames: `bytes uploaded` sum = 0 and `chunks remeshed` sum = 0 after frame 1, Upload timer about 0 (no copy pass is begun when no chunk is dirty).
3. Full orbit: no chunk pops in or out at the frustum edges; `vertices drawn` per window equals stage 2 for the same visible set.
4. Growth test (see recipe below).
5. Repeated `B` toggling and Quit: no validation errors, no leaked buffers.
6. A chunk with `vertexCount == 0` is not drawn and not uploaded (e.g. `E` on every block of a chunk is impractical; desk-checked only).

### Done (verified by build / desk-check)
- 3.2 `vertex.glsl` live UBO is `{ mat4 viewProjection; vec4 chunkOffset; }` (`row_major`, set 1, binding 0 kept), `gl_Position = viewProjection * vec4(a_position + chunkOffset.xyz, 1.0)`. The build regenerated `src/shaders/vertex.spv` through the glslang custom command: 1752 -> 1920 bytes, timestamp 21:02:22, `git status` shows it modified, and the build-dir copy `build/shaders/vertex.spv` is byte-identical (`cmp`). `num_uniform_buffers` stays 1.
- 3.3 `src/ChunkMeshStore.h/.cpp` (added to CMakeLists.txt). `std::map<Int3, GpuMesh>`; per chunk one vertex buffer with power-of-two capacity (`CapacityBytesFor`, the single line to swap for the exact-fit test), grown only when the required bytes exceed capacity (old buffer released immediately, new one created), never shrunk. A mesh with 0 vertices sets `vertexCount = 0` and keeps its buffer. Pooled UPLOAD transfer buffer sized to the frame's total bytes rounded up to a power of two, mapped once with `cycle = true`, memcpy at running offsets, one copy pass with one `SDL_UploadToGPUBuffer` per non-empty chunk (destination `cycle = true`). `Upload` returns true without creating anything when there are no bytes; `OnRender` only calls it when at least one chunk was remeshed. `ReleaseAll()` releases chunk buffers and the transfer buffer.
- 3.4 Draw path: file-local `ChunkUniforms` (`float[16]` + `float[4]`, `static_assert(sizeof == 80)`), viewProjection via the existing `toOutFloat16Array`; `IsChunkInFrustum` helper (positive-vertex test on `sceneCamera->frustrumPlanes`, inside means plane eq >= 0 like `Camera::IsPointInFrustum`) with AABB `[atPosition - 0.5, atPosition + 16 - 0.5]`. Pipeline and both samplers bound once; per chunk with `vertexCount > 0` in the frustum: push uniform (before every draw), bind chunk buffer, draw; counters for vertices/chunks drawn recorded once per frame.
- 3.5 `OnRender`: Build remeshes dirty chunks into a `MeshUpdate` vector, Upload calls `ChunkMeshStore::Upload` only if the vector is non-empty (error path: submit command buffer, return `FAILURE`), then bytes-uploaded is counted. Store is created after worldgen in `Init`. Stage 2 scaffold (`sceneChunkMeshes`, `sceneVertexCount`, flatten step), scene buffer members and their creation/growth code are deleted.
- 3.6 `Quit`: after `SDL_WaitForGPUIdle`, `chunkMeshStore->ReleaseAll()` replaces the scene buffer release (null-guarded, since Quit can run before `Init` created it).
- 3.7 Build: `cmake --build build` (Debug, Ninja, g++ UCRT64) succeeds, no warnings or errors from new code.

### Desk-check notes
- std140: `mat4` (64 B, `row_major` matches the row-major output of `toOutFloat16Array`) followed by `vec4` at offset 64 gives 80 B, equal to `sizeof(ChunkUniforms)`. Only `chunkOffset.xyz` is used, `w = 0`.
- The copy pass ends inside `Upload` before `SDL_BeginGPURenderPass` (which comes after the swapchain acquire), so no upload happens inside a render pass. Uniform pushes inside the render pass use `SDL_PushGPUVertexUniformData` on the command buffer as before.
- A chunk whose mesh becomes empty keeps its buffer with `vertexCount = 0` and is skipped by the draw loop, so no stale geometry is drawn. A regrown buffer starts with `vertexCount = 0` until the copy is recorded.
- Vertices are chunk-local (about -0.5..15.5), the shader adds `atPosition`, so drawn positions equal the scaffold's flattened world positions.

### NOT RUN - needs interactive session (pending user)
The app cannot be launched here (Windows application control blocks `build/SDL1.exe`; no bypass attempted). No result below was observed.
- 3.1 checks 1-6 and 3.8 (hold-pose image compare, clean-frame counters, full-orbit pop-in check, vertex counts equal stage 2, repeated `B`, Quit without validation errors).
- 3.8 growth test: no temporary exact-fit or device debug change was made. Recipe for the user: in `src/ChunkMeshStore.cpp` make `CapacityBytesFor` return `requiredBytes` (exact fit), and enable the debug flag in `SDL_CreateGPUDevice` (`App.cpp`, debug_mode argument true); build, run, press `E` on several blocks (each removal shrinks the mesh, so growth is not triggered until stage 4; in stage 3 this only checks that shrinking uploads work with no overflow or validation error); revert both changes afterwards. Real growth is only exercised in stage 4 (interior removal exposing neighbor faces).
- 3.9 revert/record: nothing to revert (no temporaries were added); hold-pose screenshot NOT recorded.

## Group 4 - Neighbor-Aware Culling (Stage 4)

### Standards Applied (Group 4)
- global/coding-style.md (lowerCamel for ChunkManager/mesher, table-driven direction offsets instead of six copies), global/minimal-implementation.md (one predicate, no temporaries added), global/commenting.md (sparse comments), global/error-handling.md (no new failure paths), global/conventions.md (no new files or dependencies).

### 4.1 Verification checklist (stage 4) - for the user to run
1. Vertices drawn average (benchmark second-loop report) is strictly lower than stage 3 on the same loop (60-80% drop is a sanity hint).
2. No holes at x=15/16 and z=15/16 seams (walk along both seams).
3. `E` on a border block (local 0/15 with a loaded neighbor): remeshed = 2 and faces appear on both sides.
4. `E` on an interior block fully surrounded by solids: remeshed = 1, six faces of neighbors appear, mesh grows.
5. +Y tops and -Y bottoms of the terrain still render (view from above and from below).
6. Outer X/Z perimeter of the world has no side faces (accepted, unloaded neighbor = solid).

### Done (build / desk-check)
- 4.2 `ChunkManager::isNeighborCellSolid(chunk, x, y, z)`: in-bounds cells read `blocks`; out-of-bounds cells resolve per axis (offset -16/0/+16) through `findChunk` and wrap the local coordinate; unloaded neighbor is air only when the out-of-range axis is Y (x and z in range), otherwise solid.
- 4.3 `buildChunkMesh` now names its `ChunkManager` parameter and emits a direction's two triangles only when the neighbor cell is not solid. Table `kNeighborOffsets` order matches face order: 0-1 +Y (0,1,0), 2-3 -Y (0,-1,0), 4-5 +X (1,0,0), 6-7 -X (-1,0,0), 8-9 +Z (0,0,1), 10-11 -Z (0,0,-1). Winding, vertex order, chunk-local positions and world UVs unchanged.
- 4.4 build part: `cmake --build build` succeeds with no warnings or errors.

### Desk-check
- Border cells: local 0 uses neighbor cell -1 (wrap to 15 of the -axis chunk), local 15 uses cell 16 (wrap to 0 of the +axis chunk); `(local + 16) % 16` maps -1 -> 15, 16 -> 0. Holds on all three axes.
- Unloaded: +/-X and +/-Z solid (no face towards nothing), +/-Y air (tops/bottoms emitted). Fully enclosed block emits nothing.
- Dirtying: `removeBlock` on local 0/15 dirties the loaded face neighbor on that axis (Group 2 `markBoundaryNeighborsDirty`), so the neighbor is remeshed and gains the newly exposed face; the edited chunk itself is remeshed so interior neighbors gain faces. Remeshed = 2 expected for a border edit with one loaded neighbor.
- Growth: after an interior removal the mesh can exceed the buffer capacity. `ChunkMeshStore::Upload` first calls `EnsureChunkCapacity` for every update; when required bytes exceed capacity it releases the old buffer, creates a bigger one (sets vertexCount 0), and only then records the copy, which sets `vertexCount` to the new count. Transfer buffer is sized to the total afterwards. Growth is handled before the copy pass.
- Unchanged App.cpp / ChunkMeshStore.cpp (no edit needed).

### NOT RUN - needs interactive session (pending user)
The app cannot be launched here (Windows application control blocks `build/SDL1.exe`; no bypass). No result below was observed.
- 4.1 checks 1-6 and 4.4 run checks (vertices-drawn drop vs stage 3 second-loop report, seam walk, hold-pose screenshot vs stage 3, tops/bottoms visible from above and below).
- 4.5 growth re-run: no temporaries added. Recipe: in `src/ChunkMeshStore.cpp` make `CapacityBytesFor` return `requiredBytes` and enable the debug flag in `SDL_CreateGPUDevice` (`App.cpp`); build and run; `E` on an interior block fully surrounded by solids (expect remeshed = 1, mesh growth) and on a border block (expect remeshed = 2); expect no overflow or validation error; then revert both changes (diff/grep) and rebuild.

## Group 5 - Delete Cache and LOD, Dead Code (Stage 5)

### Standards Applied (Group 5)
- global/minimal-implementation.md and global/coding-style.md (dead code deleted, no new code), global/commenting.md (only deletions), global/conventions.md (no build logs added at repo root).

### 5.1 Verification checklist (stage 5)
Clean build without warnings; grep finds none of `ChunkMeshCache`, `LODDBLock`, `cache.`, `GetLevelOfDetailFromDistance`, `std::execution`, `chunkFaceCulling`; CMakeLists.txt does not list `ChunkCache.h`; final benchmark and hold-pose screenshot match group 4 (runtime part NOT RUN).

### Done
- 5.2 Deleted `src/core/ChunkCache.h` (`LODDBLock`, `ChunkMeshCache`) and its CMakeLists.txt entry; `App::GetLevelOfDetailFromDistance` and its LOD comment block (App.h); `ChunkManager::smallestChunkSizeLogNumber` (ChunkManager.h). Earlier groups had already removed the LOD path, `cache`, atomics and the flatten code.
- 5.3 Deleted `chunkFaceCulling`, `canCreateVertexBufferEveryFrame`, `halfChunk` (unused) and `#include <execution>` from App.cpp. The dead lambdas were already deleted in group 2. `<list>`/`<chrono>` in App.cpp were already unused at HEAD, so they are left (pre-existing, out of scope).
- 5.4 Dead member sweep: `Chunk::didUserEditChunk` deleted (only a commented-out caller at App.cpp:748, in the out-of-scope commented E-key block); const `Chunk::operator[]` deleted with its `<cassert>` include (no callers); `Chunk::numNonEmptyBlocks` KEPT (caller: `ChunkMesher.cpp` `reserve(numNonEmptyBlocks * kVerticesPerBlock)`; with culling this over-reserves, harmless, left as is). Noted, not touched: `Chunk::isEmpty()` has no callers but is pre-existing dead code outside the cleanup list.
- 5.5 Clean rebuild (`cmake --build build --clean-first`, Debug, Ninja, g++ UCRT64): 14/14 steps, exit 0, zero warnings/errors.
- 5.6 Grep of `ChunkMeshCache|LODDBLock|\bcache\.|GetLevelOfDetailFromDistance|std::execution|<execution>|chunkFaceCulling|canCreateVertexBufferEveryFrame|smallestChunkSizeLogNumber|ChunkCache|didUserEditChunk` over `src` and `CMakeLists.txt`: only remaining hit is the commented-out `chunk.didUserEditChunk = true;` at App.cpp:748 (out-of-scope commented E-key block). `git status`: no build logs/.err/.out added by this task (root logs are pre-existing and untracked-status unchanged); `build/` and the vcpkg submodule are not tracked changes. `ChunkCache.h` deletion is staged (`D `) because it was removed with `git rm`.

### 5.7 Final benchmark and record: NOT RUN - needs interactive session (pending user)
No before/after summary exists; nothing fabricated.

## Consolidated pending user verification (ALL groups)
The app cannot be launched here (Windows application control blocks `build/SDL1.exe`; no bypass attempted). Build config: Debug, Ninja, g++ UCRT64, present mode printed in the profiler header.

1. Baseline (1.9/1.10), needs the OLD code: stash the working tree or check out commit cf9b275 into a separate worktree, add nothing else; the old code has no profiler, so cherry-pick only what is needed or use the window FPS as a rough baseline. Label numbers "old code, buggy cache" (uninitialised `cachedBlockPresence`). Sanity gate: hold-pose screenshot shows four plausible chunks; vertices drawn == 36 x solid blocks; else declare the baseline non-comparable and use the stage 2 image as reference.
2. Stage 1: header shows `worldChunks == chunkMap == 4` with 4 distinct keys; two benchmark runs agree within about 5% (use the second 900-frame report); stage sum ~ Frame; `B` on/off restores pose with no drift; mouse ignored while active; quit with zero frames does not crash.
3. Stage 2: hold-pose identical to reference; after frame 1 remeshed sum = 0 and uploaded bytes = 0; camera motion does not remesh; `E` interior -> remeshed 1; `E` on local 0/15 with loaded neighbor -> remeshed 2; repeated `E` on removed cell no crash. `setBlock` has no runtime caller (desk-checked only): optional temporary setBlock+removeBlock in one handler call, logging `chunkMap.size()` (+1 for a new neighbor chunk) and dirty flags.
4. Stage 3: hold-pose identical; clean frames upload 0 bytes; full orbit shows no chunk pop in/out; `B` toggling and Quit produce no validation errors (enable the device debug flag temporarily).
5. Stage 4: vertices drawn average strictly lower than stage 3 (60-80% drop hint); no holes at x=15/16 and z=15/16 seams; border `E` -> remeshed 2; interior `E` on a fully enclosed block grows the mesh; tops and bottoms render from above and below; perimeter X/Z sides open (accepted).
6. Growth-path recipe (3.8/4.5): in `src/ChunkMeshStore.cpp` make `CapacityBytesFor` return `requiredBytes` (exact fit) and enable the debug flag in `SDL_CreateGPUDevice` (`App.cpp`); build, run, `E` on an interior enclosed block and on a border block; expect growth, no overflow/validation error; REVERT both changes, confirm by diff/grep, rebuild.
7. Stage 5 (5.7): final second-loop benchmark report and hold-pose screenshot match group 4; Quit clean; write the before/after summary (baseline vs final avg/p95 Frame, vertices drawn) here, honouring the non-comparable label if the baseline gate fails.

## Verification fixes (fix loop iteration 1)

- Mesher: `buildChunkMesh` now checks each neighbor cell first and builds `Face`s only for exposed directions (corner-index table `kFaceCorners`); output order, winding, chunk-local positions and world-position UVs are unchanged by construction (same corners, same triangle order, same `Face` -> `GetFaceDrawCallVerticies` path). Removed the `reserve(numNonEmptyBlocks * 36)` over-reserve; deleted `Chunk::numNonEmptyBlocks` and `Chunk::isEmpty` (no callers).
- `ChunkMeshStore`: byte totals and capacities computed in `size_t`; a single mesh or the per-call total above 2^31 bytes logs an error and returns false before `std::bit_ceil`. Growth behavior unchanged; the growth recipe (item 6 above) still applies to `CapacityBytesFor`.
- `FrameProfiler::BeginFrame` reads the counter for the frame boundary and re-reads it after `Report()` so log time is not attributed to the next frame.
- `App`: swapchain-parameter failure logs a warning and sets the header present mode to `unknown`; null-check of `sceneCamera` in `ToggleBenchmark`; `LogProfilerHeader` tolerates a null `chunkManager`; `kFreeModeProfilerWindowFrames` is private; unused `uniformBuffer` and the stale `(void) result;` comment removed; commented-out blocks in the `E` handler and at the top of `ConstructChunkAtLine` deleted. `ApplyBenchmarkPose` const left as is (it mutates the global camera, not `App`).
- Docs: architecture.md and roadmap.md updated to the new design.
- Build: clean incremental build, zero warnings. Runtime verification still pending (see consolidated list).

## 2026-09-29T19:22:02Z - Implementation Complete (workflow finalized)

**Steps**: 31/41 done, 4 skipped by decision (2.8, 2.10, 3.9, 4.5), 6 runtime steps pending user (1.9, 1.10, 2.9, 3.8, 4.4, 5.7).
**Verification**: passed with issues (0 critical); one fix loop applied. Runtime verification NOT RUN — see "Consolidated pending user verification" above.
