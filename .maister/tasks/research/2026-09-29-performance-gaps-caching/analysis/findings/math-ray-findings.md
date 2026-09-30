# Math / Raycast Hot Paths Findings

## TL;DR
- Math primitives are header-inlined and cheap; the real costs are per-frame recomputation in App::OnRender and the per-frame raycast.
- Every frame OnRender unconditionally rebuilds frustum corners/planes (App.cpp:1318 -> Camera.cpp:25-102: 6 tan/atan, 6 cross+normalize), view, projection (tan) and VP (App.cpp:1320-1322), though camera state changes only on input/movement.
- One gravity raycast per frame in OnUpdate (App.cpp:1103), plus one wasted ray per Space keydown (App.cpp:597, result unused). Per-step cost is dominated by an unordered_map chunk lookup per DDA step, not math.
- Camera::UpdateDirectionVectors (trig + 3 normalizes) is rerun per held movement key per frame even when rotation is unchanged.
- The biggest per-frame cost seen in this scope is in the render lambda: a large `reserve` before the cache-hit early return and deep copies of cached vectors (App.cpp:1391, 1396, 1554).

## Key Decisions
- Impact ratings are heuristic (no profiler). Math/normalize costs are minor vs chunk-side costs, so ranked accordingly.

## Open Questions / Risks
- No profiling; impacts heuristic. Build flags/LTO not checked here (affects F8).
- Chunk size (16 used at App.cpp:1171) and sizeof(LODDBLock)/Face not verified; the "~1.7 MB reserve" figure assumes 16^3 blocks x 12 Faces x 36 B.
- F10 branch inversion needs runtime confirmation.
- Whether `debugLevel` is constexpr (SLog2 cost) not verified.

## Findings

### F1 Frustum planes/corners rebuilt every frame (Low-Med impact, easy fix, High confidence)
- App.cpp:1318 `sceneCamera->UpdateCameraFrustrumCorners();` at top of OnRender every frame. Also called at App.cpp:500 and on F key (App.cpp:603-605), suggesting on-demand was intended.
- Camera.cpp:61-98: `atanf(tanf(..))`, 4x `tanf`, 8 corners built from many Vector temporaries, 6 planes each with 2 subtractions, Cross, `Normalized()` (sqrt + zero-checked divide, Vector.h:68-73,103-110), and `Position + forward*(near+0.1f)` recomputed 6 times (Camera.cpp:88-98).
- Values depending only on fovY/aspect/near/far (tan(halfFovY), halfFovX, plane extents, Camera.cpp:61-69) never change at runtime yet are recomputed.
- Missed cache: dirty flag on Position/pitch/yaw/aspect. Note OnUpdate adds gravity each frame while airborne (App.cpp:1093,1108) so camera is dirty then. Planes could be extracted from the VP matrix (Gribb-Hartmann), removing corners and normalizes.

### F2 View/projection/VP recomputed every frame (Low impact, High confidence)
- App.cpp:1320-1322: GetViewMatrix (Camera.h:164-171), GetProjectionMatrix (Camera.h:177-192, `tan`, repeated `far - near` computations), full generic 4x4 multiply (Matrix4D.h:51-77, 64 mul) of a mostly-zero projection. Result to `float16Array` (App.cpp:1324-1325), pushed at App.cpp:1730.
- Projection depends only on fovY/aspect/near/far: cache and recompute on resize. View only on pose change.
- Matrices are returned by value (64 B) - RVO, not a real gap. Dead commented push at App.cpp:1327-1332.

### F3 Camera direction vectors rebuilt redundantly (Low impact, High confidence)
- Camera.cpp:117-124: 2 sin + 2 cos (cos(pitchRad) computed twice), a normalize of `forward` that is already unit by construction (121), and normalizes on `right`/`up` (up = right x forward of orthogonal unit vectors is already unit). `DEG_2_RAD = M_PI / 180.0f` is a double expression (GlobalVariables.h:74) and unqualified `cos/sin` may use double overloads.
- Call sites: mouse motion App.cpp:707-711 -> RotateCameraLocal (112-117); `MoveCameraHorizontal` App.cpp:97-98 and deprecated MoveCameraLocal 88-89 are called per held key per frame (App.cpp:120-135) although rotation did not change -> up to 4 redundant recomputes per frame. Dirty flag removes this.

