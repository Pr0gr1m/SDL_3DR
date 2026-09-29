# Solution Exploration: Performance Gaps and Caching in SDL_3DR

## TL;DR
- Five decision areas explored (measurement, CPU cache, GPU mesh ownership, hidden-face/meshing, chunk data layout), 4 alternatives each.
- Recommended path: Release + frame-time stats first; drop the CPU Face cache in favor of a dirty-flag chunk mesh; per-chunk persistent GPU buffers; neighbor-aware face culling; leave data layout alone for now.
- Order matters: measure -> invalidation/mesh ownership -> per-chunk GPU buffers -> neighbor culling (needs neighbor dirtying) -> layout only if memory or gen time is measured to matter.
- Confidence: medium. All research is static; nothing was profiled and magnitudes are unknown.

## Key Decisions
- Area 1: Release/RelWithDebInfo build plus an in-app CPU/GPU frame-time overlay (ms, avg/p95) — matches the roadmap's "profiling instrumentation" item and is a learning goal in itself.
- Area 2: Remove the CPU Face cache and replace it with a per-chunk `dirty` flag driving remeshing — the cache holds the wrong data layer (F1-F3, F5) and its fixes are subsumed by Area 3.
- Area 3: Per-chunk persistent GPU vertex buffers, one draw per visible chunk, rebuilt only when dirty — highest-leverage change, moderate refactor, still simple to reason about.
- Area 4: Neighbor-aware hidden-face culling (cross-chunk) generating only exposed faces; defer greedy meshing and indexing — biggest expected geometry cut for the least new machinery.
- Area 5: Keep `std::optional<Object>` for now; introduce compact block ids only when measurement (memory, gen time) demands it — lowest-value change at 4 chunks.

## Open Questions / Risks
- Every magnitude is unmeasured; if Release shows the frame is GPU-bound or already fast, Areas 3-5 lose urgency (Area 1 gates them).
- Per-chunk buffers with a 4-chunk world may add draw-call overhead that exceeds the upload savings only in unusual cases; profile-gated.
- Neighbor culling needs cross-chunk lookup and neighbor dirtying on edit; boundary blocks of unloaded chunks need a policy (treat as air vs solid).
- Thread safety of E-key edits versus parallel render (par) is unverified; remeshing on dirty must respect it.
- The toolchain mismatch (MinGW logs vs MSVC-triplet vcpkg) may mean the developer's real build config differs from what was analyzed.

## Problem Reframing

### Research Question
What performance gaps exist in SDL_3DR, especially related to caching? Findings (research-report.md, synthesis.md): the only cache (`Chunk::cache`) is inverted (App.cpp:1418), never invalidated (set true only at 1550), and holds a camera-dependent frustum-culled subset (1495-1506, 1553); even on a hit, copy/flatten/vertex regen/full upload run every frame (1396, 1587-1681); no hidden-face culling (`chunkFaceCulling=false`, App.cpp:41); no optimization build flags. Problem scope for this exploration is HOW to close those gaps, not whether to add features (LOD, streaming, lighting).

### How Might We Questions
1. HMW establish trustworthy performance numbers so each later change is justified by data, not by Debug-build artifacts? (Area 1)
2. HMW make "unchanged chunk means no work" true and correct, including after block edits? (Area 2)
3. HMW stop re-creating and re-uploading the entire scene's vertices every frame? (Area 3)
4. HMW stop generating faces the player can never see? (Area 4)
5. HMW keep per-block memory and generation cost proportional to what the renderer needs? (Area 5)

---

## Explored Alternatives

Notation: ratings are H/M/L for each perspective where H is good (feasibility: easy to build; user impact: benefit; simplicity: simple; risk: H = low risk; scalability: H = scales well). Evidence tags refer to findings F1-F17 in research-report.md.

## Decision Area 1: Measurement, Profiling and Build Configuration

### Alternative 1A: Release preset + existing timers
Add a Release/RelWithDebInfo CMake preset (optionally LTO), reuse existing `SLog1` timers, run a fixed scene and compare by eye/log.
- Strengths: minutes of work; removes the biggest confound (F16).
- Weaknesses: integer FPS and log lines are noisy; logging itself cost 7 FPS earlier (commit 9274f71); no GPU time.
- Best when: you only need a quick sanity check of Debug vs Release.
- Evidence: no `-O` in logs, CMake sets no build type (F16); integer FPS averaging noted.

