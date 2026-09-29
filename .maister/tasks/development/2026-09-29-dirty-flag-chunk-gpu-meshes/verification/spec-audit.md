# Spec Audit: Dirty-Flag Per-Chunk GPU Meshes

## TL;DR
- Verdict: PASS WITH CONCERNS (Mostly Compliant). The spec is implementable and consistent with all binding user decisions.
- Issues: Critical 0, High 1, Medium 3, Low 5.
- The High item: the stage 1 baseline (image and timings) rests on an uninitialised-memory read in the old code that the spec describes only as a "frozen cache".
- Every App.cpp, Chunk.h, CMake and shader line reference I checked matches the source.

## Key Decisions
- Verdict is pass-with-concerns, not fail: no finding blocks implementation; the High item is a verification-gate risk with a cheap mitigation.
- Baseline uninitialised read is rated High because success criteria 3 and 5 (hold-pose identity, frame-time improvement) both depend on the baseline being meaningful.
- The `setBlock`-without-caller issue is rated Medium and not a standards violation: the user mandated the `setBlock`/`removeBlock` pair, and the spec flags it openly.
- SDL semantics (deferred release, push-uniform-between-draws, cycle flags) are judged correct from SDL3 documented behaviour. No SDL headers were found locally, so this was not re-read from source.

## Open Questions / Risks
- Q1: What does the benchmark path do after the 600-frame revolution (loop with or without hold, or freeze)? How many hold frames?
- Q2: If the stage 1 baseline screenshot shows missing or garbage terrain, what becomes the reference for stages 2-3?
- Q3: Should `setBlock` get a verification hook (temporary caller), or should lazy creation be dropped from this task?
- Risk: unverified SDL behaviour claims (see finding L5).

---

## Verification performed
Read spec.md, requirements.md, scope-clarifications.md, gap-analysis.md and the high-level design. Checked the source in `src/App.cpp` (lines 36-90, 205-270, 505-790, 1080-1310, 1311-1758), `src/App.h`, `src/core/Chunk.h`, `ChunkManager.h`, `ChunkCache.h`, `Face.h/.cpp`, `Camera.h/.cpp`, `Vertex3D.h`, `Object.h`, `src/shaders/vertex.glsl`, `CMakeLists.txt` and `.gitignore`. Read the standards in `.maister/docs/standards/global/minimal-implementation.md`. Nothing was built or run, and no files were modified.

### Line-reference spot checks (all confirmed)
| Claim | Evidence |
|---|---|
| Worldgen z typo | App.cpp:1192 `Vector(atPos.x, ..., atPos.y)` |
| Inclusive bounds | Chunk.h:89-91 uses `<=`; only caller App.cpp:1184 |
| Chunk copy | App.cpp:1214 `worldChunks.push_back(chunk)` |
| Inverted presence condition | App.cpp:1418 |
| Uniform buffers | App.cpp:264 `num_uniform_buffers = 1` |
| Div-by-zero and uninitialised members | App.cpp:725 `totalFPS / numFPS`; App.h:147-148 uninitialised |
| `chunkFaceCulling`, `canCreateVertexBufferEveryFrame`, `halfChunk` | App.cpp:41, 42, 47 (`halfChunk` used only at 1382, inside deleted code) |
| Dead lambdas | App.cpp:1334-1353 |
| E-key handler | inside `SDL_EVENT_KEY_UP` (608); lazy chunk creation at 648-652 |
| Face order and winding | App.cpp:1515-1533. I mapped the corners to +Y, -Y, +X, -X, +Z, -Z and the spec's pair order is correct. |
| CMake | `ChunkCache.h` listed in CMakeLists.txt:32; glslang custom command outputs to `src/shaders/`; `vertex.spv` is git-tracked |
| Mesh vs `Object.Position` | Baseline builds corners from the cell index (`x - kBlockHalfExtent`), not `Object.Position` |

### Consistency with the binding user decisions
| Decision | Result |
|---|---|
| Unloaded neighbour solid on X/Z, air on ±Y | Met (Req 9, Key Decisions). Single predicate in `ChunkManager` for local coordinates in [-1, N]. |
| `setBlock`/`removeBlock` API, dirty self plus boundary neighbours | Met (Req 6). E-key routed through `removeBlock` (Req 7). |
| Lazy chunk creation kept, dirty new chunk and neighbours | Met. It lives in `setBlock`. |
| Single-threaded remesh before render pass | Met (Req 13). |
| No tests | Met, recorded as a documented deviation. |
| No build-config change | Met. Only the source list changes (Req 16). `vertex.spv` regenerates through the existing custom command. |
| Worldgen bug fix, `PositionInBounds` exclusive | Met (Req 4). I confirmed that after the fix each of the four `ConstructChunkAtLine` calls yields one distinct chunk key. |
| Minimal cleanup (replaced code plus dead lambdas) | Met (Req 14, Out of Scope). |
| Benchmark logs conditions, forces nothing | Met (Req 1). |

