# Specification: Dirty-Flag Per-Chunk GPU Meshes (SDL_3DR)

## TL;DR
- Replace the per-frame full-scene mesh/flatten/upload (`App::OnRender`, App.cpp:1311-1758) and the broken `ChunkMeshCache` with per-chunk `meshDirty` meshes held in persistent per-chunk `SDL_GPUBuffer`s, drawn once per visible chunk with a chunk offset in the vertex uniform.
- Mesher emits chunk-local `Vertex3D` for exposed faces only (neighbor-aware; unloaded neighbors solid on X/Z, air on +/-Y). Edits go through new `ChunkManager::setBlock`/`removeBlock`, which dirty the chunk and boundary neighbors.
- Built in 5 verified stages: (1) worldgen fix + `FrameProfiler` + benchmark path + baseline, (2) dirty flag + mesher, (3) GPU store + shader/spv rebuild, (4) neighbor culling, (5) delete cache/LOD code.
- Developer-facing only: key `B` toggles a benchmark orbit, `SDL_Log` reports avg/p95/max. No tests, no build-config change.

## Key Decisions
- Dirty-flag per-chunk GPU meshes; keep `std::optional<Object>` block storage (research ADR-001..005) - the research architecture decision is binding.
- Unloaded neighbor policy: solid on X/Z, air on +/-Y - literal all-6-sides solid would delete terrain top and bottom faces.
- GPU handles live in an App-owned `ChunkMeshStore` keyed by chunk `Int3`, never inside `Chunk` (`Chunk` is copied at App.cpp:1214 and moved on `emplace_back` reallocation).
- Mutation only through `ChunkManager::setBlock`/`removeBlock`; the non-const `tryGet`/`operator[]` are deleted, which removes the accessor bypasses of dirty marking (public `blocks`/`worldChunks` remain reachable by direct access; making them private is out of scope). `Chunk::tryInsert` also marks dirty.
- All dirty chunks are remeshed every frame regardless of visibility (single-threaded, before the render pass), so camera motion alone never triggers a remesh or upload.
- Chunk-local vertices with world-position UVs: mesher builds world-space `Face`s, calls the existing `Face::GetFaceDrawCallVerticies`, then rebases `Position` by the chunk origin. `Face.cpp` is unchanged; texturing matches baseline.
- Uniform becomes `{mat4 viewProjection; vec4 chunkOffset;}` (80 B), `num_uniform_buffers` stays 1 (App.cpp:264); `vertex.spv` is rebuilt by the existing glslang custom command and committed.
- No custom deferred-release queue: SDL defers `SDL_ReleaseGPUBuffer`/`SDL_ReleaseGPUTransferBuffer` until in-flight command buffers finish. No `SDL_WaitForGPUIdle` per chunk.
- Profiler uses `SDL_GetPerformanceCounter`, unconditional `SDL_Log` (independent of `debugLevel=0`), logs present mode and build info; forces no present mode or build type.
- `totalFPS`/`numFPS` (App.h:147-148, uninitialised, div-by-zero at App.cpp:725) are removed; their update block is inside replaced code and `Quit` logs a profiler report instead.
- Lazy chunk creation lives in `setBlock` (dirties the new chunk and its existing neighbors). `removeBlock` never creates chunks.

## Open Questions / Risks
- Debug-build timings are relative-only; every baseline/after comparison must use the same build config, and the report header prints it.
- With the solid X/Z policy, faces on the outer X/Z world boundary are not emitted: the world's four outer sides look open from outside (accepted by the user).
- The baseline geometry is itself unreliable (frozen per-cell frustum cache, inverted presence condition at App.cpp:1418). The baseline reference is the benchmark start pose, which is constrained to see all four chunks fully so the frozen cache is complete; later baseline path frames may show holes, so baseline timings are labelled "old code, buggy cache".
- `setBlock` has no runtime caller in this task (the add path at App.cpp:671-698 stays commented out). It is kept because the requirements mandate the `setBlock`/`removeBlock` pair and it is where lazy chunk creation belongs. Drop it if a review flags it as dead code.
- The lazy chunk-creation branch in the E-key handler (App.cpp:648-652) is unreachable today (a raycast hit implies the chunk exists), so routing the E-key through `removeBlock` drops it without behavior change.
- Latent worldgen defects left alone (out of scope, unreachable with the current noise range of 1 y-chunk per column): stale `currentlyWorkingChunk` across `chunkI` iterations (App.cpp:1176-1197), `Object.Position` y-offset doubling for chunks below y=0, noise ignoring chunk offset (four columns with identical terrain, so border verification uses edits and image comparison, not terrain shape).
- `static_assert(sizeof(Vertex3D) == 60)` assumes `Vector` is three packed floats; if it fails to compile the planner must check `Vector`'s layout before touching the vertex layout.
- Zero-vertex line pass draw (App.cpp:1746-1747) is dropped together with the replaced draw block; the line pipeline creation and Quit release are untouched (pre-existing, listed as follow-up).