### Alternative 1B: In-app frame-time instrumentation (recommended)
Release preset plus a small scope-timer facility feeding a ring buffer: per-stage CPU ms (frustum, mesh, flatten, vertex gen, upload, render), frame ms avg/p95/max, shown in a periodic on-screen line or one log line per second; fixed camera "benchmark path".
- Strengths: directly implements roadmap "profiling instrumentation"; repeatable; teaches timing/statistics; cheap to keep permanently.
- Weaknesses: small amount of code; CPU timers do not show GPU time; timers perturb tiny stages.
- Best when: many candidate optimizations need ranking (exactly this situation).
- Evidence: no measurement infrastructure (F16); SWOT weakness "no measurement".

### Alternative 1C: External profiler (Tracy or similar) plus GPU tooling (RenderDoc, Nsight)
Integrate Tracy zones; use RenderDoc/Nsight for GPU frame cost and upload traffic.
- Strengths: best-in-class visibility, GPU timeline, memory; strong learning value for graphics profiling.
- Weaknesses: new dependency in a vcpkg project with a MinGW/MSVC ambiguity; setup cost; overkill for 2 draw calls today.
- Best when: after 1B shows something that needs deeper inspection.

### Alternative 1D: Skip measurement, apply fixes by code reading
Fix everything the static analysis identified.
- Strengths: fastest to "improvement".
- Weaknesses: ignores that Debug likely inflates copies and STL; a stale cache makes FPS look better than correct behavior (synthesis contradiction 7); risks optimizing wrong things.
- Best when: never for a performance-learning project; listed for honesty.

### Trade-off matrix (Area 1)
| Alt | Technical feasibility | User impact | Simplicity | Risk | Scalability |
|---|---|---|---|---|---|
| 1A | H | L | H | H | L |
| 1B | H | M | H | H | M |
| 1C | M | M | L | M | H |
| 1D | H | L | H | L | L |

### Recommendation (Area 1): 1B (with 1A as its first step)
Rationale: Release build first, then lightweight in-app per-stage timers; escalate to 1C only for questions 1B cannot answer (GPU time). Trade-off accepted: a bit of instrumentation code and no GPU timing initially. Assumption: a fixed camera path is representative. Confidence: high.
Why not: 1A (too coarse to rank stages); 1C (premature dependency); 1D (unvalidated, and the stale cache distorts baselines).

---

## Decision Area 2: Fixing the CPU Chunk Cache

### Alternative 2A: In-place fix of `ChunkMeshCache`
Correct the inverted branch, initialize the bitmap, stop frustum-culling inside the cache build (cache all faces), set invalid on edit and on neighbor edit, return a reference/span instead of copying, move `reserve` after the hit/empty checks.
- Strengths: smallest diff; fixes F1-F5 quickly; keeps the current pipeline shape.
- Weaknesses: the flatten/vertex-regen/upload work still runs each frame (cache still in the wrong layer); the cache now holds all faces so per-frame frustum culling must move to a per-frame filtering step, adding work; single `cachedLOD` slot remains.
- Best when: you want a quick correctness win before larger refactors.
- Evidence: F1-F5, F10; insight 1 ("fixing cache logic alone will not fix frame cost").

### Alternative 2B: Redesign as a dirty-flag chunk mesh (recommended)
Replace the Face cache with a `Chunk::meshDirty` flag plus a stored chunk-local mesh (or nothing CPU-side once uploaded, see Area 3). Any block edit sets dirty on that chunk and on adjacent chunks when the edit touches a boundary. Frustum culling happens at chunk level only. Remove `isValidCache`, `cachedBlockPresence`, per-block frustum test.
- Strengths: removes a whole class of stale/camera-dependent bugs by construction; aligns with Area 3; less code than today.
- Weaknesses: only pays off fully when combined with Area 3; requires touching the edit path and the OnRender lambda; a temporary regression window if done before Area 3 (CPU vertices regenerated on dirty only, but still uploaded each frame).
- Best when: you are willing to do the GPU-side follow-up.
- Evidence: F1-F3, insight 2 (cache full chunk mesh, cull at chunk level).

