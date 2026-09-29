# Chunk / Mesh Caching Findings (chunk-mesh-caching)

## TL;DR
- The only cache is `Chunk::cache` (`ChunkMeshCache<16>`, ChunkCache.h:23-29): per-chunk merged "LOD blocks" (12 Faces each) plus a block-presence bitmap. It is read and written only inside the per-frame render loop, App.cpp:1363-1558.
- It is never invalidated: nothing ever sets `isValidCache=false`, and block removal (E key, App.cpp:657-660) does not touch it, so edited chunks keep showing stale meshes.
- A cache hit does not avoid the dominant per-frame work: the scene is copied out of the cache (App.cpp:1396), re-flattened (1591-1604), re-transformed by `GetFaceDrawCallVerticies` (1608) and re-uploaded via a fresh transfer buffer every frame (1648-1681). No persistent per-chunk GPU mesh exists.
- Logic bugs: inverted `isValidCache` branch (1418/1431), frustum-culled subset cached (1495-1506, 1553), `reserve(4096)` LODDBLocks per chunk per frame before the hit check (1390-1391).
- No eviction or memory bound; par loop is race-free in practice (one thread per chunk).

## Key Decisions
- Sizes are derived from struct layout by reading, not measured: Vector = 3 floats (Vector.h:16, 12 B); Face = 3 Vectors + Matrix3D of 9 floats (Face.h:14-16, Matrix3D.h:18-20) = ~72 B; LODDBLock = 12 Faces + std::byte (ChunkCache.h:10-11) ~ 872 B; Vertex3D ~ 60 B (Vertex3D.h:13-18: 12+16+12+8+12). Padding not verified with a compiler.

## Open Questions / Risks
- No profiler data; impact estimates are heuristic. Behavior after E-edit and the inverted-branch effect are inferred from code, not run.
- Whether SLog1 (per-frame logging, App.cpp:1560-1575) compiles out in release was not checked.
- Single-threaded event/render loop assumed (E-key edit vs par render) but not verified in App.h.

## 1. What is cached
- `ChunkMeshCache<N>` (ChunkCache.h:23-29): `std::vector<LODDBLock> lodBlocks`, `int cachedLOD=-1`, `bool isValidCache=false`, `std::array<bool,N^3> cachedBlockPresence` (no initializer; 4096 B for N=16).
- `LODDBLock` (ChunkCache.h:9-17): `std::array<Face,12> faces`; `std::byte numFaces{}` is never used in App.cpp.
- Owned by value per chunk: `ChunkMeshCache<N> cache;` Chunk.h:26. Chunks in `std::vector<Chunk<16>> worldChunks` (ChunkManager.h:38); index in `std::map<Int3,size_t> chunkMap` (ChunkManager.h:41).
- Cached contents are world-space Faces: cube corners (App.cpp:1508-1533) then `blockFace += chunk.atPosition` (1535-1537). Vertex3D (position/normal/tangent/UV) is NOT cached; `Face::GetFaceDrawCallVerticies` (Face.cpp:3-81) recomputes it each frame.
- Mesh.h is unrelated to the chunk cache: `Mesh` is raw pointers to a shared cube (`cubeMesh`, App.cpp:502). Each block is an `std::optional<Object>` (Chunk.h:21) with Mesh* + position + 3 angles (Object.h:32-38): redundant per-block data for a uniform cube.

## 2. Cache key
- Effective key: chunk identity + `cachedLOD == LOD` (App.cpp:1394). LOD is hard-coded 0 (App.cpp:1388; real computation commented out), so the key is constant.
- Not in key: camera/frustum, block-content version, neighbours (see 5).

## 3. Invalidation
- `isValidCache` is only ever set true (App.cpp:1550). Grep across App.cpp/App.h/core finds no assignment of false. `didUserEditChunk` (Chunk.h:23) has its only setter commented out (App.cpp:696) and its use in LOD selection commented (1388).
- E key break: `blockAt->reset()` (App.cpp:657-660) leaves `chunk.cache` untouched. Placement code is commented out (662-696). After an edit a valid, LOD-matching cache returns stale `lodBlocks` (1394-1398), so the removed block stays rendered.
- A newly created chunk (App.cpp:649-651) has a default (invalid) cache and builds normally.
- `Chunk::isEmpty(int LOD)` (Chunk.h:77-81) uses the cache but the render loop calls `isEmpty()` (App.cpp:1407); the LOD overload is unused.

