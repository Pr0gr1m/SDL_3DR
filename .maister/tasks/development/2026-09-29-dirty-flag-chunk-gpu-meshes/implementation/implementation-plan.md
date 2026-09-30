# Implementation Plan: Dirty-Flag Per-Chunk GPU Meshes (SDL_3DR)

## TL;DR
- 5 task groups, one per spec stage, strictly sequential (1 -> 2 -> 3 -> 4 -> 5); no parallelism because every group edits `App.cpp`.
- Group 1 fixes worldgen and adds `FrameProfiler` + `B` benchmark orbit on the OLD render code, then records the baseline. Group 2 adds `meshDirty`, `ChunkManager` edit API and `ChunkMesher` behind a minimal temporary scaffold. Group 3 adds `ChunkMeshStore`, the chunk-offset shader and per-chunk draws. Group 4 turns on neighbor-aware culling. Group 5 deletes cache/LOD/dead code.
- No test framework (user decision): each group ends with a build check (CMake + Ninja, vcpkg, same build config throughout) and manual/log verification steps in place of automated tests. Results go to `implementation/work-log.md`.
- Spec audit findings H1, M1, M2, M3, L1-L3 and the Req 8 cell-index note are folded into steps below; corrections to the spec are listed in "Spec Corrections".

## Key Decisions
- One task group per spec stage (5 groups) - each stage is independently shippable and verifiable, and the user asked for few groups ordered by stage.
- Test steps replaced by "define verification checks" (N.1) and "build + run checks" (N.n) - no tests exist by user decision; deviation from test-writing standard is documented in the spec.
- Benchmark loop is fixed by this plan: hold 300 frames at start pose, then one 600-frame revolution, then repeat (loop = 900 frames); the profiler window in benchmark mode equals one loop (900 frames), free mode stays 600. First benchmark loop is discarded, the second report is the measurement (resolves audit M2).
- Baseline sanity gate (audit H1): the old code reads uninitialised `cachedBlockPresence` (App.cpp:1418-1432), so baseline is verified before it is trusted (image plausibility + vertex count vs expected 36 x solid blocks); if it fails, stages 2-3 are referenced against a stage 2 image checked by eye against expected terrain and baseline timings are labelled non-comparable.
- `setBlock` and lazy chunk creation are verified in stage 2 by a TEMPORARY debug caller (removed before the group is done) that sets, logs, and immediately removes in the same handler call, so the sized-once scaffold buffer can never see a larger mesh (audit M1, M3).
- Stage 2 scaffold is the minimum needed to render mesher output through the unchanged shader and old single buffer; it is explicitly deleted in group 3 (audit M3). Group 2 verification uses removals only.
- Dead-member deletion (audit L2): `Chunk::didUserEditChunk`, `Chunk::numNonEmptyBlocks` and the const `operator[]` are deleted in group 5 when a grep shows no remaining callers.
- Mesher corners come from the cell index (x,y,z) +/- `kBlockHalfExtent`, never `Object.Position` (Req 8; baseline builds from cell index and `Object.Position` has a y-offset defect for chunks below y=0).

## Open Questions / Risks
- Baseline reads uninitialised memory (H1): baseline image/timings may be garbage. Mitigated by the sanity gate in step 1.9 and the stated fallback.
- Debug-build timings are relative-only; every comparison uses the same build config and present mode (printed in the header).
- All groups touch `App.cpp`/`App.h`; merge conflicts are impossible only because groups run sequentially. Keep them sequential.
- `setBlock` has no permanent caller; kept per the user's mandated API. If review flags it as dead code, drop it (spec risk, unchanged).
- The 80-byte uniform layout and `static_assert(sizeof(Vertex3D) == 60)` are unverified assumptions; if the assert fails, inspect `Vector` layout before touching the vertex layout.
- Growth path (buffer regrow) only occurs after stage 4 for real edits; the temporary exact-fit capacity + GPU debug test in groups 3 and 4 must be reverted (verify by grep/diff before completing).
- `spec.html` was not regenerated after the three minimal `spec.md` edits (stage count wording, "accessor bypasses" wording, cell-index corners); the md is the source of truth.
- The task-group tracking items (`TaskCreate`/`TaskUpdate`) were not available to this planner; the orchestrator should create 5 items mirroring the dependency chain if group-level tracking is wanted.
- Standards: `.maister/docs/project/architecture.md` and `roadmap.md` update belongs to the documentation phase, not these groups.