## Goal
Make static frames cost zero CPU meshing and zero GPU upload, cut vertex count with neighbor-aware face culling, and give the developer an in-app, repeatable way to measure it.

## User Stories
- As the developer, I want to press a key and get a deterministic camera orbit with a timing report in the log, so that I can compare frame cost before and after each stage.
- As the developer, I want block edits to remesh only the affected chunk (and boundary neighbor), so that editing does not slow the frame.
- As the player/developer, I want gameplay and visuals unchanged (except the intentionally open outer X/Z world sides), so that the optimization is invisible.

## Core Requirements
1. **FrameProfiler**: per-stage timers via `SDL_GetPerformanceCounter` for stages Frame (period between consecutive `BeginFrame` calls, includes present wait), Build (CPU vertex work: baseline cull+mesh+flatten, final remesh), Upload, Acquire (swapchain wait), Record (render pass recording, including chunk cull and draws), Submit. Counters per frame: vertices drawn, chunks drawn, chunks remeshed, bytes uploaded. Every 600 frames one unconditional `SDL_Log` report: avg/p95/max per stage in ms, counter averages and sums. Header logs mode (free/benchmark), present mode name, build info (NDEBUG presence, compiler, `debugLevel`), `worldChunks.size()` and `chunkMap.size()`.
2. **Benchmark camera path**: key `B` (unused; ESC/W/A/S/D/SPACE/SHIFT/F/E taken) toggles on `KEY_DOWN` non-repeat. Pose is a pure function of a benchmark frame counter (never delta time): hold at the start pose for a fixed number of frames (so a same-pose screenshot is practical), then one full revolution over 600 frames. Recommended constants: orbit radius 28 around (15.5, 3, 15.5), camera height 30, pitch/yaw looking at that centre. Hard constraint: the start pose sees all four chunks fully inside the frustum (far plane is 100). While active: skip `MoveCameraBasedOnStates`, gravity/raycast/velocity in `OnUpdate`, and ignore mouse motion. On toggle-on: save camera position, reset the profiler window, log the header. On toggle-off: restore position and `degreesCameraEulerAngle` look via `RotateCameraLocal`, zero `cameraVelocity`, log the partial window.
3. **FPS bookkeeping**: remove `totalFPS`/`numFPS` and their update (App.cpp:1577-1581) and the Quit average (App.cpp:725); `Quit` logs a final profiler report when at least one frame was recorded.
4. **Worldgen fix**: App.cpp:1192 uses `atPos.z` as chunk z; `Chunk::PositionInBounds` (Chunk.h:89-91) becomes exclusive (`<`). Result: 4 distinct chunk keys, `worldChunks.size() == chunkMap.size()`.
5. **Chunk dirty flag**: `Chunk::meshDirty` defaults true; `tryInsert` sets it on success; new `tryRemove(localPos)` resets the block, sets it and returns whether a block was removed. Remove `cache`, `ChunkCache.h` include and `isEmpty(int LOD)`. Delete the non-const `tryGet` and non-const `operator[]`; the const `tryGet` becomes the primary implementation (no `const_cast`).
6. **ChunkManager API** (lowerCamel, inline in `ChunkManager.h`): `findChunk(Int3)` (const and non-const); `chunkCoordinateFromBlockCoordinate`/`chunkLookupFromBlockPosition` moved from the App.cpp anonymous namespace (not duplicated); `removeBlock(worldBlockPosition)`; `setBlock(worldBlockPosition, Object)`; private boundary-neighbor dirtying (local coordinate 0 or N-1 dirties the adjacent chunk on that axis if loaded; face neighbors only, no diagonals). `setBlock` creating a missing chunk dirties the new chunk and all 6 existing neighbors. `Object.Position` keeps its existing chunk-local convention (used at App.cpp:1234).
7. **E-key edit** (App.cpp:639-660): replaced by a single `chunkManager->removeBlock(result.blockPosition)` call; the commented-out blocks at 662-698 in that handler are left as they are (out of cleanup scope).
8. **Mesher** (`ChunkMesher`, pure, SDL-free): `chunk + ChunkManager (const) -> std::vector<Vertex3D>`, chunk-local positions (about -0.5..15.5). Per block, 12 triangles in the existing order and winding (App.cpp:1515-1533): 0-1 +Y, 2-3 -Y, 4-5 +X, 6-7 -X, 8-9 +Z, 10-11 -Z. Corners at block +/-`kBlockHalfExtent` around the cell index (x,y,z), never `Object.Position`. Faces built in world space so UVs equal baseline, normals/tangents come from the existing `Face` code. Mesh is empty for empty chunks.
9. **Neighbor-aware culling** (stage 4): a face is emitted only when the neighbor cell is not solid. In-chunk neighbors read `blocks`; out-of-chunk neighbors resolve through `findChunk`; unloaded neighbor = solid for +/-X and +/-Z, air for +/-Y. A `ChunkManager` predicate over local coordinates in [-1, N] holds this policy in one place.
10. **ChunkMeshStore** (`src/`, SDL-facing, PascalCase methods like `App`): `std::map<Int3, GpuMesh>` with `{SDL_GPUBuffer*, capacityBytes, vertexCount}`; one persistent vertex buffer per non-empty chunk; capacity rounded up to a power of two, grown only when needed by releasing the old buffer immediately and creating a bigger one; buffers never shrink; a chunk whose mesh becomes empty keeps its buffer with `vertexCount = 0` and is not drawn. One pooled UPLOAD transfer buffer sized to the frame's total dirty bytes (grown with headroom, old one released), mapped once with `cycle = true`, all dirty chunks memcpy'd at running offsets, then one copy pass with one `SDL_UploadToGPUBuffer` per chunk (destination `cycle = true`). Nothing is created or begun when no chunk is dirty. `ReleaseAll()` is called from `App::Quit` after `SDL_WaitForGPUIdle` (replaces App.cpp:732-734).
11. **Shader**: `vertex.glsl` UBO becomes `{mat4 viewProjection; vec4 chunkOffset;}` (live code at lines 55-79, old commented shader at 1-53 untouched), `gl_Position` uses `a_position + chunkOffset.xyz`; `vertex.spv` regenerated via the existing CMake glslang custom command and committed. A file-local 80-byte uniform struct in `App.cpp` (with `static_assert` on size) is pushed before every chunk draw; `viewProjection` keeps the `row_major` qualifier and the existing `toOutFloat16Array` data.
12. **Draw path**: chunk AABB is `[atPosition - 0.5, atPosition + N - 0.5]` on each axis (blocks are centred on integers) tested against `sceneCamera->frustrumPlanes` with the existing positive-vertex loop, extracted into one anonymous-namespace helper. Pipeline and the two samplers are bound once; per visible chunk with `vertexCount > 0`: push uniform, bind that chunk's buffer, draw. Empty and off-screen chunks are skipped.
13. **Remesh policy**: in `OnRender`, before the swapchain acquire, every chunk with `meshDirty` is remeshed single-threaded, `meshDirty` cleared, results queued for one `ChunkMeshStore::Upload`. Edits run on the main thread outside `OnRender`, so `meshDirty` stays a plain `bool`.
14. **Deletions** (after stage 4 verifies): `ChunkCache.h` (also CMakeLists.txt:32), `LODDBLock`/`ChunkMeshCache`, LOD path and `GetLevelOfDetailFromDistance` (App.h:96-113 area), `ChunkManager::smallestChunkSizeLogNumber` and LOD constants (ChunkManager.h:20-35), `chunkFaceCulling` (App.cpp:41), `canCreateVertexBufferEveryFrame` (App.cpp:42), `halfChunk` (App.cpp:47) if unused, dead lambdas (App.cpp:1334-1353), `std::execution::par`/`par_unseq` loops and `<execution>` include, atomics and Face flatten, scene-buffer members (`sceneVertexBuffer`, `lastSceneVertexBufferDataSize`, `sceneVertexBufferSize`, App.h:127-131) and their creation code (App.cpp:1614-1682), unused includes made dead by this.
15. **Vertex3D**: add `static_assert(sizeof(Vertex3D) == 60)`; layout and pipeline vertex input (App.cpp:354-448) unchanged.
16. **CMakeLists.txt**: source-list edits only (add `FrameProfiler`, `ChunkMeshStore`, `ChunkMesher` files; remove `ChunkCache.h`). No presets, flags or targets change.
17. **Error handling**: keep the existing pattern: `SDL_LogError(APP_LOG_CATEGORY_GENERIC, "...: %s", SDL_GetError())`, release partial resources, submit the open command buffer, return `FAILURE`. Store methods return `bool`; `OnRender` owns the submit-and-return.
18. **Conventions**: `Chunk`/`ChunkManager`/mesher methods lowerCamel; `App`, `FrameProfiler`, `ChunkMeshStore` methods PascalCase; keep existing identifier spellings (`Frustrum`, `Verticies`); K&R, 4-space, sparse comments, no change-log comments.

