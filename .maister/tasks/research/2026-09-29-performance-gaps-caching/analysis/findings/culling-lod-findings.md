# Culling / LOD / Visibility Findings

## TL;DR
- Visibility is recomputed every frame: frustum planes rebuilt (App.cpp:1318) and every chunk AABB-tested in a par for_each (App.cpp:1363). No occlusion culling exists; LOD is disabled (LOD hard-coded 0, App.cpp:1388).
- The only cache (Chunk::cache, ChunkCache.h:23-29) stores post-frustum-culled blocks and is never invalidated (isValidCache only ever set true, App.cpp:1550): stale after block edits AND after camera turns. Correctness bug as much as perf gap.
- Cache hit still deep-copies the chunk's lodBlocks vector per frame (App.cpp:1396), then everything is flattened, regenerated into vertices and fully re-uploaded every frame (App.cpp:1587-1681).
- blockPresence branch is inverted (App.cpp:1418-1433).
- Neighbor lookups use std::map<Int3,size_t> (ChunkManager.h:41); not used in the render loop (edit/raycast only), so low frame impact.

## Key Decisions
- Treated LOD as effectively dead code: GetLevelOfDetailFromDistance (App.h:105-113) is only referenced in a comment (App.cpp:1388).
- Reported inverted condition and missing invalidation as gaps since they determine whether caching helps at all.

## Findings (file:line)

### 1. Per-frame algorithm and complexity (App.cpp:1311-1682)
- Frustum planes recomputed every frame unconditionally: App.cpp:1318 -> Camera::UpdateCameraFrustrumCorners (Camera.cpp:25-102): 8 corners, 6 plane builds (cross + normalize), atanf + 4 tanf. Constant, cheap, but not gated on camera change. Also called at App.cpp:500 and key F (604).
- chunkLODBlocks (vector<vector<LODDBLock>>) allocated fresh each frame (1360-1361). Per non-culled chunk, `reserve(16^3 = 4096)` LODDBLocks (1391); each LODDBLock = 12 Faces (ChunkCache.h:9-17), multi-KB each -> huge allocation per chunk per frame, made BEFORE the cache-hit check at 1394, so wasted on hits and on the empty-chunk early return (1407).
- Chunk-level test: AABB vs 6 planes, O(chunks x 6) linear scan (1367-1380). No spatial hierarchy, no camera-moved dirty flag, no far-distance early out (far=100, Camera.h:63).
- Distance (sqrt) computed per chunk (1382-1387), unused because LOD=0 (1388).
- Rebuild path per chunk: O(N^3=4096) presence scan (1419-1430) + O(N^3) count (1481-1489 at LODBlockSize=1) + 6-plane per-block test (1495-1504) + 12 Faces built and translated (1524-1537).
- Flatten: serial nested copy of all Faces into one vector (1596-1604); then par_unseq per face GetFaceDrawCallVerticies with atomic fetch_add(3) (1608-1612) - atomic contention, nondeterministic order. Transfer buffer created/mapped/released and full upload every frame (1648-1681); GPU buffer recreated with SDL_WaitForGPUIdle when size changes (1627-1637, gated by canCreateVertexBufferEveryFrame). Culling changes (camera rotate) change vertex count nearly every frame.
- Dead code: isFaceInCameraFrustrum lambda (1338-1353), Camera::IsPointInFrustum (Camera.cpp:104-114). "Greedy meshing took" timer measures nothing (1562-1564).

### 2. Caching of visibility/LOD/occlusion
- No visibility cache across frames: no per-chunk visible flag, no camera-delta check. Whole pipeline reruns.
- No occlusion culling (grep for occlu: no hits). Backface cull mode NONE (App.cpp:475-479); chunkFaceCulling=false (App.cpp:41). Every LOD block emits all 12 faces (1524-1533); hidden interior faces are never removed.
- Mesh cache: ChunkMeshCache<N> {lodBlocks, cachedLOD=-1, isValidCache, cachedBlockPresence} (ChunkCache.h:23-29), member at Chunk.h:26. Key = cachedLOD only. Written 1548-1556; read 1394.
- No invalidation: block removal via E key (App.cpp:640-660, `blockAt->reset()`) never touches chunk.cache -> removed blocks keep rendering. didUserEditChunk (Chunk.h:23) never set (assignments are in commented code, App.cpp:696).
- Cache content depends on camera: per-block frustum test (1495-1506) filters blocks before caching (1553-1554). After first build, blocks outside the original view are permanently missing; blocks visible then stay drawn when out of view. Cache should hold the full chunk mesh; cull at chunk/draw level.
- Inverted condition: `if (chunk.cache.isValidCache) { rebuild presence } else { blockPresence = cachedBlockPresence }` (1418-1433). On first build (invalid) it copies cachedBlockPresence, which has no initializer (ChunkCache.h:28) -> indeterminate/zero data; result is stored at 1548-1550. Real impact unverified (medium-low confidence); when valid but LOD changes it rescans needlessly.
- Cache hit deep-copies: `chunkLODBlocks[idx] = chunk.cache.lodBlocks;` (1396, also 1554). Could use a pointer/span or per-chunk persistent GPU buffer.
- Empty chunks: early return at 1407 after allocation; Chunk::isEmpty() scans up to 4096 optionals every frame (Chunk.h:72-75); emptiness not cached (isEmpty(LOD) at Chunk.h:77 is unused here).
- Memory: each Chunk embeds array<optional<Object>,4096> (Chunk.h:21) + 4096 bool presence + lodBlocks; worldChunks.push_back(chunk) copies whole chunks (App.cpp:1214).