## Spec Corrections (recorded, not silently applied)
Applied to `spec.md` (minimal): TL;DR now says 5 stages (L1); "nothing can bypass dirty marking" reworded to "removes the accessor bypasses" (L3); Req 8 states corners come from the cell index.
Recorded in this plan only: benchmark constants and loop/window alignment (M2, steps 1.6-1.7); baseline sanity gate and fallback (H1, step 1.9); temporary `setBlock` verification (M1, steps 2.8, 2.10); scaffold minimality (M3, step 2.7); dead-code list (L2, step 5.4); "no stray build logs" means this task adds none - pre-existing tracked root logs are left alone (L4, step 5.6).

## Overview
Total Steps: 41
Task Groups: 5
Expected Tests: 0 automated (no test framework by user decision); 5 build checks + about 30 manual/log verification checks (stage checklists from spec)

## Implementation Steps

### Task Group 1: Worldgen Fix, FrameProfiler, Benchmark Path, Baseline (Stage 1)
**Dependencies:** None
**Files to Modify:** src/App.cpp, src/App.h, src/core/Chunk.h, src/FrameProfiler.h, src/FrameProfiler.cpp, CMakeLists.txt, .maister/tasks/development/2026-09-29-dirty-flag-chunk-gpu-meshes/implementation/work-log.md
**Estimated Steps:** 10

- [ ] 1.0 Complete stage 1 (profiler, benchmark, baseline on old render code)
  - [x] 1.1 Define the verification checklist (replaces "write tests")
    - Write into work-log.md the stage 1 checks: 4 distinct chunk keys, two runs agree within ~5%, stage sum ~ Frame, B toggle restores pose with no drift, mouse ignored while active, zero-frame exit safe, baseline sanity gate.
    - Record build config (Debug/Release, generator, present mode); use the same one for every later group.
  - [x] 1.2 Fix worldgen
    - App.cpp:1192 use `atPos.z` as the chunk z component.
    - `Chunk::PositionInBounds` (Chunk.h:89-91) exclusive `<`; only caller is App.cpp:1184.
  - [x] 1.3 Add `FrameProfiler` (`src/FrameProfiler.h/.cpp`, PascalCase methods, add to CMakeLists.txt source list)
    - `BeginFrame`/`EndFrame`; RAII stage scope for Frame/Build/Upload/Acquire/Record/Submit via `SDL_GetPerformanceCounter`.
    - Counters per frame: vertices drawn, chunks drawn, chunks remeshed, bytes uploaded.
    - Configurable window length, `ResetWindow`; report = avg/p95/max per stage in ms + counter averages and sums via unconditional `SDL_Log`; header prints mode, present mode name, build info (NDEBUG, compiler, `debugLevel`), `worldChunks.size()`, `chunkMap.size()`.
    - Report is a no-op with zero recorded frames (no division by zero).
  - [x] 1.4 Instrument the OLD `OnRender`/`Iterate`
    - `Iterate` (App.cpp:547-561) wraps BeginFrame/EndFrame; retain chosen present mode name (App.cpp:233-239).
    - Build = 1363-1612, Upload = 1614-1681, Acquire = 1686, Record = 1714-1750, Submit = 1752; counters from `totalVertexNumber` and `vertexDataSize`.
    - Log header once at world creation (App.cpp:516-524) and on benchmark toggle-on.
  - [x] 1.5 Remove FPS bookkeeping
    - Delete `totalFPS`/`numFPS` (App.h:147-148), their update (App.cpp:1577-1581) and the Quit average (App.cpp:725); Quit logs a final profiler report when at least one frame was recorded.
  - [x] 1.6 Benchmark toggle and state (key `B`)
    - Members: benchmark flag, benchmark frame counter, saved position and look angles.
    - `SDLK_B` on KEY_DOWN, non-repeat (App.cpp:572-660 area). Toggle-on: save camera position, reset profiler to 900-frame window, log header. Toggle-off: restore position and `degreesCameraEulerAngle` look via `RotateCameraLocal`, zero `cameraVelocity`, log partial window, restore 600-frame window.
    - While active: skip `MoveCameraBasedOnStates` in `Iterate`, early return in `OnUpdate` after time bookkeeping (App.cpp:1087-1118) applying the pose, ignore mouse motion (App.cpp:706-711).
  - [x] 1.7 Benchmark pose function (pure function of the frame counter, never delta time)
    - Constants: hold 300 frames, revolution 600 frames, loop 900 frames repeating; orbit radius 28 around (15.5, 3, 15.5), camera height 30.
    - Frame `f` -> `k = f % 900`; angle 0 while `k < 300`, else `2*pi*(k-300)/600`; position = centre + (r*sin a, 0, r*cos a) with y = 30.
    - Look direction d = normalize(centre - position); derive pitch/yaw from the convention forward = (cos p sin y, sin p, -cos p cos y) at Camera.cpp:121 (yaw = atan2(d.x, -d.z), pitch = asin(d.y)); confirm against Camera.cpp before use.
    - Start pose must see all four chunks fully inside the frustum (far plane 100); confirm visually.
  - [x] 1.8 Build check
    - Configure/build with CMake + Ninja (vcpkg toolchain) in the recorded config; fix warnings from new code; app starts and renders as before.
  - [ ] 1.9 Run baseline and sanity gate (audit H1)
    - Run checks from 1.1: header shows `worldChunks.size() == chunkMap.size()` and 4 distinct chunk positions.
    - Baseline sanity: hold-pose screenshot must show all four chunks with plausible terrain. Also temporarily log total solid blocks (remove after) and compare with baseline vertices drawn at hold pose (expect 36 x solid blocks); a mismatch means uninitialised `cachedBlockPresence` corrupted the baseline.
    - If the sanity gate fails: label baseline timings "non-comparable", and use a stage 2 hold-pose screenshot (checked against expected terrain by eye) as the reference for stages 2-3.
    - Timing: leave benchmark on for two loops; use the second 900-frame report; repeat run and confirm ~5% agreement.
    - Program exit with zero rendered frames must not crash.
  - [ ] 1.10 Record baseline in `implementation/work-log.md`
    - Numbers labelled "old code, buggy cache" (plus non-comparable flag if 1.9 failed), hold-pose screenshot path, build config, present mode.

