# Decision Log

## TL;DR
- 5 ADRs, one per decision area, all Accepted; alternatives are analyzed in [solution-exploration.md](solution-exploration.md).
- Most consequential: ADR-002 (dirty-flag mesh replaces Face cache) and ADR-003 (per-chunk persistent GPU buffers) - together they remove the per-frame rebuild/upload.
- ADR-001 accepts relative-only timings if the build is Debug (no build-config change); ADR-005 defers block-layout work.
- All decisions keep the project dependency-free (SDL3 + standard library).

## Key Decisions
- ADR-001: in-app timers + benchmark path, no CMake/preset change.
- ADR-002: dirty flag replaces the Face cache.
- ADR-003: per-chunk persistent GPU buffers with pooled transfer and regrow policy.
- ADR-004: neighbor-aware cross-chunk culling, chunk-local vertices, stated boundary policy.
- ADR-005: keep `std::optional<Object>` per block for now.

## Open Questions / Risks
- Debug-build timings are relative-only (ADR-001); absolute conclusions unsafe.
- Deferred GPU buffer release and edit-vs-parallel-meshing thread safety are unverified (ADR-002, ADR-003).
- Unloaded-neighbor boundary policy must be chosen at specification (ADR-004).

---

## ADR-001: In-app per-stage timers and benchmark path without build-config change

### Status
Accepted