### Alternative 2C: Remove caching entirely, regenerate every frame
Delete the cache; optimize the per-frame path (avoid reserve, cull chunks, parallelize).
- Strengths: simplest; no invalidation problem; may be adequate for 4 chunks in Release.
- Weaknesses: cost scales with visible blocks every frame; cannot support larger worlds; abandons a core roadmap theme (mesh caching).
- Best when: Release measurements show the current scene is far under budget.
- Evidence: 4-chunk scale (insight 6); F4, F5 removal of per-frame allocations.

### Alternative 2D: Versioned/hash-keyed cache with multi-LOD slots
Key cache entries by (chunk version counter, LOD); keep multiple LOD entries, with hysteresis; validity is a compare of version numbers, avoiding explicit invalidation calls.
- Strengths: robust to missed invalidations; supports future LOD (F10).
- Weaknesses: more machinery than the current 4-chunk, LOD-0-only world needs; LOD is hard-coded to 0; over-engineered now.
- Best when: real LOD is activated.

### Trade-off matrix (Area 2)
| Alt | Technical feasibility | User impact | Simplicity | Risk | Scalability |
|---|---|---|---|---|---|
| 2A | H | M | H | H | L |
| 2B | M | H | H | M | H |
| 2C | H | L | H | H | L |
| 2D | M | L | L | M | H |