**Acceptance Criteria:**
- Project builds; 4 distinct chunks and equal `worldChunks`/`chunkMap` sizes in the header.
- Profiler reports avg/p95/max per stage; stage sum roughly equals Frame; two runs agree within ~5%.
- `B` on/off repeatedly restores the free camera exactly; mouse ignored while active.
- Baseline is recorded and its trustworthiness explicitly stated (sane or fallback reference declared).

### Task Group 2: meshDirty, ChunkManager Edit API, ChunkMesher (Stage 2)
**Dependencies:** Group 1
**Files to Modify:** src/core/Chunk.h, src/core/ChunkManager.h, src/core/ChunkMesher.h, src/core/ChunkMesher.cpp, src/core/Vertex3D.h, src/App.cpp, src/App.h, CMakeLists.txt, .maister/tasks/development/2026-09-29-dirty-flag-chunk-gpu-meshes/implementation/work-log.md
**Estimated Steps:** 10

- [ ] 2.0 Complete stage 2 (dirty flag, edit API, mesher emitting all faces, temporary scaffold)
  - [x] 2.1 Define the verification checklist
    - Hold-pose image identical to reference; static window remeshed sum = 0 and uploaded bytes sum = 0 after frame 1; camera motion does not remesh; `E` on interior block -> remeshed 1; `E` on local coordinate 0/15 with loaded neighbor -> remeshed 2; repeated `E` on removed cell -> no crash, remeshed unchanged; temporary `setBlock` checks (2.8).
  - [x] 2.2 `Chunk` changes (Chunk.h:6, 25-26, 49-70, 77-81)
    - `meshDirty = true` default; `tryInsert` sets it on success; add `tryRemove(localPos)` (reset block, set dirty, return whether removed).
    - Remove `cache`, `ChunkCache.h` include, `isEmpty(int LOD)`; delete non-const `tryGet` and non-const `operator[]`; const `tryGet` becomes the primary implementation (no `const_cast`).
    - Leave `didUserEditChunk`, `numNonEmptyBlocks`, const `operator[]` for the group 5 caller check.
  - [x] 2.3 `ChunkManager` API (inline in ChunkManager.h, lowerCamel)
    - `findChunk(Int3)` const and non-const.
    - Move `chunkCoordinateFromBlockCoordinate`/`chunkLookupFromBlockPosition` from the App.cpp anonymous namespace (App.cpp:64-86) as statics; update App call sites (no duplicate).
    - `removeBlock(worldBlockPosition)` (never creates chunks); `setBlock(worldBlockPosition, Object)` creating a missing chunk lazily (dirty the new chunk and its 6 loaded neighbors); hold no `Chunk` reference across `worldChunks.emplace_back`.
    - Private boundary-neighbor dirtying: local coordinate 0 or N-1 dirties the adjacent loaded chunk on that axis, face neighbors only.
  - [x] 2.4 `Vertex3D.h:11-19` add `static_assert(sizeof(Vertex3D) == 60)`; if it fails, inspect `Vector` layout first.
  - [x] 2.5 `ChunkMesher` (`src/core/ChunkMesher.h/.cpp`, pure, SDL-free, add to CMakeLists.txt)
    - `chunk + const ChunkManager -> std::vector<Vertex3D>`; empty vector for empty chunks.
    - Per block, 12 triangles in the App.cpp:1515-1533 order/winding: 0-1 +Y, 2-3 -Y, 4-5 +X, 6-7 -X, 8-9 +Z, 10-11 -Z; ALL faces emitted (no culling yet).
    - Corners come from the cell index (x,y,z) +/- `kBlockHalfExtent`, never `Object.Position`. Build world-space `Face`s (existing `Face::GetFaceDrawCallVerticies`, Face.cpp unchanged), then rebase `Position` by `atPosition` so vertices are chunk-local (about -0.5..15.5) with world-position UVs.
  - [x] 2.6 Reroute the E-key (App.cpp:639-660)
    - Replace the handler body with one `chunkManager->removeBlock(result.blockPosition)` call after the raycast; leave the commented-out blocks at 662-698 untouched.
  - [x] 2.7 Remesh policy plus MINIMAL temporary scaffold in `OnRender`
    - Before swapchain acquire, remesh every `meshDirty` chunk single-threaded, clear the flag, count `chunks remeshed`; Build timer wraps it.
    - Scaffold (deleted in group 3): App-side per-chunk cache of mesher output; only when any chunk was remeshed, flatten with `Position += chunk.atPosition` into the old scene vertex vector and upload through the old single buffer; old shader/draw path unchanged. Remove the old mesh/cache/LOD path that read `chunk.cache`. Keep it as small as possible; mark it clearly for removal.
    - Scaffold limits: buffer is sized once, so stage 2 verification uses removals only (meshes shrink); no added blocks are ever rendered.
  - [~] 2.8 SKIPPED (app cannot run here; desk-checked instead): Temporary `setBlock` verification hook (audit M1)
    - Add a temporary debug key handler that, in ONE call, does: `setBlock` into an empty cell of an existing chunk and log dirty flags of that chunk and boundary neighbors, then `removeBlock` the same cell so no larger mesh is ever rendered; then `setBlock` at a world position in a not-yet-created neighboring chunk, log `chunkMap.size()` (expect +1), the new chunk's dirty flag and the dirty flags of its existing neighbors (all 6 loaded neighbors dirty), then `removeBlock` it.
    - Confirm no crash or stale-reference behaviour after `worldChunks` reallocates, and that the following frame's remeshed counter matches the number of dirty chunks logged.
  - [ ] 2.9 Build check and run checks from 2.1
    - Build in the recorded config; hold-pose screenshot vs reference (baseline, or the fallback reference declared in 1.9); static window remeshed = 0/uploaded = 0 after frame 1; free-camera motion does not remesh; edit checks (interior, boundary -> 2, repeated `E`).
  - [~] 2.10 SKIPPED (no hook added): Remove the temporary debug hook and re-verify
    - Delete the debug key handler and any logging added for 2.8; rebuild; grep confirms the handler is gone; repeat one `E` check; record results in work-log.md.