## Visual Design
No UI and no mockups (`design-context/` absent). Visual reference is a baseline screenshot taken at the benchmark start pose in stage 1, after the worldgen fix and before any rendering change. Expected image relationship to baseline: identical after stages 2 and 3; after stage 4 identical except outer X/Z perimeter side faces seen from outside (open shell, user-approved) and interior faces that were never visible.

## Reusable Components

### Existing Code to Leverage
| Path | Provides | Use |
|---|---|---|
| `src/App.cpp:1515-1533` | 12-face corner layout and winding | Copy face order/winding into the mesher |
| `src/core/Face.h`, `Face.cpp:3-81` | `GetFaceDrawCallVerticies` (normal, tangent, UV from position, color) | Mesher reuses it unchanged, then rebases `Position` |
| `src/App.cpp:1364-1380` | Frustum AABB positive-vertex test | Extract into chunk visibility helper with -0.5 offset |
| `src/App.cpp:64-86` | `ChunkCoordinateFromBlockCoordinate`, `ChunkLookupFromBlockPosition` | Move to `ChunkManager` as statics |
| `src/core/ChunkManager.h:38-41` | `worldChunks`, `chunkMap` (`std::map<Int3, size_t>`) | Neighbor and edit lookups |
| `src/core/Chunk.h:34-46` | `isPosInBounds`, `chunkIndex`, `tryInsert` | Mesher in-chunk reads, worldgen insert |
| `src/App.cpp:1219-1305` | `CheckIsPointInsideAny`, `RaycastRay` (const `tryGet`) | Unchanged; E-key raycast feeds `removeBlock` |
| `src/App.cpp:721-783` | `Quit` release ordering, `SDL_WaitForGPUIdle` | Add store release in place of scene buffer release |
| `src/App.cpp:1611-1690` patterns | Early-return-submit error handling, `SDL_MapGPUTransferBuffer`/copy pass usage | Model for store upload error paths |
| `src/App.cpp:233-239` | Present mode selection | Keep; store the name for the profiler header |
| `src/core/Camera.h/.cpp` | `Position`, `RotateCameraLocal` path, `cameraVelocity`, frustum planes | Benchmark pose and restore |
| `src/core/Int3.h` | Lexicographic `operator<`, `toVector`, `Vector::toInt3` | `std::map` key for store and neighbor keys |
| `src/GlobalVariables.h` | `kBlockHalfExtent`, `APP_LOG_CATEGORY_GENERIC`, `SDL_Log` macros | Constants and logging (profiler uses `SDL_Log` directly, not `SLog1`) |
| `src/shaders`, `CMakeLists.txt:46-67` | glslang custom command, `.spv` copy to build dir | Shader rebuild path, no CMake change |
| Sampler `REPEAT` (App.cpp:1070-1072) | Safety net for UV values | World-position UVs kept anyway |

