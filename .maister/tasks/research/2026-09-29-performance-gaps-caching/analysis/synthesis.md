# Synthesis: Performance Gaps and Caching in SDL_3DR

## TL;DR
- The only cache (`Chunk::cache`, a per-chunk CPU list of world-space Faces) is broken and cannot deliver its intended benefit: inverted branch (App.cpp:1418), never invalidated (only ever set true, 1550), and it stores a frustum-culled subset (1495-1506, cached at 1553).
- Even on a cache "hit" the dominant per-frame work still runs: vector deep copy, re-flatten, Face->Vertex3D regeneration, new transfer buffer, full upload (App.cpp:1396, 1587-1681). The cache never reaches the GPU.
- Nothing was profiled. All impact ratings are heuristic; the build has no optimization flags, so Debug-build behaviour may inflate CPU-side conclusions.

## Key Decisions
- Treat cache correctness bugs (stale, camera-dependent, uninitialized) as gaps equal to perf gaps: they decide whether caching can be relied on.
- Recommend measuring first (Release build plus frame-time stats) before any refactor.

## Open Questions / Risks
- No profile and no sizeof measurement: all struct sizes are read-from-layout estimates (Face ~72 B, LODDBLock ~872 B, Vertex3D 60 B, Chunk ~160 KiB).
- Whether real runs are Debug (likely: no -O in logs, CMake sets no build type).
- Real effect of the inverted branch depends on whether `cachedBlockPresence` happens to be zero (no initializer, ChunkCache.h:28).

## Research Question
What performance gaps exist in SDL_3DR (C++23 voxel renderer, SDL3 GPU/Vulkan), especially related to caching?

## Cross-Source Analysis

### Validated (multiple gatherers agree; spot-checked against source)
| Claim | Gatherers | Source check |
|---|---|---|
| Inverted `isValidCache` branch | caching, gpu, culling, worldgen, math | Verified: App.cpp:1418 `if (isValidCache)` rescans blocks; else copies `cachedBlockPresence` (1431-1433). Comment at 1417 says the opposite. |
| Cache never invalidated | caching, culling, worldgen | Verified by grep: only ChunkCache.h:27 (default false) and App.cpp:1550 (true). No false assignment. |
| Cache stores frustum-culled subset | caching, gpu, culling, worldgen | Consistent citations (per-block plane test before emplace_back, cache write 1553). Not fully re-read. |
| `reserve(4096)` before hit check | caching, gpu, culling, worldgen, math | Verified: App.cpp:1390-1391 precede the hit check at 1394. Comment ("once per thread") is misleading. |
| Deep copy on hit and miss | caching, gpu, culling, math, build (diff) | Verified: App.cpp:1396 and 1554. |
| Full re-flatten / re-upload every frame | caching, gpu, culling | Consistent citations (1587-1681); not individually re-read. |
| No optimization build flags | build (math notes LTO unchecked) | Config review only; no build type or -O in CMake or logs. |
| `chunkFaceCulling=false`, `canCreateVertexBufferEveryFrame=false` | all | Verified: App.cpp:41-42. |
| `cachedLOD` is set on build | (checked by synthesizer) | Verified: App.cpp:1556, so hits are reachable for LOD 0. |

### Contradictions and discrepancies
1. Occlusion culling: culling-lod reports none exists; build history quotes commit f36f85d "Occlusion culling done". A case-insensitive grep of src finds zero occurrences of "occlu". Resolution: the commit message is not borne out by HEAD; later commits (9976959, 373fa52, 1de1dc7) moved to face culling, which is now disabled and buggy. Medium confidence; commit diffs were not inspected.
2. Reserve size: ~3.5 MB per visible chunk per frame (caching, worldgen: 4096 x ~872 B) vs ~1.7 MB (math: 432 B per block). Neither is measured. Both agree on "MB per visible chunk per frame".
3. Severity of the inverted branch: caching/worldgen say the first build uses garbage/UB; culling says medium-low, unverified. Reconciled: definite logic bug by code read, observable effect depends on memory contents. It also makes a valid cache with changed LOD rescan.
4. Vertex buffer growth: gpu/caching flag an overflow risk when the flag is false; culling describes a resize with WaitForGPUIdle. That path is dead because App.cpp:42 is false. Overflow risk is the live concern (unrun).
5. Chunk index: math mentions an unordered_map lookup per DDA step; the other three and ChunkManager.h:41 say `std::map<Int3,size_t>`. Majority wins; Int3 hash quality is currently moot.
6. Minor line-number drift between gatherers (empty-chunk return 1407 vs 1414; cache copy 1552 vs 1554). Source shows 1407 and 1554.
7. A stale, camera-baked cache draws fewer triangles than a correct one, so measured FPS may look better than a correct implementation would.

### Confidence
- High: cache logic bugs, per-frame copy/reserve, no GPU-resident mesh, no optimization config, all 12 faces per block.
- Medium: vertex buffer overflow, par_unseq atomics, Int3 hash weakness, Debug-build assumption.
- Low: absolute cost of any item.