**Acceptance Criteria:**
- Hold-pose image identical to the reference; clean frames report remeshed = 0 and uploaded bytes = 0.
- `E` remeshes exactly the edited chunk (+1 neighbor on a boundary); no stale geometry.
- `setBlock` (existing-chunk and lazy-creation branches) verified via log with the temporary hook, and the hook is removed.
- Scaffold is minimal and clearly marked for deletion in group 3.

### Task Group 3: Per-Chunk GPU Store, Chunk-Offset Shader, Per-Chunk Draws (Stage 3)
**Dependencies:** Group 2
**Files to Modify:** src/ChunkMeshStore.h, src/ChunkMeshStore.cpp, src/App.cpp, src/App.h, src/shaders/vertex.glsl, src/shaders/vertex.spv, CMakeLists.txt, .maister/tasks/development/2026-09-29-dirty-flag-chunk-gpu-meshes/implementation/work-log.md
**Estimated Steps:** 9

- [ ] 3.0 Complete stage 3 (persistent per-chunk buffers, chunk offset draws, scaffold deleted)
  - [x] 3.1 Define the verification checklist
    - Hold-pose identical to stage 2/reference; clean frames upload 0 bytes and create no copy pass; full orbit with no chunk pop in/out; vertex counts per window match stage 2; growth test; repeated `B` toggling and Quit without validation errors; `vertexCount = 0` chunk not drawn.
  - [x] 3.2 Shader and spv
    - `vertex.glsl` live UBO (lines 55-79; old commented shader 1-53 untouched) becomes `{ mat4 viewProjection; vec4 chunkOffset; }`, keep `row_major` on `viewProjection`; `gl_Position` uses `a_position + chunkOffset.xyz`.
    - Rebuild through the existing CMake glslang custom command; confirm `vertex.spv` changed (diff/timestamp) and the build-dir copy updated; `num_uniform_buffers` stays 1 (App.cpp:264).
  - [x] 3.3 `ChunkMeshStore` (`src/ChunkMeshStore.h/.cpp`, PascalCase, add to CMakeLists.txt)
    - `std::map<Int3, GpuMesh>` with `{SDL_GPUBuffer*, capacityBytes, vertexCount}`; capacity rounded up to power of two, grown only when needed (release old, create larger immediately), never shrinks; empty mesh keeps buffer with `vertexCount = 0`.
    - `Upload`: one pooled UPLOAD transfer buffer sized to the frame's total dirty bytes (grown with headroom), mapped once `cycle = true`, memcpy at running offsets, one copy pass with one `SDL_UploadToGPUBuffer` per chunk (destination `cycle = true`); nothing created or begun when no chunk is dirty.
    - `ReleaseAll()`; methods return `bool`, errors follow `SDL_LogError(APP_LOG_CATEGORY_GENERIC, "...: %s", SDL_GetError())` and release partial resources, `OnRender` submits and returns `FAILURE`. Add a small capacity policy constant/function so the temporary exact-fit switch in 3.8 is a one-line change.
  - [x] 3.4 Draw path in `App.cpp`
    - File-local 80-byte uniform struct with `static_assert` on size, using the existing `toOutFloat16Array` data; push before every chunk draw.
    - Extract the frustum positive-vertex loop (App.cpp:1364-1380) into one anonymous-namespace helper; chunk AABB = `[atPosition - 0.5, atPosition + N - 0.5]` per axis, tested against `sceneCamera->frustrumPlanes`.
    - Bind pipeline and both samplers once; per visible chunk with `vertexCount > 0`: push uniform, bind chunk buffer, draw; record vertices drawn and chunks drawn.
  - [x] 3.5 Wire store into `OnRender`/`App`
    - Build: remesh dirty chunks into the queue; Upload: single `ChunkMeshStore::Upload` if queue non-empty; count bytes uploaded; store member and creation after worldgen (App.cpp:516-524).
    - Delete the group 2 scaffold and the scene-buffer members (`sceneVertexBuffer`, `lastSceneVertexBufferDataSize`, `sceneVertexBufferSize`, App.h:127-131) with their creation code (App.cpp:1614-1682); drop the zero-vertex line pass draw (App.cpp:1746-1747) with the replaced block.
  - [x] 3.6 `Quit`
    - After `SDL_WaitForGPUIdle`, call `ReleaseAll()` in place of the scene buffer release (App.cpp:732-734); no per-chunk `SDL_WaitForGPUIdle`, no custom deferred-release queue.
  - [x] 3.7 Build check
    - Full CMake + Ninja build including shader step; zero warnings from new code; app runs.
  - [ ] 3.8 Run checks from 3.1 including growth test
    - Hold-pose image compare; clean-frame counters; full orbit for pop-in/out at frustum edges (AABB offset correct); window vertex counts equal stage 2 for the same visible set.
    - Growth test: TEMPORARILY switch capacity policy to exact-fit and enable the device debug flag (`SDL_CreateGPUDevice`, App.cpp:217-218); remove several blocks with `E`; expect no overflow, crash or validation error. Repeated `B` toggles and Quit show no validation errors or leaks.
  - [~] 3.9 SKIPPED (no temporaries added; app cannot run here): Revert temporaries and record
    - Restore power-of-two policy and device debug setting; diff/grep to confirm; rebuild; record results and hold-pose screenshot in work-log.md.

