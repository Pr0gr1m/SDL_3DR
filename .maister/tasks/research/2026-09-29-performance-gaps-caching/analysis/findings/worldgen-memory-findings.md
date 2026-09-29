# World Generation and Memory Findings (SDL_3DR)

## TL;DR
- World gen is one-shot at init: 4 columns x 256 (x,z) noise calls, single-threaded, no heightmap cache; cheap now but structurally non-scalable (no streaming/unloading, fixed 2x2 area).
- Chunk = 4096 `std::optional<Object>` (~40 B each, ~160 KiB est.) + 4 KiB presence array + per-chunk mesh cache; dense AoS, far larger than a 1-byte block id needs.
- Mesh cache logic is inverted (App.cpp:1418) and never invalidated on block edit (App.cpp:657-660); cache hits copy whole vectors every frame (App.cpp:1396, 1552).
- Int3 hash is weak (Int3.h:321-328) but unused on the hot path: chunkMap is `std::map<Int3,size_t>` (ChunkManager.h:240).
- Noise is sampled in local coords (no world offset), so all 4 columns get identical terrain.

## Key Decisions
- Sizes are estimates from field layout (no sizeof run), marked "est."
- Focus on gen/storage/hash/streaming/allocation; render upload covered by other categories.

## Findings

### 1. Terrain generation cost and caching
- `ConstructChunkAtLine` App.cpp:1120-1216, called 4x at App.cpp:518-521 (once, in init). The noise object is `new`'d then deleted right after (App.cpp:516, 528): no persistence, no reuse.
- Per (x,z): one 2D `SimplexNoise::noise(...)` (App.cpp:1160) = 256 calls/column, 1024 total. `fractal` (octaves) never used; ctor `SimplexNoise(0.15f,3,0,0)` (App.cpp:516). Noise is cheap: 256-entry permutation table `perm[uint8_t(i)]` (SimplexNoise.cpp:46, 74-75), static functions, no allocation.
- No heightmap cache: height is consumed immediately and not stored. No per-block noise calls (noise is per column; blocks filled at App.cpp:1204-1207), which is good.
- BUG: noise uses local `x*freq + epsilon` only (App.cpp:1160), ignoring `atPos.x/z`; all four columns get identical heightmaps. New chunk position uses `atPos.y` for z (App.cpp:1192). `PositionInBounds` is inclusive at upper bound (`<= atPosition+N`, Chunk.h:165), used for chunk matching (App.cpp:1184), so it can match the wrong chunk.
- Column loop does a linear scan of `yChunks` per column per level (App.cpp:1182-1189); tiny now.
- Height is `(noise+1)*2` -> ~0..4 blocks minus groundZeroYLevel(-2) (App.cpp:46, 1161-1170): shallow terrain.
- `tryInsert(Vector,...)` does 3x `std::lround` per block (Chunk.h:117-121, GlobalVariables.h:50): float round-trip instead of int indexing.
- Single-threaded generation; only the meshing loop uses `std::execution::par` (App.cpp:1363).

### 2. Chunk storage layout / memory footprint
- `Chunk<N>`: `std::array<std::optional<Object>, N^3>` inline (Chunk.h:96), N=16 (ChunkManager.h:216) -> 4096 slots.
- `Object` = `Mesh*` (8) + `Vector` (12) + alpha/beta/gamma (12) (Object.h:47-51) ~32 B est.; with optional ~40 B est. -> ~160 KiB per chunk (est.). Every block stores a redundant world Position, rotation floats and an identical mesh pointer (`cubeMesh.get()`, App.cpp:1205). A `uint8_t` block id would be 4 KiB. Author comment acknowledges ("ai says it can be faster", Chunk.h:96).
- `ChunkMeshCache<N>` embedded (Chunk.h:101, ChunkCache.h:192-198): `std::vector<LODDBLock>`, plus `std::array<bool,4096> cachedBlockPresence` (4 KiB, uninitialized).
- `LODDBLock` = `std::array<Face,12>` + byte (ChunkCache.h:178-180). `Face` holds 3 Vectors plus a `Matrix3D` (Face.h:14-23; ~72 B est.) -> ~870 B est. per visible block, storing 12 world-space triangles (App.cpp:1522-1531). No greedy meshing or face culling occurs (`chunkFaceCulling=false`, App.cpp:41; log label "Greedy meshing" at App.cpp:1564 times nothing).
- Iteration order: storage index `(y*N+z)*N+x` (Chunk.h:113-115; App.cpp:1411), x fastest. Meshing loops iterate y, x, z with z innermost (App.cpp:1435-1437, 1466-1470): innermost stride is N elements, cache-unfriendly; should be y,z,x. Generation loop also x,z outer / y inner (App.cpp:1157-1204).
- `Chunk::isEmpty()` scans all 4096 optionals (Chunk.h:147-150), called per chunk per frame on the cache-miss path (App.cpp:1407); no live block counter.
- `tryGet(Vector)` in hot loops (App.cpp:1422) builds a Vector and does 3 lround per block.

