# Pragmatic Review (post-fix): dirty-flag chunk GPU meshes

## TL;DR
Status: appropriate. Diff vs HEAD is 323 added / 604 removed lines, plus about 440 lines in the new units (`ChunkMeshStore` 190, `FrameProfiler` 171, `ChunkMesher` 77). The fix loop cleared the earlier dead code and commented-out blocks in `App.cpp`. What remains is optional trimming. Findings: 0 Critical, 0 High, 1 Medium (residual), 3 Low.

## Key Decisions
- `setBlock` (`ChunkManager.h:87`) has no caller. It is user-mandated, so it stays. About 15 lines, shares helpers with `removeBlock`.
- Pow2 growth (`bit_ceil`) and the pooled transfer buffer are acceptable now. Reasons: the fix loop added overflow guards that make the sizing code correct, and the mesher was reworked to emit only exposed faces. The remaining cost is about 6 lines for `CapacityBytesFor` and about 25 for the pool. Rewriting would be churn for no user-visible gain. Keep, and revisit only if the code is touched again.
- `FrameProfiler` (171 lines) is acceptable as a deliberate roadmap item (profiling instrumentation), since it exists to measure the FPS work. It is the largest optional piece. It is not a blocker, and trimming is a future option, not a request.
- The dirty-flag design (`meshDirty`, boundary neighbor invalidation, one buffer per chunk) is proportionate.

## Earlier findings: status
| # | Earlier finding | Status |
|---|---|---|
| 1 | Commented-out blocks in `E` handler and `ConstructChunkAtLine` | Resolved. `didUserEditChunk` no longer appears anywhere in `src`. Other commented-out code remains in `App.cpp`, but it was not part of that finding (see L1). |
| 2 | `FrameProfiler` size (p95, Frame stage, sample vector) | Residual, accepted. Deliberately kept, still 171 lines with the per-frame sample vector. |
| 3 | `bit_ceil` pow2 capacity and pooled transfer buffer | Residual, accepted. Kept deliberately, and now has 2^31 overflow guards (`FitsBufferLimit`). |
| 4 | Wide `ChunkManager` API; `chunkCoordinateFromBlockCoordinate` has a single caller | Residual, negligible. Its uses are all within `ChunkManager.h`. `setBlock` is mandated. |
| 5 | Dead `Chunk::isEmpty()` | Resolved. `isEmpty` and `numNonEmptyBlocks` were removed, with no references left. |
| 6 | Unnamed `ChunkManager &` parameter in `buildChunkMesh` header | Not re-verified in this pass. Low. |
| 7 | `(void) result;` and link comment in `Quit` | Resolved, per the work-log. |
| 8 | `kFreeModeProfilerWindowFrames` public | Resolved. Now private. |
| 9 | `vertex.spv` regenerated with `vertex.glsl` | Residual process note. Regenerate and commit both together. |

## Findings

### Medium
1. **Residual: `FrameProfiler` plus benchmark orbit mode remain the largest optional surface.** In `App.cpp` this shows up as the `std::optional<StageScope>` chaining, `ToggleBenchmark`, `ApplyBenchmarkPose` and `LogProfilerHeader`. Accepted under the roadmap, but it is the first thing to cut if the project needs to shrink. Optional trims: drop p95 and the `Frame` stage, and use running sums instead of the `FrameSample` vector (about -40 to -60 LOC).

### Low
- L1. About 20 more commented-out lines remain in `App.cpp`: `//ConstructChunkAtLine(...)` at lines 581-583, commented `MoveCameraLocal` and `OnQuit` in the key handlers around lines 635-721, and a commented `OnQuit` stub at 831-833. Most of these look pre-existing. Delete when convenient, per `coding-style.md`.
- L2. `CapacityBytesFor` truncates to `Uint32`, and `Upload` casts offset to `Uint32`. This is safe only because of the 2^31 guard. That coupling is easy to miss; the constant's comment covers it.
- L3. `ChunkMeshStore` never releases a mesh when a chunk goes away. Fine today (no chunk removal exists). Add release when unloading is introduced.

## Complexity assessment
Project scale: personal prototype, 4 chunks, single developer. Overall complexity: Low to Medium and proportionate. The per-chunk persistent buffers replace a per-frame rebuild, so the change removes work and net deletes code. No speculative abstraction layers were found. The cache, LOD and parallel loops are gone.

## Developer experience
Positive: one linear upload path (a single copy pass for all updates); failure paths log and return false. Negative: the profiler stage scoping in `OnRender` is somewhat indirect.

## Requirements alignment
The dirty-flag mesh requirement is met. Extras (profiler, benchmark mode, pow2 growth, pooled transfer buffer) are judged acceptable, per Key Decisions.

## Context consistency
Single mesh path (`buildChunkMesh` to `ChunkMeshStore`). No unused members were found for `isEmpty`, `numNonEmptyBlocks` or `didUserEditChunk`. `PositionInBounds` is still used by `ConstructChunkAtLine`.

## Top simplifications (all optional)
1. Trim `FrameProfiler` (-40 to -60 LOC).
2. Delete the remaining commented-out code in `App.cpp` (about -20 LOC).
3. Drop the pooled transfer buffer only if the file is being touched anyway (about -25 LOC).

## Open Questions / Risks
- Should benchmark mode and the profiler stay as permanent tooling? If yes, finding 1 is moot.
- `setBlock` is unexercised at runtime. It uses `worldChunks.emplace_back`, which invalidates any held `Chunk*`. Document this when a caller is added.
- Runtime verification of the upload path (transfer buffer reuse across frames with `cycle=true`) is still pending, and there are no automated tests for rendering.