## 4. Eviction / memory bounds
- None: no LRU, cap, or chunk unloading; `worldChunks` only grows (App.cpp:650, 1214). World is 4 columns at startup (App.cpp:518-521).
- Per chunk always allocated, even if empty: 4096 x sizeof(optional<Object>) (Chunk.h:21) + 4 KB bitmap + lodBlocks vector.
- Worst case per chunk if all 4096 cells were blocks: 4096 x ~872 B ~ 3.5 MB of cached faces. `chunkFaceCulling` is `false` (App.cpp:41), so every block, including interior ones, stores all 12 faces.

## 5. Correctness/design problems in cache logic
1. Inverted branch: App.cpp:1418 `if (chunk.cache.isValidCache) { recompute presence from blocks } else { blockPresence = chunk.cache.cachedBlockPresence; }` (1431-1433). The comment at 1417 says the opposite. On first build (invalid cache) it copies the uninitialized `cachedBlockPresence` (ChunkCache.h:28) into the presence grid, so the first mesh is built from garbage/UB. High confidence (code read).
2. Frustum-dependent data cached: per-LOD-block frustum test skips invisible blocks (1495-1506) before `emplace_back` (1539); `lodBlocks` is then stored (1553) and marked valid. Later frames hit (1394) and reuse that first-view subset, so geometry missing from the first view stays missing when the camera turns (chunk-level reject at 1380 returns before touching the cache, which is fine).
3. Empty-chunk early return (1407) never validates the cache, so empty in-frustum chunks rescan 4096 optionals (`isEmpty`, Chunk.h:72-75) every frame.
4. Copies: `chunk.cache.lodBlocks = std::move(chunkLocalBlocks)` then `chunkLODBlocks[idx] = chunk.cache.lodBlocks` (1553-1554) copies the whole vector; the hit path also copies (1396). A pointer/reference would avoid the copy.
5. `chunkLocalBlocks.reserve(4096)` runs at 1390-1391, before the hit check (1394) and empty check (1407): ~3.5 MB allocated per in-frustum chunk per frame even on hits. The comment claims "once per thread"; it is per chunk per frame.
6. `chunkFaceCulling` block (1443-1478) is dead (constexpr false) and has index typos (`x != 0` guards z-1 at 1467; `x` guards z+1 at 1473). Not a perf item now.

## 6. Per-frame recomputation (App.cpp:1363-1681)
Every frame, cache hit or not:
- `chunkLODBlocks` vector-of-vectors sized to worldChunks (1360-1361), filled by copies (1396, 1554).
- Flatten to `nonFrustrumCulledFaces` (1587-1604): second full copy, ~864 B per block.
- `GetFaceDrawCallVerticies` per face (1608-1612): 3 Matrix3D multiplies (Face.cpp:43-45), 2 cross products + 2 normalizations (47, 49), tangent multiply + normalize (51), 3 UV choices. Rotation matrix is identity (Face.h:16; faces built without rotation at App.cpp:1524-1533), so the multiplies are no-ops; normals/tangents/UVs depend only on static cube-face geometry and position.
- `std::vector<Vertex3D> verticies(total)` allocated and zero-filled (1583), memcpy'd into (1611), then memcpy'd again into the mapped transfer buffer (1666).
- New `SDL_GPUTransferBuffer` created and released every frame (1651, 1681).
- Vertex buffer created once (1614-1620) and only resized if `canCreateVertexBufferEveryFrame` (constexpr false, App.cpp:42) - so the resize branch (1627-1645) is dead. If visible vertex count later exceeds the first frame's size, the upload (1678-1679) writes `vertexDataSize` bytes into a smaller buffer: overflow risk (logic read, not run).
- Net: the cache saves only presence scan + cube-corner generation; the dominant cost (Face to Vertex3D, ~36 vertices x 60 B = ~2.1 KB per block, copies, upload) is uncached.
- The "Greedy meshing took" timer (1562-1564) measures nothing (no code between start and log).