### 3. LOD
- Disabled: `auto LOD = 0; //chunk.didUserEditChunk ? 0 : GetLevelOfDetailFromDistance(distance);` (App.cpp:1388). Far chunks render at full detail.
- If re-enabled: LOD change makes cachedLOD != LOD (1394) -> full chunk remesh (no hysteresis, single cachedLOD slot, ChunkCache.h:26, so boundary oscillation thrashes; can't hold multiple LODs). GetLevelOfDetailFromDistance logs via SLog2 on every call and uses pow/log (App.h:105-113; comment at 110 suggests a table). LOD blocks use >50% fill threshold (App.cpp:1491), dropping sparse geometry (hence the didUserEditChunk hack).
- App.cpp:1073 max_lod is texture mip, unrelated.

### 4. Neighbor lookups and data structures
- ChunkManager.h:38-41: std::vector<Chunk<16>> worldChunks + std::map<Int3,size_t> chunkMap (O(log n), pointer chasing; <unordered_map> included but unused, ChunkManager.h:5). emplace_back (App.cpp:650) can reallocate/move all chunks; render loop derives index by pointer subtraction (1364) - safe only while no edits occur concurrently.
- Map lookups occur in edit (App.cpp:642), CheckIsPointInsideAny (1220-1225) and raycast per DDA step; not in culling loop.
- No cross-chunk neighbor lookup for face culling: chunkFaceCulling code (1443-1478) uses tryGet inside the chunk only, has copy/paste bugs (z-1 guarded by `x != 0` at 1467; z+1 by x at 1473), only sets presence true and never skips faces: effectively no-op.
- Camera::currentMoveStates is std::map<MoveStates,bool> (Camera.h:56) - heap nodes for 6 keys; std::array<bool,6> suffices (minor).

### 5. Redundant per-frame work
1. Frustum rebuild when camera static (1318).
2. Linear all-chunk test, no spatial structure.
3. 4096-LODDBLock reserve per visible chunk (1391).
4. Deep copy of cached blocks (1396).
5. Full flatten (1596-1604), vertex regeneration (1608-1612), transfer-buffer create + full upload (1648-1681), possible WaitForGPUIdle (1636).
6. SLog1 every frame (1560, 1564, 1569, 1575).
7. 12 triangles (36 verts) per cube with global coords, no index buffer/shared verts.

## Prioritized gaps (heuristic)
| Gap | Impact | Effort | Confidence |
|---|---|---|---|
| Cache never invalidated on edit + camera-dependent contents | High (correctness) | Low | High |
| Full re-flatten/re-upload every frame; no per-chunk persistent GPU mesh | High | Medium-High | High |
| Deep copy + 4096-block reserve per chunk per frame | Medium-High | Low | High |
| No hidden-face/occlusion culling (cross-chunk neighbors) | High | Medium | High |
| LOD disabled; single-LOD cache slot, no hysteresis | Medium | Medium | High |
| Inverted presence-cache branch | Medium | Low | Medium |
| Frustum/visibility recomputed when camera static; linear chunk scan | Low-Medium | Low | High |
| std::map chunkMap / map move states | Low | Low | High |

## Open Questions / Risks
- Is cachedBlockPresence zero-initialised in practice (ChunkCache.h:28)? Runtime check needed to gauge the inverted-branch impact.
- No profiling data; SLog1 timer at App.cpp:1560 could give the culling/mesh split.
- Hidden-face savings depend on world density (terrain columns built at App.cpp:1195-1211).