### F4 Repeated normalize of constant gravity vector (Low impact, High confidence)
- `GetGravityVector()` is constexpr (0,-1,0)*9.81 (App.cpp:1307-1309); `.Normalized()` on the constant is evaluated every frame at App.cpp:1103 and 1105, and at 138, 142, 597. RaycastRay then normalizes again (App.cpp:1244) and calls `Magnitude()` (1245) - another sqrt, redundant since Normalized returns zero vector for zero input. Per-frame gravity raycast: about 3 sqrt for a constant (0,-1,0). Use a constexpr unit gravity direction.
- Vector::operator/ (Vector.h:68-73) branches on zero and does 3 divisions; Normalized() (Vector.h:103-110) already tested zero, so duplicate branch and 3 divs instead of 1 div + 3 mul.
- MoveCameraHorizontal normalizes per call (App.cpp:104): needed but could be once per frame.

### F5 Raycast frequency and per-step cost (Medium impact, High confidence)
- Frequency: OnUpdate every frame (App.cpp:1103); Space keydown (App.cpp:597, `raycastHit` unused - pure waste); look-ray on key at App.cpp:640-643 (event-driven, fine). Gravity ray is straight down, maxDistance 1.5 (kCameraHeightAboveGround, GlobalVariables.h:71).
- Per-step (App.cpp:1277-1302): each step calls CheckIsPointInsideAny (App.cpp:1219-1240): BlockPositionFromPoint (App.cpp:72-78), ChunkLookupFromBlockPosition (App.cpp:80-86; float->int->float floor->int round trips at 68-70), `chunkMap.find` with Int3 hash (App.cpp:1223; Int3.h:78-85), `worldChunks[idx]`, `tryGet`, Vector args by value, RaycastHit returned by value.
- No chunk-pointer reuse between steps: consecutive steps nearly always stay in the same chunk yet do a full hash lookup. A straight-down 1.5-length ray is axis-aligned; a direct column lookup would replace the DDA. Cache last hit keyed by block coordinate when the camera has not changed block.
- SLog2 (App.cpp:1269-1274,1278,1282) is a runtime `if (debugLevel >= 2)` (GlobalVariables.h:18): one load+compare per step, minor.
- Gravity ray runs about 2-3 steps per call.

### F6 Collision checks per frame (Low, High confidence)
- The only collision is the single downward ray in OnUpdate (App.cpp:1103-1109) and floor clamp (1112-1115): about 2-3 map lookups per frame. No horizontal collision, no broadphase. Gap is coverage, not perf.
- `MoveCameraBasedOnVelocity` (Camera.cpp:21-23) adds velocity without dt scaling while gravity is dt-scaled (App.cpp:1108): frame-rate dependent (correctness).
- `std::map<MoveStates,bool>` (Camera.h:56): `GetMoveState` uses non-const `operator[]` (Camera.cpp:7-9), 6 tree lookups per frame (App.cpp:120-143). Replace with bitmask / std::array<bool,6>.

### F7 Pass-by-value (Low impact)
- Vector is 12 B trivially copyable; by-value is fine: `operator*(Vector)` (Vector.h:64), `AddForceThisTick(Vector...)`, `RaycastRay(Vector,Vector,float)` (App.cpp:1243), `CheckIsPointInsideAny(Vector)`, `PositionInBounds(Vector)` (Chunk.h:89). No large-object pass-by-value in the math headers.
- Large copies elsewhere (render lambda): `chunkLODBlocks[idx] = chunk.cache.lodBlocks` deep copy at App.cpp:1396 (cache hit, every frame) and 1554; `blockPresence = chunk.cache.cachedBlockPresence` array copy (App.cpp:1432); `worldChunks.push_back(chunk)` copies whole Chunk (App.cpp:1214, generation-time); lambda `isFaceInCameraFrustrum` copies 3 Vectors per plane (App.cpp:1342-1344; lambda appears unused in the code I read).