## 7. Allocation patterns
| Site | Per frame? | Approx size |
|---|---|---|
| `chunkLODBlocks` (1360) | yes | n_chunks vectors |
| `reserve(4096)` LODDBLock (1391) | yes, per in-frustum chunk | ~3.5 MB each |
| lodBlocks copies (1396, 1554) | yes | ~872 B x n_blocks |
| `nonFrustrumCulledFaces` (1587-1594) | yes | ~864 B x n_blocks |
| `verticies` (1583) | yes | ~2.1 KB x n_blocks |
| GPU transfer buffer create/release (1651/1681) | yes | same as verticies |
| `worldChunks.push_back(chunk)` copy (App.cpp:1213-1214) | load time | copies whole Chunk incl. cache |

## 8. Thread-safety
- `std::for_each(std::execution::par, ...)` over chunks (1363). Each lambda mutates only its own `chunk.cache` (1549-1556) and its own `chunkLODBlocks[idx]` (1396, 1554; idx from pointer difference, 1364). Distinct-element writes: no race. Counters are atomics (1355-1356, 1397, 1541). `sceneCamera` is only read (1371, 1496).
- Second pass uses `par_unseq` (1608) with an atomic `cpyIndex.fetch_add(3)` (1606-1607). Atomics/synchronization are not formally permitted under par_unseq (medium confidence, standard rule) and output vertex order becomes nondeterministic frame to frame (harmless for opaque triangles, possible z-fight flicker on coplanar faces).
- No mutex guards cache or chunk map. E-key edit (App.cpp:639-660) is on the event path, presumed non-concurrent with render.
- `emplace_back` on `worldChunks` (650) may reallocate; safe because `chunkMap` stores indices, not pointers (ChunkManager.h:41), but each reallocation moves whole Chunks (4096-slot array + cache).

## 9. Related observations
- `chunkMap` is `std::map<Int3,size_t>` (ChunkManager.h:41): O(log n) node-chasing lookups on every raycast step (App.cpp:1221-1223) and gravity rays (597, 1103). `<unordered_map>` is already included (ChunkManager.h:5). Low-medium impact.
- `Chunk` stores `std::array<std::optional<Object>, 4096>` (Chunk.h:21; comment "ai says it can be faster"); a block-id byte array would shrink memory ~10x+ and make `isEmpty`/`numNonEmptyBlocks` (Chunk.h:72-87, both O(4096), uncached) cheaper.
- Possible typo outside scope: new chunk created at `Vector(atPos.x, ..., atPos.y)` (App.cpp:1192) uses atPos.y for z; flag for worldgen gatherer.

## Gap list (impact / effort / confidence)
| # | Gap | Evidence | Impact | Effort | Conf. |
|---|---|---|---|---|---|
| 1 | Final vertices not cached; full Face->Vertex3D + upload each frame | App.cpp:1583-1681, Face.cpp:3-81 | High | Med | High |
| 2 | No invalidation / dirty flag on edit | App.cpp:657-660, ChunkCache.h:27 | High (correctness) | Low | High |
| 3 | Cache stores frustum-culled subset | App.cpp:1495-1506, 1553 | High (correctness) | Low | High |
| 4 | Inverted isValidCache branch, uninitialized bitmap | App.cpp:1418, 1431; ChunkCache.h:28 | High (correctness) | Low | High |
| 5 | reserve(4096 LODDBLock) per chunk per frame | App.cpp:1390-1391 | Med-High | Low | High |
| 6 | Cache copies + second flatten copy | App.cpp:1396, 1554, 1591-1604 | Med | Low | High |
| 7 | Per-frame transfer buffer + verticies alloc | App.cpp:1583, 1651, 1681 | Med | Med | High |
| 8 | Vertex buffer sized once; growth overflow risk | App.cpp:42, 1614-1645, 1678 | Med (correctness) | Low | Med |
| 9 | No neighbour face culling; 12 faces per block | App.cpp:41, 1524-1541 | High | Med | High |
| 10 | Redundant identity-matrix multiplies/normalizes | Face.cpp:43-51, Face.h:16 | Low-Med | Low | High |
| 11 | std::map chunk lookup; fat Chunk storage | ChunkManager.h:41, Chunk.h:21 | Low-Med | Med | Med |
| 12 | No cache eviction/memory bound | ChunkCache.h, App.cpp:1553 | Low now | Low | High |
| 13 | par_unseq with atomic slot allocation | App.cpp:1606-1612 | Low | Low | Med |
| 14 | Empty chunks not cached | App.cpp:1407, Chunk.h:72 | Low | Low | High |
