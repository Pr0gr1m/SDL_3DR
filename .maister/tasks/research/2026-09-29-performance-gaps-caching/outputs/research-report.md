# Research Report: Performance Gaps and Caching in SDL_3DR

## TL;DR
- The only cache (per-chunk CPU list of world-space Faces, `Chunk::cache`) is broken: inverted branch (App.cpp:1418), never invalidated, and it bakes in the first frame's frustum-culled subset.
- Even on a cache hit, each frame still deep-copies, re-flattens, regenerates 60 B vertices and re-uploads the whole scene through a new transfer buffer; no per-chunk GPU mesh exists.
- Hidden-face culling, LOD and occlusion are absent or disabled; the build has no optimization flags.
- NOTHING WAS PROFILED. Impacts are heuristic and may be inflated by Debug builds. Measure first.

## Key Decisions
- Measure in a Release build with frame-time stats before changing code — every ranking here is unmeasured, and Debug inflates STL/copy costs.
- Fix cache correctness first (low effort, high value) — a stale or camera-dependent cache is a visible bug and blocks any caching strategy.
- Then adopt per-chunk persistent GPU meshes with a dirty flag — this removes the copy/flatten/upload/transfer-buffer cost in one change.
- Add neighbor-aware hidden-face culling after that — largest expected geometry reduction, needs cross-chunk lookup.

## Open Questions / Risks
- Real build type behind existing FPS claims is unknown (likely Debug); the only number is 79->89 FPS in commit 9274f71 for an undefined "small scene".
- All struct sizes are unverified estimates (no sizeof run).
- Runtime effect of the inverted presence branch (depends on uninitialized memory) and of the vertex-buffer overflow risk were not run.
- Commit f36f85d says "Occlusion culling done" but HEAD contains none (contradiction, see below).

Report type: technical research, static analysis. Date: 2026-09-29. Researcher: research-synthesizer.

## Table of Contents
1. Executive Summary
2. Research Objectives
3. Methodology
4. Findings
5. Analysis and Insights
6. Conclusions
7. Recommendations
8. Appendices

## 1. Executive Summary
Six gatherers reviewed chunk/mesh caching, GPU/frame loop, culling/LOD, world-gen/memory, math/raycast and build/history via static reading of the codebase, git log and build configuration. Source lines cited by several gatherers were spot-checked against `src/App.cpp`.

Main finding: the mesh cache is both incorrect and shallow. Its branch is inverted (App.cpp:1418 rescans blocks when the cache is valid and copies an uninitialized bitmap when invalid), it is never invalidated (`isValidCache` is only ever set true at 1550; the E-key edit at 657-660 does not touch it), and its content depends on the camera (per-block frustum test at 1495-1506 before caching at 1553). It is also shallow: the cache holds CPU Faces, so every frame still copies (1396, 1554), flattens (1587-1604), regenerates vertices (1608-1612) and uploads the entire scene via a per-frame transfer buffer (1648-1681). A `reserve` of ~4096 LODDBLocks (megabytes by estimate) runs per visible chunk per frame before the hit check (1390-1391).

Secondary gaps: no hidden-face culling (12 faces per block, non-indexed 60 B vertices), LOD disabled, no optimization build configuration, and many small recompute-when-unchanged items (frustum, view/projection, gravity ray, direction vectors). No profiling exists, so the ordering is judgment, not measurement.

## 2. Research Objectives
- Primary question: what performance gaps exist in SDL_3DR, especially around caching?
- Sub-questions: what is cached and how is it keyed/invalidated; what does the frame loop recompute; what GPU resources are (not) cached; what do culling/LOD, world-gen/memory, math/raycast and build config contribute?
- In scope: source under `src/`, git history, build/CMake config. Out of scope: runtime profiling, fixing code, driver-level analysis.