### New Components Required
| Component | Files | Why reuse is impossible |
|---|---|---|
| `FrameProfiler` | `src/FrameProfiler.h/.cpp` | No timing infrastructure exists; existing `SLog1` timing is compiled out (`debugLevel=0`) and measures nothing. SDL-facing, so it lives beside `App`, not in SDL-free `core/`. |
| `ChunkMesher` | `src/core/ChunkMesher.h/.cpp` | Meshing is inline inside the `OnRender` lambda (1363-1558) and mixes culling, LOD and caching. A pure function is the only clean seam and keeps `core/` SDL-free. |
| `ChunkMeshStore` | `src/ChunkMeshStore.h/.cpp` | Nothing owns per-chunk GPU handles; App.cpp is already 1758 lines and Quit must release them. Handles cannot live in `Chunk` (copy/move). |
| `ChunkManager` methods | `ChunkManager.h` (inline) | Neighbor lookup and dirty-marking do not exist; `chunkMap` has no accessor. |

## Technical Approach
Data flow: worldgen or edit -> `meshDirty` (chunk plus boundary neighbors) -> per frame, remesh every dirty chunk into chunk-local `Vertex3D` -> pooled transfer buffer -> one copy pass -> per-chunk persistent GPU buffer -> per-chunk draw with chunk offset uniform.