## Patterns and Themes
1. Cache in the wrong layer (architectural, pervasive): caches CPU Faces, not GPU meshes; all downstream stages are uncached.
2. Recompute-when-unchanged (implementation, pervasive): frustum/view/projection (App.cpp:1318-1322), direction vectors, constant gravity normalize, empty-chunk scan, transfer-buffer create/release.
3. Data bloat (design): `std::optional<Object>` per block, 12 world-space Faces per block, 60 B non-indexed vertices (~2.1 KB per block), constant normal/tangent stored per vertex.
4. Disabled or stub optimizations (organizational): LOD hard-coded 0, `chunkFaceCulling` false, "greedy meshing" timer measures nothing, no occlusion, empty line pass.
5. Copy-paste/index bugs: z-neighbor guards use x (App.cpp:1467, 1473), atPos.y used for z (1192), duplicate SDL_DestroySurface (951, 1062), noise ignores chunk offset.
6. No measurement culture: integer FPS, single recorded number (79->89 FPS, commit 9274f71, attributed to logging).

## Key Insights
1. Fixing cache logic alone will not fix frame cost: the hit path still copies, flattens, regenerates and uploads (High).
2. The cache cannot be correct while it stores frustum-culled output; it must hold the full chunk mesh and cull at chunk/draw level (High).
3. Highest-leverage structural change: per-chunk persistent GPU vertex buffers with dirty-flag remesh. It subsumes copy, flatten, upload, transfer-buffer churn and staleness (High confidence in direction, magnitude unmeasured).
4. Neighbor-aware hidden-face culling could remove most interior faces; currently off and buggy. Savings depend on terrain density (terrain is shallow, roughly 0-4 blocks) (Medium).
5. Debug-build confound: the "massively optimized" commit 5ecf119 and the 7 FPS attributed to logging were likely measured unoptimized; vector copies, bounds-checked std::array indexing and atomics are inflated in Debug (Medium).
6. World generation, raycast and math are minor at the current 4-chunk scale; they matter only with streaming or larger worlds (Medium).

## Relationships and Dependencies
- OnRender (App.cpp:1311-1756): frustum -> par per-chunk (cache check -> presence -> per-block cull -> Faces) -> serial flatten -> par_unseq Face->Vertex3D -> transfer buffer -> GPU copy -> 1 scene draw + 1 empty line draw.
- `worldChunks` vector plus `chunkMap` (std::map index) own chunks; each Chunk owns its cache by value. E-key edit (App.cpp:657-660) mutates blocks without touching the cache.
- Ordering: invalidation and full-chunk cache must precede per-chunk GPU buffers; hidden-face culling needs cross-chunk neighbor lookup and neighbor invalidation.

## Gaps and Uncertainties
- No profile, sizeof, Release run, or runtime confirmation of stale-cache visuals.
- Thread-safety of E-key edit vs parallel render not verified.
- Toolchain mismatch (MinGW vs MSVC-triplet vcpkg) means the logs may not reflect the developer's real build.
- Whether `std::execution::par` actually parallelizes on the toolchain.

## Synthesis by Framework (Technical)
- Components: ChunkManager/Chunk/ChunkMeshCache, App::OnRender lambda, Face/Vertex3D, SDL GPU upload, Camera/raycast.
- Pattern consistency: low; several half-finished optimizations.
- Flow: see Relationships.

## Prioritized Gap Table
| # | Gap | Impact | Effort | Confidence |
|---|---|---|---|---|
| 0 | No Release/optimization config; measure first | High (validity of all else) | Low | High (config) / Med (runtime) |
| 1 | Inverted isValidCache branch + uninitialized bitmap | High (correctness) | Low | High |
| 2 | No invalidation on edit | High (correctness) | Low | High |
| 3 | Cache stores frustum-culled subset | High (correctness) | Low | High |
| 4 | reserve(4096) before hit check, per chunk per frame | Med-High | Low | High |
| 5 | Deep copy on hit + second flatten copy | Med | Low | High |
| 6 | Full Face->Vertex3D regen + full upload per frame; no per-chunk GPU mesh | High | Med-High | High |
| 7 | New transfer buffer + vertex vector per frame | Med | Med | High |
| 8 | Hidden-face culling off/buggy; 12 faces per block; non-indexed 60 B verts | High | Med | High |
| 9 | Vertex buffer sized once (overflow risk) | Med (correctness) | Low | Medium |
| 10 | LOD disabled; single-slot cache, no hysteresis | Med | Med | High |
| 11 | Frustum/view/proj recomputed when static | Low-Med | Low | High |
| 12 | Empty chunks rescanned (4096) each frame | Low | Low | High |
| 13 | Chunk storage bloat, std::map lookup, weak Int3 hash, per-step raycast lookup | Low-Med | Med | Medium |
| 14 | Blend on, no mips, duplicate sampler, no atlas | Low | Low | Med-High |
| 15 | par_unseq with atomic slot allocation | Low | Low | Medium |

## Conclusions
Primary: (1) caching exists but is ineffective and unsafe; (2) the bottleneck class is per-frame full-scene CPU regeneration and upload plus unculled hidden faces; (3) build configuration may dominate observed numbers. Recommendation: measure in Release first, fix cache correctness (cheap), then move to per-chunk GPU meshes with dirty flags.