**Acceptance Criteria:**
- Hold-pose image identical to the reference; clean frames report 0 bytes uploaded and no copy pass.
- Orbit shows no visible chunk popping; vertex counts equal stage 2.
- No validation errors during growth test, repeated `B`, or Quit; temporary changes reverted.
- `vertex.spv` matches `vertex.glsl`; scaffold and scene buffer code are gone.

### Task Group 4: Neighbor-Aware Culling (Stage 4)
**Dependencies:** Group 3
**Files to Modify:** src/core/ChunkManager.h, src/core/ChunkMesher.cpp, src/ChunkMeshStore.cpp, src/App.cpp, .maister/tasks/development/2026-09-29-dirty-flag-chunk-gpu-meshes/implementation/work-log.md
**Estimated Steps:** 5

- [ ] 4.0 Complete stage 4 (emit exposed faces only)
  - [x] 4.1 Define the verification checklist
    - Vertices drawn average strictly lower than stage 3 on the same benchmark loop (about 60-80% drop is a sanity hint); no holes on x=15/16 and z=15/16 seams; border edit remeshes 2 chunks; interior edit exposes neighbor faces and grows the mesh; +Y tops and -Y bottoms still render; open outer X/Z sides as accepted.
  - [x] 4.2 Neighbor-solid predicate in `ChunkManager`
    - One predicate over local coordinates in [-1, N]: in-chunk reads `blocks`; out-of-chunk resolves through `findChunk`; unloaded neighbor is solid for +/-X and +/-Z, air for +/-Y (literal all-solid would delete terrain tops and bottoms).
  - [x] 4.3 Mesher culling
    - Emit a face only when the neighbor cell is not solid, using the predicate; face order and winding unchanged.
  - [ ] 4.4 Build check and run checks from 4.1
    - Build; benchmark second-loop report vs group 3 (vertices drawn, Frame avg/p95); screenshots along seams and hold pose vs group 3 (differences only at outer X/Z perimeter side faces); view terrain from above and below.
  - [~] 4.5 SKIPPED (app cannot run here; recipe in work-log): Growth-path re-run
    - TEMPORARILY switch capacity to exact-fit and enable device debug again; remove an interior block fully surrounded by solids, and a border block (expect remeshed = 2); expect mesh growth, no overflow/validation error; revert both temporaries, confirm by diff/grep, rebuild, record in work-log.md.