---

## Findings

### H1. Baseline reference reads uninitialised memory; the spec calls it a "frozen cache" (Ambiguous / Incorrect)
**Severity**: High (gates success criteria 3 and 5; cheap to mitigate)

**Spec reference**: Open Questions ("baseline geometry ... frozen per-cell frustum cache, inverted presence condition at App.cpp:1418"); Stage 1 step 5; Success Criteria 3 and 5 (hold-pose image "identical to the baseline screenshot", frame-time improvement versus baseline).

**Evidence**:
- App.cpp:1418-1432: `if (chunk.cache.isValidCache) { rebuild from blocks } else { blockPresence = chunk.cache.cachedBlockPresence; }`.
- On the first build, `isValidCache` is false, so the code copies `cachedBlockPresence`. That is a default-initialised `std::array<bool, N^3>` (ChunkCache.h:28) and is never written before this read.
- The rebuild branch is effectively unreachable: once the cache is valid, the early return at App.cpp:1394 fires.
- So the baseline's block presence, and therefore its geometry, vertex counts and Build time, depend on whatever bytes were in memory. The image may be right, partly wrong or empty.
- The spec covers the frustum-frozen part only (constraining the start pose). The research report does mention the uninitialised bitmap (research-report.md:36), but the spec drops it.

**Gap**: Stage 1 has no step confirming the baseline actually renders the expected terrain, and no fallback reference if it doesn't. The stage 2 and 3 "identical image" gates are then unsound.

**Recommendation**:
- Add a stage 1 check that the baseline hold-pose image shows all four chunks with plausible terrain.
- State the fallback: if it doesn't, the reference for stages 2-3 is a stage 2 image checked against expected terrain, and baseline timings are labelled non-comparable.
- Name the uninitialised read in the Open Risks.

### M1. `setBlock` (and lazy chunk creation) has no caller and no verification (Extra / Incomplete)
**Severity**: Medium (not a spec conflict, a quality risk)

**Spec reference**: Req 6; Open Risks ("`setBlock` has no runtime caller ... Drop it if a review flags it"); Standards Compliance (minimal-implementation).

**Evidence**:
- The add path at App.cpp:671-698 stays commented out, so nothing calls `setBlock`.
- None of the five stages verifies `setBlock`, lazy chunk creation, or the "dirty new chunk plus 6 neighbours" rule.
- `minimal-implementation.md`: "Create only methods ... that will actually be called" and "Unused Code Is Debt".
- The reference-invalidation hazard the spec names (`emplace_back` reallocating `worldChunks`) lives exactly in this untested code.

**Assessment**: The user's binding decision mandates the `setBlock`/`removeBlock` API, so keeping it is defensible, and the spec discloses the deviation. The untested creation branch is the real risk.

**Recommendation**: Add a temporary verification step (a debug-only caller in stage 2, reverted before commit) that exercises `setBlock` into an empty cell of an existing chunk and into a not-yet-created chunk. Alternatively, ask the user to confirm that untested, uncalled code is acceptable (Q3).

### M2. Benchmark path is underspecified (Ambiguous)
**Severity**: Medium (affects the repeatability the whole task measures)

**Spec reference**: Req 2; Stage 1 verification 2; "Discard the first report window ... benchmark windows are aligned to path loops".

**Evidence and gaps**:
1. The hold frame count is "fixed" but not given, and "practical" screenshot capture depends on it (about 90 FPS means 60 hold frames is under 1 s).
2. Behaviour after the 600-frame revolution is undefined: repeat, repeat with hold, or freeze.
3. The profiler window is 600 frames and the path is hold + 600 frames. A window after toggle-on therefore covers the hold plus only part of the orbit. The claim "windows are aligned to path loops" is not true as written.
4. "Discard the first window after app start" conflicts with "toggling B resets the window", since the first benchmark window starts at toggle-on.
5. No yaw/pitch formula is given. The convention is `forward = (cos p sin y, sin p, -cos p cos y)` (Camera.cpp:121); an implementer must derive it.

**Feasibility check**: The recommended pose (radius 28, camera height 30, centre (15.5, 3, 15.5), vertical FOV 80, aspect 16:9, far 100) does fit all four chunks (32x32 footprint). The closest corner is about 21 degrees off the view axis and the worst case stays inside the half-FOV.

**Recommendation**: Fix the hold length (for example 300 frames), define post-revolution behaviour (recommended: hold, then orbit again in a loop of length hold + 600, with the report window equal to one loop), and give the yaw formula.

### M3. Stage 2 scaffold adds throwaway work with its own risk (Extra)
**Severity**: Medium-Low (accepted design choice, noting cost)

**Spec reference**: Technical Approach, "Stage 2 transitional scaffold".