`OnRender` order after the change: acquire command buffer; update frustum and view-projection; Build (remesh dirty chunks, queue uploads); Upload (single `ChunkMeshStore::Upload` if queue non-empty); Acquire swapchain; Record (bind pipeline and samplers, loop chunks with frustum test, push uniform, bind, draw); end pass; Submit. Profiler `BeginFrame`/`EndFrame` wrap `Iterate` (App.cpp:547-561); stage timers are RAII scopes around each region.

Neighbor and dirty notes: `worldChunks.emplace_back` can reallocate, so `setBlock` must not hold `Chunk` references across creation. After the worldgen fix, `chunkMap` keys are unique and neighbor lookups by `atPosition +/- 16` are correct. A newly created chunk changes its neighbors' boundary faces (unloaded-policy becomes real data), hence dirtying all 6 loaded neighbors. Empty-mesh chunks skip upload and draw.

Worldgen at startup marks every chunk dirty (default), so frame 1 remeshes and uploads all four chunks; later frames are clean.

Stage 2 transitional scaffold (removed in stage 3): mesher output is cached per chunk in App, flattened into the old scene vertex vector with `Position += chunk.atPosition` and uploaded through the old single buffer only when any chunk was remeshed. It exists only so stage 2 is verifiable against the baseline with the unchanged shader; it must not survive stage 3.

### Integration Points (line numbers refer to the current files, before any edit)
| File:lines | Change |
|---|---|
| `App.cpp:41-42, 47` | Delete `chunkFaceCulling`, `canCreateVertexBufferEveryFrame`, `halfChunk` (stage 5) |
| `App.cpp:64-86` | Move chunk-coordinate helpers to `ChunkManager`; App call sites use the statics |
| `App.cpp:233-239` | Keep present-mode logic; retain the chosen mode for the profiler header |
| `App.cpp:264` | `num_uniform_buffers = 1` unchanged |
| `App.cpp:516-524` | After worldgen calls: create store; log chunk counts in benchmark/profiler header |
| `App.cpp:547-561` | `Iterate`: profiler `BeginFrame`/`EndFrame`; skip `MoveCameraBasedOnStates` when benchmarking |
| `App.cpp:572-660` | Add `SDLK_B` toggle (KEY_DOWN, non-repeat); replace E-key body 639-660 with `removeBlock` |
| `App.cpp:706-711` | Ignore mouse motion while benchmarking |
| `App.cpp:721-783` | Quit: replace FPS log (725) with profiler report; replace 732-734 with `ReleaseAll()` (before `chunkManager.reset()` at 773 or independent of it) |
| `App.cpp:1087-1118` | `OnUpdate`: early return after time bookkeeping (1088-1091) when benchmarking, applying the frame-counter pose |
| `App.cpp:1184, 1192` | `PositionInBounds` use remains; `atPos.y` -> `atPos.z` at 1192 |
| `App.cpp:1311-1758` | `OnRender` rewritten per Core Requirement 13/12; 1334-1353 dead lambdas deleted; 1363-1558 mesher and cache removed; 1566-1582 FPS block removed; 1583-1681 flatten/upload removed; 1721-1750 replaced by per-chunk draws |
| `App.h:54, 127-148` | Add profiler, store, benchmark state members; remove scene buffer and FPS members; `GetLevelOfDetailFromDistance` (96-113 area) deleted in stage 5 |
| `core/Chunk.h:6, 25-26, 49-70, 77-81, 89-91` | Include and cache removed; accessors trimmed; `meshDirty`, `tryRemove` added; bounds exclusive |
| `core/ChunkManager.h:20-35, 38-41` | LOD constants removed (stage 5); lookup/edit/neighbor API added |
| `core/Vertex3D.h:11-19` | `static_assert` on size |
| `shaders/vertex.glsl:63-65, 71-72` | UBO gains `chunkOffset`; position adds it |
| `shaders/vertex.spv` | Regenerated and committed |
| `CMakeLists.txt:6-33` | Add new source files, remove `src/core/ChunkCache.h` (line 32) |