### F8 Inlining (header vs cpp) (Low-Med impact, High confidence)
- Inline in header: Magnitude/Normalized/Negated/Cross/Dot (Vector.h:99-126), operators, Matrix3D/4D, Camera view/projection getters.
- Out-of-line (.cpp, no inlining without LTO/unity): `Vector::toInt3`, `Vector::floored` (Vector.cpp:6-12), `Int3::toVector` (Int3.cpp:4-6), and all of Camera.cpp (SetMoveState/GetMoveState/AddForceThisTick/MoveCameraBasedOnVelocity, Camera.cpp:3-23; IsPointInFrustum, Camera.cpp:104-114). Trivial ones should be inline. LTO status not checked (see build-config gatherer).
- Header bloat: Matrix4D.h pulls <string> and GlobalVariables.h for `toString` (Matrix4D.h:4-5,79-84); Int3.h pulls <iostream>, <iomanip>, <unordered_set>, <expected> (Int3.h:5-12). Compile-time only.
- Matrix3D.h is not used in App.cpp hot paths (dead code likely).

### F9 Int3 hash quality (Medium impact on chunkMap lookups, Medium confidence)
- Int3.h:84 `hashA ^ (hashB*420) ^ (hashC*67)`; std::hash<int> is identity on common stdlibs. Chunk keys are multiples of 16, so low bits are zero and XOR-combine collides easily (e.g. same-magnitude permutations). chunkMap.find runs per DDA step (App.cpp:1223) and elsewhere. Chunk counts not measured. `Int3::operator<` unused for the map (TODO Int3.h:14).

### F10 Render-lambda cache issues seen in passing (for chunk-caching gatherer)
- App.cpp:1390-1391 allocates and `reserve`s `chunkLocalBlocks` for chunkSizeXYZ^3 LODDBlocks BEFORE the cache-hit early return at 1394-1399: a huge per-visible-chunk-per-frame allocation that is discarded on cache hit (estimate up to about 1.7 MB per chunk if 16^3 x 432 B; unverified).
- Cache hit path still deep-copies `lodBlocks` (App.cpp:1396) every frame instead of referencing/pointing.
- App.cpp:1407 `chunk.isEmpty()` (Chunk.h:72-75) linear scan of all blocks for uncached chunks; empty chunks return before caching (1407) so are rescanned each frame. Chunk.h:77-81 has an isEmpty(LOD) overload unused here.
- App.cpp:1418-1433 comment says "rebuild only when cache invalid", but `if (chunk.cache.isValidCache)` scans blocks when cache IS valid and copies `cachedBlockPresence` when not valid: appears inverted (needs runtime confirmation). Also App.cpp:1467 and 1473 reuse `x != 0` / `x != chunkSize-1` guards for z-neighbors (copy-paste bug).
- LOD is hardcoded 0 (App.cpp:1388) so LOD cache key is effectively constant.
- Per-chunk frustum test each frame (App.cpp:1363-1380) is not skipped when camera is stationary.

## Prioritized gaps (math/raycast scope)
| # | Gap | Impact | Effort | Confidence |
|---|-----|--------|--------|-----------|
| 1 | Large reserve before cache-hit return + deep copy of cached lodBlocks per frame (App.cpp:1391,1396,1554) | High | Low | High (size estimate Med) |
| 2 | Frustum/view/proj rebuilt each frame with no dirty flag (App.cpp:1318-1322) | Low-Med | Low | High |
| 3 | DDA per-step full chunk hash lookup, no chunk reuse; weak Int3 hash (App.cpp:1223, Int3.h:84) | Med | Low-Med | Med |
| 4 | Gravity ray every frame + unused ray on Space (App.cpp:597,1103); redundant normalizes (1103-1105,1244-1245) | Low-Med | Low | High |
| 5 | UpdateDirectionVectors run per held key per frame; needless normalizes/double trig (Camera.cpp:117-124) | Low | Low | High |
| 6 | std::map move states; trivial Camera/Vector functions out-of-line | Low | Low | High |