### Recommendation (Area 2): 2B, staged
Stage 1 can be 2A's correctness subset (invalidate on edit, stop caching a frustum subset) if Area 3 is delayed; Stage 2 replaces it with dirty-flag meshes. Trade-off accepted: touching the edit path and OnRender in one go. Assumptions: blocks change rarely (E-key edit only); neighbor dirtying is enough for correctness. Confidence: medium-high.
Why not: 2A alone (cache stays in the wrong layer); 2C (throws away the project's caching theme and loses headroom, though it is a valid fallback if Release results are great); 2D (speculative until LOD exists).

---

## Decision Area 3: GPU-Side Mesh Ownership

### Alternative 3A: Keep one shared vertex buffer, upload only when anything is dirty
Build the scene vertex array only when a chunk changed or visibility set changed; reuse a persistent transfer buffer; add a growth policy for the vertex buffer.
- Strengths: minimal change to the draw path (1 scene draw); fixes F7 and F9; most frames do zero upload.
- Weaknesses: camera movement changes the visible chunk set, so the merged buffer rebuilds often in a moving-camera scene; any single edit re-uploads everything.
- Best when: the camera is mostly static or the world is tiny.
- Evidence: F6, F7, F9.

### Alternative 3B: Per-chunk persistent GPU vertex buffers, dirty-flag remesh (recommended)
Each chunk owns an `SDL_GPUBuffer` and vertex count. On dirty: rebuild that chunk's vertices, upload via a reused/pooled transfer buffer, update count. Frame: frustum-test chunks, bind buffer and draw per visible chunk (or a handful of draws).
- Strengths: static chunks cost zero CPU mesh/upload work; camera motion causes no rebuild; edits touch one chunk (plus neighbors); maps cleanly to Area 2B; typical voxel-engine structure so it is transferable learning.
- Weaknesses: N draw calls (trivial at 4 chunks, needs attention at hundreds); buffer lifetime management (release on unload, deferred delete while in flight); per-chunk buffer size varies (needs capacity + regrow policy).
- Best when: chunks are mostly static and the count is modest to moderate.
- Evidence: insight 3; F6, F7, F9; synthesis dependency "invalidation before per-chunk GPU buffers".

### Alternative 3C: Per-chunk buffers plus indirect/multi-draw
As 3B but issue draws as a batch with indirect draw commands (SDL3 GPU supports indirect draw), populating a command buffer of visible chunks; optionally GPU-driven culling with compute.
- Strengths: scales to many chunks; fewer CPU draw calls; strong graphics-learning value.
- Weaknesses: SDL3 GPU indirect-draw support and multi-draw specifics add complexity; needs one shared/suballocated buffer or buffer arrays; harder to debug; no evidence draw calls are a bottleneck.
- Best when: chunk counts reach hundreds and CPU draw submission shows up in profiles.

### Alternative 3D: Single merged buffer with a sub-allocator and dirty regions
One big GPU buffer divided into chunk regions (free list/slab); only dirty regions are re-uploaded with copy passes; one draw call or a few over the ranges.
- Strengths: 1 buffer, few binds; partial updates; efficient at scale.
- Weaknesses: allocator, fragmentation and defragmentation, region regrow; the most machinery for a 4-chunk world; culled chunks still resident but need per-range draws anyway.
- Best when: memory and buffer-count pressure appear (streaming worlds).

### Trade-off matrix (Area 3)
| Alt | Technical feasibility | User impact | Simplicity | Risk | Scalability |
|---|---|---|---|---|---|
| 3A | H | M | H | H | L |
| 3B | M | H | M | M | M |
| 3C | L | M | L | M | H |
| 3D | L | M | L | L | H |

### Recommendation (Area 3): 3B
Rationale: best win-to-complexity ratio and evolves naturally toward 3C/3D. Trade-off accepted: N draw calls and buffer-lifetime bookkeeping. Assumptions: buffer create/destroy cost is acceptable on dirty (rare); SDL3 GPU handles many small buffers fine at this scale. Confidence: medium-high on direction, low on magnitude. Guard against F9 by sizing per-chunk buffers from actual vertex counts with a capacity policy.
Why not: 3A (rebuild whenever the camera moves; keeps the merged-scene design that made the problem); 3C and 3D (premature without evidence draw calls or buffer count matter).

---

## Decision Area 4: Hidden-Face Culling and Meshing Strategy

### Alternative 4A: Fix and enable existing face culling (in-chunk only)
Fix the z-guard typos (App.cpp:1467, 1473), enable `chunkFaceCulling`, cull faces between two solid blocks within a chunk; chunk borders emit all faces.
- Strengths: tiny change; big cut already inside chunk interiors; fixes known bugs.
- Weaknesses: faces on the 16-block chunk borders remain (with 16^3 chunks, roughly 6 boundary faces of 16x16 cells per chunk stay conservative); still 12 vertices per face pair (two triangles, non-indexed).
- Best when: you want a quick win now.
- Evidence: F8; App.cpp:41; copy-paste index bugs pattern.

### Alternative 4B: Neighbor-aware culling including cross-chunk lookup (recommended)
Generate a face only when the neighbor cell is air, consulting the adjacent chunk via `chunkMap` for border cells (treat missing chunks as air or solid by a stated policy). Emit chunk-local, not world-space, vertices. On edits at boundaries, mark neighbor chunk dirty.
- Strengths: complete removal of interior hidden faces; smaller per-chunk meshes benefit Area 3 uploads; correct by construction with dirty neighbors.
- Weaknesses: needs cross-chunk access and neighbor dirtying; light complexity in the mesher; boundary policy decisions.
- Best when: chunk meshes are rebuilt rarely (they are, with Areas 2-3 done).
- Evidence: insight 4; F8; ordering dependency in synthesis.

### Alternative 4C: Greedy meshing
Merge coplanar same-texture faces into larger quads.
- Strengths: fewest triangles; strong learning value.
- Weaknesses: texture coordinates must tile or use a texture array/atlas (single texture, no atlas today, F14); per-face UV/normal handling changes; nonuniform lighting/AO later becomes harder; the "greedy meshing" timer in the code is an unused stub (pattern 4). Terrain is shallow (0-4 blocks), so surface quads dominate and merging depends on texture variety.
- Best when: triangle count is measured as the bottleneck after 4B and textures tile.

### Alternative 4D: Indexed/compact vertices (4 verts + 6 indices per face, packed attributes)
Replace 36 x 60 B per block with indexed quads and packed vertex data (constant normal/tangent removed or derived from face id).
- Strengths: roughly a large reduction in vertex bytes; complements 4B; less upload and bandwidth.
- Weaknesses: shader changes (unpack), index buffer management in Area 3 buffers; benefit is bandwidth, and Area 3 already removes most per-frame uploads.
- Best when: vertex bandwidth or memory shows up after 4B.
- Evidence: data-bloat pattern 3; per-block ~2.1 KB estimate.

### Trade-off matrix (Area 4)
| Alt | Technical feasibility | User impact | Simplicity | Risk | Scalability |
|---|---|---|---|---|---|
| 4A | H | M | H | H | M |
| 4B | M | H | M | M | H |
| 4C | L | M | L | M | H |
| 4D | M | M | M | M | M |

### Recommendation (Area 4): 4B (4A as the first half, same code)
Rationale: largest expected geometry reduction with modest machinery; 4C and 4D are natural later steps if profiling calls for them. Trade-off accepted: cross-chunk coupling. Assumptions: air-outside-loaded-world is acceptable at the edge (faces at world boundary visible); terrain density makes savings meaningful (medium confidence, terrain 0-4 blocks thick). Confidence: medium.
Why not: 4A only (leaves border faces and is a subset of 4B); 4C (texture/atlas prerequisites, speculative benefit); 4D (mostly bandwidth, mostly obviated by Area 3).

---

## Decision Area 5: Chunk Data Layout

### Alternative 5A: Keep `std::optional<Object>` per block
Leave storage as is; optionally add a cached `isEmpty` flag and fix chunk lookups.
- Strengths: zero risk; no refactor; adequate at 4 chunks.
- Weaknesses: ~160 KiB per chunk by estimate; poor cache locality; the empty-chunk scan touches 4096 optionals (F12, F13).
- Best when: memory and generation time are not measured problems.

### Alternative 5B: Compact block ids (uint8/uint16) with a palette/registry for block properties
Store `std::array<uint16_t, N^3>`; block metadata (texture, solidity) looked up in a table; `Object` created on demand.
- Strengths: roughly two orders of magnitude smaller; fast scans and neighbor checks (helps Area 4B); trivial `isEmpty`/counters.
- Weaknesses: touches worldgen, edit path, raycast, mesher; per-block `Object` state (if any, such as orientation or health) needs a home; broad refactor for a 4-chunk world.
- Best when: streaming or large view distance is targeted (roadmap "future considerations").

### Alternative 5C: Hybrid: compact ids for meshing plus rich object side table
Ids for terrain plus a sparse map for blocks needing per-instance state.
- Strengths: keeps flexibility for special blocks; most blocks stay compact.
- Weaknesses: two representations to keep consistent; extra concept for a learner project with no special blocks yet.
- Best when: block entities/state are added.

### Alternative 5D: Run-length or octree/sparse voxel compression
- Strengths: tiny memory for sparse or uniform terrain; enables huge worlds.
- Weaknesses: complex random access/edit; overkill; no evidence memory is limiting.
- Best when: world sizes move well beyond current scale.

### Trade-off matrix (Area 5)
| Alt | Technical feasibility | User impact | Simplicity | Risk | Scalability |
|---|---|---|---|---|---|
| 5A | H | L | H | H | L |
| 5B | M | M | M | M | H |
| 5C | L | M | L | M | H |
| 5D | L | L | L | L | H |

### Recommendation (Area 5): 5A now, 5B when triggered
Triggers: measured chunk memory or worldgen time matters, or the mesher's neighbor lookups show up in profiles, or streaming is started. Trade-off accepted: continued bloat and poorer locality. Assumption: the 4-chunk world stays small. Confidence: medium.
Why not: 5B now (biggest refactor for smallest current payoff); 5C and 5D (speculative).

---

## Trade-Off Analysis

Combined view of the recommended alternative in each area against its runner-up (H is good on every axis).

| Area | Choice | Feasibility | User impact | Simplicity | Risk | Scalability | Note |
|---|---|---|---|---|---|---|---|
| 1 Measurement | 1B in-app timers + Release | H | M | H | H | M | gates all others |
| 2 CPU cache | 2B dirty-flag mesh | M | H | H | M | H | depends on 3B for full payoff |
| 3 GPU ownership | 3B per-chunk buffers | M | H | M | M | M | subsumes copy/flatten/upload |
| 4 Meshing | 4B neighbor-aware culling | M | H | M | M | H | needs neighbor dirtying |
| 5 Layout | 5A keep (5B later) | H | L | H | H | L | deferred by evidence |

Perspective priorities (no user dialogue was available; derived from vision.md): the project is a learning vehicle with a performance goal, so learning value and measurability weigh alongside raw speed; simplicity is favored because there is a single developer; low risk is favored because there are no tests (0% coverage).

## User Preferences
No user preferences were supplied; the orchestrator handles convergence. Constraints assumed from project docs: single developer, C++23, SDL3 GPU/Vulkan, 4-chunk world, 16^3 chunks, learning and performance goals, roadmap high priority "profiling instrumentation" and "further rendering performance work".

## Recommended Approach
Combined sequence (each step independently shippable and measurable with Area 1's tooling):
1. Release/RelWithDebInfo preset plus per-stage frame-time instrumentation and a fixed benchmark path (1A -> 1B). Record a baseline.
2. Correctness stopgap (only if the next steps are delayed): invalidate on edit, stop caching a frustum subset, initialize the bitmap (2A subset).
3. Replace the CPU Face cache with dirty-flag chunk meshes; chunk-level frustum test only (2B).
4. Per-chunk persistent GPU vertex buffers, pooled transfer buffer, capacity/regrow policy (3B).
5. Neighbor-aware face culling with cross-chunk lookup and neighbor dirtying (4B); re-measure.
6. Hold data layout (5A); reconsider 5B, 4C, 4D, 3C/3D only on measured triggers.
Primary rationale: the research shows the cache is in the wrong layer and the frame pays for full regeneration and re-upload; moving ownership to per-chunk GPU meshes with dirty flags removes the dominant structural cost while removing code, and neighbor culling then shrinks each mesh. Measurement first prevents optimizing Debug-only artifacts.
Key trade-offs accepted: a multi-step refactor of OnRender, cross-chunk coupling in the mesher, N draw calls, continued bloated block storage.
Key assumptions: block edits are rare; frames are CPU-bound on mesh/upload in Release (Area 1 tests this); SDL3 GPU handles per-chunk buffers cheaply; the 4-chunk scale stays small in this iteration.
Assumptions whose failure would change the recommendation: if Release shows the frame is already GPU-bound or well under budget, stop after Area 1 and the bug fixes (2C is then reasonable); if edits become frequent, revisit dirty-region uploads (3D).
Confidence: medium.

## Why Not Others
- 1A alone: too coarse to rank stages. 1C: premature dependency. 1D: unvalidated, and the stale cache distorts baselines.
- 2A alone: correct but leaves the pipeline running every frame. 2C: loses headroom and the project's caching theme (valid if Release is fast). 2D: needs real LOD first.
- 3A: rebuilds on camera motion and re-uploads the whole scene on any edit. 3C: no evidence of draw-call bottleneck. 3D: allocator complexity without scale need.
- 4A alone: subset of 4B. 4C: needs tileable textures/atlas and only helps if triangle count is the limit. 4D: mostly bandwidth, largely obviated by per-chunk persistent buffers.
- 5B/5C/5D now: refactor cost exceeds current benefit at 4 chunks.

## Deferred Ideas
- Real LOD with hysteresis and multi-LOD mesh storage (F10) — only meaningful after per-chunk meshes exist.
- Occlusion culling (commit message claims it, HEAD has none) — separate feature; revisit after profiling.
- Texture atlas/array, mipmaps, duplicate sampler cleanup, blend state review (F14).
- Threaded chunk generation/meshing and streaming/eviction (roadmap future).
- World-generation fixes: noise ignoring chunk offset, no heightmap cache (F17).
- Small recompute items: dirty-flag frustum/view/projection, constexpr gravity direction, DDA chunk pointer reuse, `unordered_map` or last-chunk cache, Int3 hash quality (F11-F13).
- Concurrency review of `par_unseq` with atomics and E-key edit versus render (F15).
- Unrelated bugs found: double `SDL_DestroySurface` (951, 1062), `atPos.y` used for z (1192), dead line pass, integer FPS averaging.
- Lighting/AO (constraint on greedy meshing) and GPU-driven culling.