### Context
Nothing in SDL_3DR has been profiled; the only number on record is an integer-FPS change attributed to logging. Research suspects Debug-build artifacts inflate copy/STL costs. The user chose not to change build configuration and to stay dependency-free. See [Area 1](solution-exploration.md#decision-area-1-measurement-profiling-and-build-configuration).

### Decision Drivers
- Each later change must be justified by data
- Dependency-free (no Tracy)
- Roadmap item "profiling instrumentation"; learning value
- No CMake/preset changes

### Considered Options
1. 1A: Release preset + existing timers
2. 1B: In-app per-stage timers (no build change)
3. 1C: External profiler (Tracy, RenderDoc, Nsight)
4. 1D: No measurement

### Decision Outcome
Chosen option: **1B without the Release preset**, because it ranks stages repeatably (avg/p95/max, fixed benchmark camera path) with no new dependency or build changes. Timings are relative-only if the build is Debug.

### Consequences

#### Good
- Repeatable before/after comparison for every later stage
- Permanent, cheap instrumentation; teaches timing statistics

#### Bad
- Debug timings may misrank stages (copy/STL inflated); absolute numbers not trustworthy
- CPU-only: no GPU time
- Timers perturb very small stages

---

## ADR-002: Replace the Face cache with a per-chunk dirty flag

### Status
Accepted

### Context
`Chunk::cache` holds CPU world-space Faces, is inverted (App.cpp:1418), never invalidated, and stores a camera-dependent frustum-culled subset; even on a hit the frame copies, flattens, regenerates vertices and uploads. The cache is in the wrong layer. See [Area 2](solution-exploration.md#decision-area-2-fixing-the-cpu-chunk-cache).

### Decision Drivers
- Correctness by construction (no stale or camera-dependent cache)
- Alignment with GPU-side ownership (ADR-003)
- Less code, single developer, no tests
- Blocks change rarely (E-key edit only)

### Considered Options
1. 2A: In-place fix of `ChunkMeshCache`
2. 2B: Dirty-flag chunk mesh, chunk-level culling
3. 2C: Remove caching, regenerate every frame
4. 2D: Versioned multi-LOD cache

### Decision Outcome
Chosen option: **2B**, because a `meshDirty` flag with chunk-level frustum culling removes the whole bug class and is the prerequisite for per-chunk GPU buffers. Remove `isValidCache`, `cachedBlockPresence` and the per-block frustum test. The 2A correctness subset (invalidate on edit, do not cache a frustum subset, initialize bitmap) is an optional stopgap only if later stages are delayed.

### Consequences

#### Good
- Removes stale/camera-dependent bugs; net code reduction
- Edits touch one chunk plus boundary neighbors

#### Bad
- Touches the edit path and OnRender together
- Full payoff only with ADR-003; temporary window where vertices regenerate only when dirty but still upload scene-wide
- Dirty-flag access must be safe against parallel meshing (unverified)

---

## ADR-003: Per-chunk persistent GPU vertex buffers

### Status
Accepted

### Context
Every frame builds one merged vertex array and uploads it through a new transfer buffer into a single vertex buffer sized once (overflow risk, dead resize path since `canCreateVertexBufferEveryFrame=false`). See [Area 3](solution-exploration.md#decision-area-3-gpu-side-mesh-ownership).

### Decision Drivers
- Static chunks should cost zero CPU mesh/upload work
- Camera motion must not force rebuilds
- Fix vertex-buffer overflow risk
- Simplicity at a 4-chunk scale, transferable voxel-engine structure

### Considered Options
1. 3A: Single shared buffer, upload only when dirty
2. 3B: Per-chunk persistent buffers, dirty-flag remesh
3. 3C: Per-chunk buffers with indirect/multi-draw
4. 3D: Single buffer with sub-allocator and dirty regions

### Decision Outcome
Chosen option: **3B**, with a pooled/reused transfer buffer and a capacity + regrow policy (grow geometrically from actual vertex counts; defer release of replaced buffers until in-flight frames finish; release on chunk unload), because it has the best win-to-complexity ratio and evolves toward 3C/3D.

### Consequences

#### Good
- Zero upload on clean frames; edits upload one chunk
- Covers overflow risk and per-frame transfer buffer churn

#### Bad
- N draw calls (fine at 4 chunks; revisit at hundreds)
- Buffer lifetime bookkeeping; chunk-local vertices need a per-chunk offset in the pipeline
- Magnitude of benefit unmeasured

---

## ADR-004: Neighbor-aware cross-chunk face culling

### Status
Accepted

### Context
Every block emits 12 faces; `chunkFaceCulling` is false and its z-neighbor guards use x (App.cpp:1467, 1473). Terrain is shallow (0-4 blocks), so savings are density-dependent. See [Area 4](solution-exploration.md#decision-area-4-hidden-face-culling-and-meshing-strategy).

### Decision Drivers
- Largest expected geometry reduction for modest machinery
- Smaller per-chunk meshes reduce upload cost
- Correctness at chunk borders and after edits

### Considered Options
1. 4A: Fix and enable in-chunk culling only
2. 4B: Neighbor-aware culling including cross-chunk lookup
3. 4C: Greedy meshing
4. 4D: Indexed/compact vertices

### Decision Outcome
Chosen option: **4B**. Fix the z-guard typos first (4A as first half), emit chunk-local vertices, consult neighbor chunks via `ChunkManager` at borders, and mark neighbors dirty on boundary edits (ADR-002). Boundary policy for unloaded neighbors must be stated in the specification; recommended default is to treat missing chunks as air so world-edge faces remain visible.

### Consequences

#### Good
- Removes interior hidden faces including at chunk borders
- Correct by construction given neighbor dirtying

#### Bad
- Cross-chunk coupling in the mesher and neighbor invalidation
- Boundary policy affects visuals at world edge; wrong policy causes holes or wasted faces
- Greedy meshing/indexed vertices deferred (may be needed if triangle/bandwidth measured limiting)

---

## ADR-005: Keep `std::optional<Object>` block storage for now

### Status
Accepted

### Context
Chunks store `std::optional<Object>` per block (~160 KiB per chunk by estimate), giving poor locality and 4096-optional scans. At 4 chunks, no evidence that memory or generation time limits. See [Area 5](solution-exploration.md#decision-area-5-chunk-data-layout).

### Decision Drivers
- Avoid a broad refactor (worldgen, edit, raycast, mesher) for small current payoff
- Keep risk low with zero tests
- Preserve option to change later

### Considered Options
1. 5A: Keep `std::optional<Object>`
2. 5B: Compact block ids with registry
3. 5C: Hybrid ids + side table
4. 5D: RLE/octree compression

### Decision Outcome
Chosen option: **5A**, because the refactor cost exceeds the benefit at the current world size. Revisit 5B when any trigger fires: measured chunk memory or worldgen time matters, the mesher's neighbor lookups show up in profiles, or streaming/large view distance work starts.

### Consequences

#### Good
- Zero risk and effort now
- Mesher accesses blocks through a narrow interface so 5B can be swapped in later

#### Bad
- Continued bloat and poor locality; neighbor checks in the mesher stay slower than with compact ids