### 3. Hashing of Int3 keys
- `chunkMap` is `std::map<Int3,size_t>` (ChunkManager.h:240): red-black tree, O(log n), node allocations, pointer chasing. Int3.h:257 TODO shows the author debated map vs unordered_map.
- `std::hash<Int3>` (Int3.h:319-329): `hA ^ (hB*420) ^ (hC*67)` with identity int hash; weak mixing and collision-prone for chunk coords (multiples of 16, symmetric values). Currently unused by chunkMap.
- Lookups: `ChunkLookupFromBlockPosition` (App.cpp:68-84, floor + float division) then `find` (App.cpp:646, 1223), once per raycast step (App.cpp:1221-1234) with no last-chunk cache.
- Key from `atPosition.toInt3()` (App.cpp:1215).

### 4. Streaming / loading / unloading
- None. Fixed 4 columns at init (App.cpp:518-521; others commented App.cpp:522-524). `worldChunks` only grows (`emplace_back` App.cpp:650, `push_back` App.cpp:1214); no unloading, distance-based load, disk persistence or async.
- Player placement lazily creates chunks synchronously (App.cpp:646-654).
- `chunkMap` stores vector indices, so any future removal or reordering would invalidate them.

### 5. Allocation patterns / copies
- `worldChunks` is a `std::vector<Chunk>` with no `reserve` (ChunkManager.h:237); each element ~165 KiB inline, so growth reallocates and moves/copies MBs.
- `yChunks` local vector grows via `push_back` (App.cpp:1193), then chunks are COPIED into the world (`worldChunks.push_back(chunk)` from `const auto&`, App.cpp:1213-1214), ~165 KiB + cache vector each. `Chunk newChunk` on the stack (~165 KiB, App.cpp:1192); `blockPresence` 4 KiB stack array per thread task (App.cpp:1415). `currentlyWorkingChunk = &yChunks.back()` (App.cpp:1194) can dangle after the next reallocation; `const_cast` at App.cpp:1185.
- Per frame: `chunkLODBlocks` vector-of-vectors rebuilt (App.cpp:1360-1361); cache hit copies whole `lodBlocks` (App.cpp:1396); rebuild copies again (App.cpp:1552). `chunkLocalBlocks.reserve(4096)` (App.cpp:1391) is executed before the cache-hit and empty-chunk early returns: ~4096 x ~870 B = ~3.5 MB (est.) allocation per chunk per frame regardless.
- Cache correctness/perf bugs: inverted logic App.cpp:1418-1432 (when `isValidCache` true it rebuilds presence from blocks; when false it copies uninitialized `cachedBlockPresence`). `isValidCache` is only ever set true (App.cpp:1550); block removal (`blockAt->reset()`, App.cpp:657-660) never invalidates cache or neighbors. Cache is keyed by LOD only (LOD forced 0, App.cpp:1388) yet stores frustum-culled blocks (App.cpp:1498-1508), so a camera move reuses stale, culled geometry.

## Open Questions / Risks
- sizeof(Object/Face/Matrix3D) not measured; sizes are estimates.
- Actual generation and per-frame times not profiled; generation is likely negligible next to per-frame meshing/copy cost.
- Cache staleness and inverted logic inferred from reading only; not run.
- Whether MSVC `std::execution::par` actually parallelizes here not verified.