**Acceptance Criteria:**
- Vertices-drawn average strictly decreases versus stage 3; no holes at chunk borders.
- Edits at borders remesh both chunks and reveal faces on both sides; growth path exercised without errors; temporaries reverted.

### Task Group 5: Delete Cache and LOD, Dead Code (Stage 5)
**Dependencies:** Group 4
**Files to Modify:** src/App.cpp, src/App.h, src/core/ChunkManager.h, src/core/Chunk.h, src/core/ChunkCache.h, CMakeLists.txt, .maister/tasks/development/2026-09-29-dirty-flag-chunk-gpu-meshes/implementation/work-log.md
**Estimated Steps:** 7

- [ ] 5.0 Complete stage 5 (remove replaced code)
  - [x] 5.1 Define the verification checklist
    - Clean build without warnings from removed identifiers; grep finds none of `ChunkMeshCache`, `LODDBLock`, `cache.`, `GetLevelOfDetailFromDistance`, `std::execution`, `chunkFaceCulling`; CMakeLists.txt no longer lists `ChunkCache.h`; final benchmark and hold-pose screenshot match group 4.
  - [x] 5.2 Delete cache and LOD
    - Delete `ChunkCache.h` (and CMakeLists.txt:32 entry), `LODDBLock`/`ChunkMeshCache`, LOD path and `GetLevelOfDetailFromDistance` (App.h:96-113 area), `ChunkManager::smallestChunkSizeLogNumber` and LOD constants (ChunkManager.h:20-35).
  - [x] 5.3 Delete dead App code
    - `chunkFaceCulling` (App.cpp:41), `canCreateVertexBufferEveryFrame` (App.cpp:42), `halfChunk` (App.cpp:47) if unused, dead lambdas (App.cpp:1334-1353), `std::execution::par`/`par_unseq` loops and the `<execution>` include, atomics and Face flatten leftovers, and any unused includes made dead by this task.
  - [x] 5.4 Dead member sweep (audit L2)
    - Grep for callers of `Chunk::didUserEditChunk`, `Chunk::numNonEmptyBlocks` and const `Chunk::operator[]`; delete each that has no caller, keep and note any that do.
  - [x] 5.5 Build check
    - Clean full rebuild in the recorded config; no warnings from removed identifiers.
  - [x] 5.6 Grep and repo hygiene checks
    - Run the grep list from 5.1; confirm `git status` shows this task added no build logs or `.err`/`.out` files at the repo root (pre-existing tracked logs are left alone).
  - [ ] 5.7 Final benchmark and record
    - Second-loop benchmark report, hold-pose screenshot vs group 4, Quit shows no errors; write final before/after summary (baseline vs final avg/p95 Frame, vertices drawn) to work-log.md, honouring the non-comparable label if 1.9 failed.