## Implementation Guidance

### Staged Implementation Order and Verification
No automated tests exist and none are added. Each stage is shippable and is verified by profiler report, screenshot comparison and controlled edits. All comparisons use the same build config and present mode (both printed in the header). Discard the first report window after app start; benchmark windows are aligned to path loops because toggling `B` resets the window.

**Stage 1 - Worldgen fix, FrameProfiler, benchmark path, baseline** (Requirements 1-4)
Instrument the old code: Build = 1363-1612, Upload = 1614-1681, Acquire = 1686, Record = 1714-1750, Submit = 1752, counters from `totalVertexNumber` and `vertexDataSize`. Fix the z typo and exclusive bounds first, so the baseline scene is final.
Verify:
1. Header shows `worldChunks.size() == chunkMap.size()` and 4 distinct chunk positions.
2. Two benchmark runs agree on avg Frame within noise (about 5%).
3. Stage times (Build+Upload+Acquire+Record+Submit) sum to roughly Frame, unexplained remainder being update and event work.
4. Toggle off restores the free camera at the saved pose; `B` on/off repeatedly leaves no drift; mouse ignored while active.
5. Record baseline numbers (build label "old code, buggy cache") and the hold-pose screenshot in `implementation/work-log.md`.
6. Program exit with zero rendered frames does not divide by zero.

**Stage 2 - `meshDirty`, neighbor lookup helper, mesher (all 6 faces still emitted)** (Requirements 5-8, 13, scaffold)
Mesher emits every face (no culling yet) so geometry equals baseline. `findChunk`, `setBlock`/`removeBlock`, boundary dirtying and E-key rerouting land here.
Verify:
1. Hold-pose image identical to the baseline screenshot.
2. Static window: remeshed sum = 0 and uploaded bytes sum = 0 after the first frame; Build timer about 0.
3. Camera motion (free mode) does not remesh.
4. Press `E` on an interior block: next window shows remeshed = 1, block visually gone, no stale geometry.
5. `E` on a block at local coordinate 0 or 15 on an axis with a loaded neighbor: remeshed = 2.
6. `E` repeated on already-removed position: no crash, remeshed unchanged.

**Stage 3 - Per-chunk GPU store, shader/spv rebuild, chunk-offset draws** (Requirements 10-12, 15; delete scaffold and scene buffer)
Rebuild `vertex.spv` through the normal CMake build and confirm the file changed (timestamp/diff) and the build-dir copy is updated. Fix AABB offset.
Verify:
1. Hold-pose image identical to stage 2/baseline.
2. Clean frames: upload bytes = 0, no copy pass created; Upload timer about 0.
3. Full orbit: no chunk pops in or out visibly while any part of it is on screen (AABB offset correct); vertex counts per window match stage 2 for the same visible set.
4. Growth test: temporarily switch the capacity policy to exact-fit and enable device debug (`SDL_CreateGPUDevice` debug flag at App.cpp:217-218), remove several blocks with `E` (each removal in stage 3 shrinks, so also re-run this check in stage 4 where removing interior blocks exposes neighbor faces and grows meshes); no overflow, crash or validation error. Revert both temporary changes before commit.
5. Repeated `B` toggling and Quit: no validation errors, no leaked buffers (release ordering: after `SDL_WaitForGPUIdle`).
6. Single-vertex-count sanity: chunk with `vertexCount = 0` is not drawn and has no upload.