## 3. Methodology
Static analysis. 6 finding files (chunk-mesh-caching, gpu-frame, culling-lod, worldgen-memory, math-ray, build-config) covering App.cpp, core/*.h/.cpp, shaders, CMake, vcpkg, build logs and 23 commits. Framework: technical (components, patterns, flows) with gap analysis and cross-source validation. Synthesizer re-checked App.cpp:1386-1397, 1418-1433, 1544-1556 and grepped src for `isValidCache`, `reserve`, `occlu`, and the two constexpr flags.

## 4. Findings
| # | Finding | Category | Confidence | Sources |
|---|---|---|---|---|
| F1 | isValidCache branch inverted; first build copies uninitialized bitmap | Cache correctness | High | 5 gatherers; verified |
| F2 | Cache never invalidated; E-key edit leaves stale mesh | Cache correctness | High | 3; verified by grep |
| F3 | Cache stores frustum-culled subset | Cache correctness | High | 4 |
| F4 | reserve(4096 LODDBLock) before hit check, per chunk per frame | Allocation | High (size Med) | 5; verified |
| F5 | Deep copy on hit and miss; second flatten copy | Copies | High | 5; verified |
| F6 | Full Face->Vertex3D regen + full upload every frame; no GPU per-chunk mesh | GPU/frame | High | 3 |
| F7 | New transfer buffer and vertex vector each frame | GPU/frame | High | 2 |
| F8 | Hidden-face culling off and buggy; 12 faces/block; non-indexed 60 B vertices | Geometry | High | 4; flags verified |
| F9 | Vertex buffer created once; overflow risk on growth | GPU correctness | Medium | 2 |
| F10 | LOD hard-coded 0; single cachedLOD slot | LOD | High | 3 |
| F11 | Frustum/view/projection rebuilt each frame | Recompute | High | 2 |
| F12 | Empty chunks rescanned each frame (4096 optionals) | Recompute | High | 3 |
| F13 | Chunk storage bloat (optional<Object> per block), std::map chunk lookup, weak Int3 hash | Memory/lookup | Medium | 4 |
| F14 | Fragment/texture issues: blend on, no mips, duplicate sampler, single texture, duplicate DestroySurface | GPU misc | Med-High | 1 |
| F15 | par_unseq + atomic slot allocation (nondeterministic order) | Concurrency | Medium | 2 |
| F16 | No Release/LTO config; toolchain mismatch; no measurement infrastructure | Build | High (config) | 1 |
| F17 | World gen single-threaded, no heightmap cache; noise ignores chunk offset | World gen | High | 1 |

Evidence highlights:
```
App.cpp:1394  if (chunk.cache.cachedLOD == LOD && chunk.cache.isValidCache) {
App.cpp:1396      chunkLODBlocks[idx] = chunk.cache.lodBlocks;   // deep copy
App.cpp:1418  if (chunk.cache.isValidCache) { ...rescan blocks... } else { blockPresence = chunk.cache.cachedBlockPresence; }
App.cpp:41-42 chunkFaceCulling = false; canCreateVertexBufferEveryFrame = false
```
Implications: cache hit-rate improvements are worthless without moving the cache to the GPU layer; correctness fixes are required regardless.

## 5. Analysis and Insights
Patterns (see analysis/synthesis.md): cache-in-wrong-layer (pervasive), recompute-when-unchanged (pervasive), data bloat, disabled/stub optimizations, copy-paste index bugs, no measurement culture.

Insights:
1. Correct cache logic alone will not reduce frame cost (High).
2. A camera-dependent cache is structurally wrong; cache full chunk meshes and cull at chunk/draw level (High).
3. Per-chunk persistent GPU buffers with dirty flags are the highest-leverage change (direction High, magnitude unknown).
4. Hidden-face culling is likely the largest geometry reduction; terrain is shallow so savings are density-dependent (Medium).
5. Build type likely confounds every prior "optimization" claim (Medium).
6. World-gen, raycast and math are minor at 4 chunks (Medium).

Relationships: OnRender flows frustum -> parallel per-chunk cache/mesh -> serial flatten -> parallel vertex regen -> transfer -> one scene draw. Chunks live in a vector indexed by `std::map`; edits bypass the cache.

Contradictions noted:
- Occlusion culling: culling-lod finds none; commit f36f85d says "done". grep of src finds no "occlu"; treat as absent at HEAD (Medium).
- Reserve size 1.7 MB vs 3.5 MB per chunk: unmeasured estimates, same order of magnitude.
- Chunk map: one gatherer says unordered_map; three and ChunkManager.h:41 say `std::map`. Treated as `std::map`.
- Dead resize/WaitForGPUIdle path (flag false) vs overflow risk: overflow is the live risk.

SWOT (of the current performance design):
- Strengths: only 2 draw calls; chunk-level frustum test is cheap and correct; per-chunk parallel meshing without data races; static GPU resources created once; shader build incremental.
- Weaknesses: broken cache; full re-upload every frame; heavy per-block data; no measurement.
- Opportunities: per-chunk GPU meshes, hidden-face culling, indexed/compact vertices, Release + LTO, real LOD with hysteresis.
- Threats: vertex-buffer overflow on edit/growth; stale geometry after edits; memory growth with no eviction when the world scales.

## 6. Conclusions
- Direct answer: the main performance gaps are (1) a CPU-only, incorrect, never-invalidated mesh cache that leaves the full copy/flatten/vertex/upload pipeline running every frame, (2) no hidden-face culling or working LOD, and (3) no optimization build configuration. Small per-frame recomputations in camera/raycast are secondary. Confidence: High on existence of these gaps, Low on their magnitude.
- Secondary: several correctness bugs surfaced (z-neighbor guard typos, atPos.y for z, noise ignoring chunk offset, double DestroySurface, dead line pass, integer FPS averaging).
- Overall confidence: Medium-High for structural claims, Low for quantitative impact.

## 7. Recommendations
| Pri | Recommendation | Effort | Rationale / Risk |
|---|---|---|---|
| 0 | Add Release/RelWithDebInfo preset (optionally LTO), record frame-time (ms, avg/p95) on a fixed scene, use existing SLog1 timers or a profiler | Low | Validates everything else; risk none |
| 1 | Fix cache: correct branch or drop presence cache, initialize bitmap, cache full chunk mesh (no frustum test inside), set invalid on block edit and on neighbor edit | Low | Correctness; risk: behaviour change visible |
| 2 | Move `reserve` after hit/empty checks, return reference/span instead of copying lodBlocks | Low | Cheap allocation/copy savings |
| 3 | Skip the per-frame rebuild when nothing changed; persistent transfer buffer or direct mapped write; size vertex buffer with growth policy | Med | Removes upload churn and overflow risk |
| 4 | Per-chunk GPU vertex buffers with dirty flag (subsumes 3 for static chunks) | Med-High | Largest expected win; risk: larger refactor, needs profiling to justify |
| 5 | Fix and enable neighbor-aware face culling (incl. cross-chunk); compact/indexed vertices | Med | Large geometry cut; density-dependent |
| 6 | Low-cost items: dirty-flag frustum/view/proj, constexpr gravity dir, chunk pointer reuse in DDA, unordered_map or last-chunk cache, bitmask move states, cache empty flag | Low | Small gains at current scale |
| 7 | Later: real LOD with hysteresis and multi-LOD cache, texture atlas/mips, streaming/eviction, block-id storage | Med-High | Only needed as world grows |

## 8. Appendices
### A. Sources
Finding files under `analysis/findings/`: chunk-mesh-caching-findings.md, gpu-frame-findings.md, culling-lod-findings.md, worldgen-memory-findings.md, math-ray-findings.md, build-config-findings.md. Source spot-checks: `src/App.cpp` 41-42, 1386-1397, 1418-1433, 1544-1556; grep across `src/`.

### B. Gaps and uncertainties
No profiler data; no sizeof; no run of edit/stale-cache scenario; thread-safety of edits vs render not verified; build logs are from another machine and mismatched toolchain; unclear if std::execution::par parallelizes; commit diffs not inspected for f36f85d.

### C. Methodology notes
Impact/effort ratings are qualitative. Estimates: Vector 12 B, Face ~72 B, LODDBLock ~872 B, Vertex3D 60 B (12+16+12+8+12), per-block GPU data ~2.1 KB (36 vertices), Chunk ~160 KiB; all derived from field layout, not compiler output.

Companion files: analysis/synthesis.md, outputs/research-report.html.