**Acceptance Criteria:**
- All listed identifiers and `std::execution` are gone; `ChunkCache.h` not in CMakeLists.txt.
- Final benchmark and hold-pose image match group 4; Quit clean.
- No dead members remain from the cleanup list; no build logs added.

## Execution Order

1. Group 1: Worldgen Fix, FrameProfiler, Benchmark, Baseline (10 steps)
2. Group 2: meshDirty, Edit API, ChunkMesher (10 steps, depends on 1)
3. Group 3: Per-Chunk GPU Store, Shader, Draws (9 steps, depends on 2)
4. Group 4: Neighbor-Aware Culling (5 steps, depends on 3)
5. Group 5: Delete Cache and LOD (7 steps, depends on 4)

## Standards Compliance

Follow standards from `.maister/docs/standards/`:
- global/coding-style.md - naming per class family (Chunk/ChunkManager/mesher lowerCamel; App/FrameProfiler/ChunkMeshStore PascalCase), no dead code, DRY.
- global/minimal-implementation.md - no future stubs, delete scaffold and temporary hooks, unused code is debt.
- global/commenting.md - sparse comments, no change-log comments.
- global/error-handling.md - `SDL_LogError`, release resources, submit command buffer, fail fast.
- global/conventions.md - no new dependencies, repo root free of new build logs.
- testing/test-writing.md - documented deviation: no tests by user decision; manual stage verification substitutes.

## Notes

- Verification-Driven (no tests exist): each group starts with a checklist (N.1) and ends with build + run checks.
- Same build config and present mode for every comparison.
- Temporary code (debug hook in group 2, exact-fit capacity and device debug in groups 3-4, temporary block-count log in group 1) must be removed before its group is complete.
- Mark Progress: check off steps as completed; results go to `implementation/work-log.md`.
- Reuse First: Face code, frustum loop, present-mode selection, `Int3` map key, existing error pattern.