**Evidence**:
- Stage 2 flattens mesher output into the old single scene buffer and uploads it through the old path, which keeps the unchanged shader.
- That path sizes the buffer once (`canCreateVertexBufferEveryFrame = false`, App.cpp:42, 1614-1625), with no regrow. It is safe in stage 2 only because no culling is applied and only removals are tested (meshes shrink).
- The scaffold must be written, verified and then deleted in stage 3.

**Recommendation**: State the "removals only" limit in stage 2 (it is implied) or allow merging stages 2 and 3 to avoid the scaffold. The spec's choice is workable.

### L1. Documentation inconsistency: 4 stages vs 5 (Incorrect)
**Severity**: Low. The TL;DR says "Built in 4 verified stages ... then delete cache/LOD" while Implementation Guidance defines five stages. Align the wording.

### L2. Dead members left behind by the cleanup list (Incomplete)
**Severity**: Low. **Spec reference**: Req 5, Req 14, minimal-implementation.

**Evidence**:
- `Chunk::didUserEditChunk` (Chunk.h:23) is referenced only in a comment inside the deleted LOD code (App.cpp:1388).
- `Chunk::numNonEmptyBlocks` (Chunk.h:83) has no caller.
- The const `operator[]` has no caller (grep found none). Req 5 deletes only the non-const one, and the integration table says "accessors trimmed" without naming these.

**Recommendation**: List them explicitly as delete or keep, per the "no dead code" standard. `numNonEmptyBlocks` predates this task, so leaving it is defensible.

### L3. "Nothing can bypass dirty marking" is overstated (Incorrect)
**Severity**: Low. **Spec reference**: Key Decisions ("non-const `tryGet`/`operator[]` are deleted so nothing can bypass dirty marking").

**Evidence**: `Chunk::blocks` and `ChunkManager::worldChunks` are public, and `chunkMap` has no accessor restriction, so direct mutation stays possible. The deletion removes the convenient bypasses only.

**Recommendation**: Reword to "removes the accessor bypasses". Making `blocks` private is out of scope and not needed.

### L4. Repo-root build logs are already tracked in git (Ambiguous)
**Severity**: Low. **Spec reference**: Stage 5 ("no stray build logs from the repo root are committed"); conventions.

**Evidence**: `git ls-files` lists `build-last.txt`, `build-output.txt`, `current-build.txt` and `patched-build.txt`. The working tree also has untracked `*.err`, `*.out` and similar files at the root.

**Recommendation**: Clarify that the criterion means "this task adds none". Otherwise a verifier will either flag pre-existing tracked files or scope-creep into removing them.

### L5. SDL semantic claims not independently re-verified (Ambiguous)
**Severity**: Low. **Spec reference**: Key Decisions ("SDL defers release"), Req 10-12.

**Evidence**: No SDL3 headers or source were found locally, so I could not re-read the SDL docs. Each claim is consistent with SDL3's documented model:
- `SDL_ReleaseGPUBuffer` and `SDL_ReleaseGPUTransferBuffer` defer destruction until in-flight command buffers finish.
- `SDL_PushGPUVertexUniformData` between draws applies to subsequent draws.
- `cycle = true` on the map and on each destination upload is legal.
- The 80-byte std140 layout `{row_major mat4; vec4}` puts the vec4 at offset 64.
- The existing code already releases the transfer buffer right after upload (App.cpp:1679), which supports the deferral claim.

**Recommendation**: Stage 3 checks 4-5 (device debug plus repeated toggling) already cover this at runtime. Keep them mandatory.

---

## Standards compliance
| Standard | Assessment |
|---|---|
| coding-style | OK. Helpers moved, not copied; dead code removed; naming per class family is stated. See L2 for leftover dead members. |
| minimal-implementation | Mostly OK. See M1 (`setBlock` without a caller, disclosed); stage 2 scaffold is explicitly temporary (M3). |
| commenting | OK. |
| error-handling | OK. Keeps the existing log, release, submit, `FAILURE` pattern; store methods return `bool`. |
| conventions | OK. No new dependencies; see L4 for the build-logs wording. |
| test-writing | Documented deviation, consistent with the user's decision. |

## Implementability
- The four-chunk world, key `B` (unused: ESC, W, A, S, D, SPACE, LSHIFT, F, E are taken, and E is a KEY_UP handler), `num_uniform_buffers = 1` and the CMake source-list-only edits are all confirmed feasible.
- Mesher inputs are unambiguous except for the position source: Req 8 does not say the corners come from the cell index rather than `Object.Position`. The baseline uses the cell index, and `Object.Position` has a known y-offset defect for chunks below y=0 (spec Open Risks). Recommend stating "cell index" explicitly (Low).
- `static_assert(sizeof(Vertex3D) == 60)` should hold: three floats for `Vector` (Vector.h), four for `SDL_FColor`, two UVs and two more vectors, which is 60 bytes with no padding.

## Compliance status
PASS WITH CONCERNS (Mostly Compliant). Critical 0, High 1, Medium 3, Low 5.