**Stage 4 - Neighbor-aware culling and boundary policy** (Requirement 9)
Verify:
1. Vertices-drawn average on the same benchmark path drops versus stage 3 (expected large drop, roughly 60-80% for this terrain; a strict decrease is the gate, the range is a sanity hint).
2. No holes at chunk borders: watch the orbit and hold pose along the x=15/16 and z=15/16 seams; compare screenshots with stage 3, differences only at the outer X/Z perimeter side faces from outside.
3. Edit at a border block reveals neighbor faces on both sides in one remesh (remeshed = 2) and the world stays closed elsewhere.
4. Edit an interior block fully surrounded by solids: neighbor faces become visible, mesh grows, growth path exercised (re-run the temporary exact-fit growth test).
5. Top faces of terrain and bottom faces at y=-0.5 still render (`+/-Y` air policy); nothing vanishes when viewed from above.
6. Open outer sides visible from outside at the orbit are as described (see through terrain edge), matching the accepted policy.

**Stage 5 - Delete cache and LOD** (Requirement 14)
Verify: project builds with no warnings from removed identifiers; grep finds no `ChunkMeshCache`, `LODDBLock`, `cache.`, `GetLevelOfDetailFromDistance`, `std::execution`, `chunkFaceCulling`; `CMakeLists.txt` no longer lists `ChunkCache.h`; final benchmark and hold-pose screenshot match stage 4; no stray build logs from the repo root are committed.

### Testing Approach
- Per constraint, no test framework, test target or build-config change. Instead each stage above has 2-8 focused manual checks; run only that stage's checks plus the hold-pose screenshot regression.
- The pure `ChunkMesher` is the seam if tests are added later; when that happens use 2-8 focused tests per step group (mesher, dirty marking, neighbor policy).

### Standards Compliance
- `.maister/docs/standards/global/coding-style.md`: naming consistency per class family, no dead code, no backward-compatibility shims, DRY (chunk-coordinate helpers moved, not copied).
- `.maister/docs/standards/global/minimal-implementation.md`: every method has an immediate caller (exception: `setBlock`, flagged in risks), delete exploration artifacts and the stage 2 scaffold, no future stubs, no speculative abstractions.
- `.maister/docs/standards/global/commenting.md`: names carry meaning, sparse comments, no change-log comments.
- `.maister/docs/standards/global/error-handling.md`: clear `SDL_LogError` messages, fail fast, release resources on the error path, submit the command buffer before returning.
- `.maister/docs/standards/global/conventions.md`: predictable structure, no new dependencies, keep repo root free of build logs.
- `.maister/docs/standards/testing/test-writing.md`: recorded as a documented deviation (no tests by user decision); manual stage verification stands in for risk-based coverage of the mesher and edit paths.
- Update `.maister/docs/project/architecture.md` and `roadmap.md` in the documentation phase to describe the dirty-flag mesh flow and remove the LOD/cache mentions.

## Out of Scope
- Build presets, LTO, external profilers (Tracy, RenderDoc, Nsight), on-screen stats.
- Greedy meshing, indexed/compact vertices, indirect or multi-draw, suballocated single buffer, texture atlas or mipmaps, compact block ids/palette.
- Real LOD, occlusion culling, chunk streaming/eviction, threaded generation or threaded remesh.
- Noise offset fix (four chunk columns keep identical terrain), the stale `currentlyWorkingChunk` and `Object.Position` latent worldgen issues, the empty line pass and line pipeline, the unused `uniformBuffer` member, double `SDL_DestroySurface`, unrelated bugs.
- Cleanup beyond code this task replaces plus the dead lambdas at App.cpp:1334-1353 (commented worldgen block 1123-1156, unused `GetFaceLineCallVerticies` and commented E-key blocks stay).
- Forcing a present mode or viewport resize handling (viewport stays fixed 1920x1080).

## Success Criteria
- Clean frames (no edit) record zero chunks remeshed and zero bytes uploaded in the profiler window; camera motion alone triggers none.
- A block edit remeshes only the edited chunk, plus the adjacent chunk when the block is on a chunk boundary; no stale or missing geometry.
- Hold-pose image is identical to the baseline screenshot after stages 2 and 3, and differs after stage 4 only on the accepted open outer X/Z sides.
- Vertices-drawn average on the benchmark path drops after stage 4 versus stage 3, with no visible holes at chunk borders.
- Average and p95 frame time on the benchmark path improve versus the stage 1 baseline under the same build config and present mode.
- No buffer overflow or validation error when a chunk mesh grows; Quit releases all chunk buffers without errors.
- `worldChunks.size() == chunkMap.size()` with 4 distinct chunks; no `ChunkMeshCache`/LOD/`std::execution` code remains; `vertex.spv` matches `vertex.glsl`.
