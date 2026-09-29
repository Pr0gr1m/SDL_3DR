# Requirements

## TL;DR
- Implement the researched design (research-context/high-level-design.md, ADR-001..005) with the scope decisions below.
- Developer-facing only: benchmark key + SDL_Log profiler report; gameplay unchanged; success = identical baseline image, no border holes, zero remesh/upload on clean frames.

## Key Decisions
- Architecture per research: dirty-flag per-chunk GPU meshes; chunk-level frustum cull; neighbor-aware culling; keep optional<Object>.
- Boundary: unloaded neighbors solid on X/Z, air on ±Y. Missing-as-solid was the user's choice; ±Y exception approved after critical gap.
- setBlock/removeBlock API marks dirty (self + boundary neighbors); E-key routed through it; mesher uses const reads.
- Single-threaded in-frame remesh; lazy chunk creation kept; PositionInBounds exclusive; worldgen z/duplicate-key bug fixed.
- UVs from world position; chunk-local vertices with per-chunk offset in vertex uniform ({mat4; vec4}, num_uniform_buffers stays 1); vertex.spv rebuilt.
- Profiler: SDL_GetPerformanceCounter stage timers, avg/p95/max via unconditional SDL_Log, logs present mode/build info; benchmark camera path toggled by key, frame-counter driven, bypasses gravity/input.
- No new tests/framework, no CMake/build-config changes, cleanup limited to replaced code + dead lambdas App.cpp:1334-1353.

## Open Questions / Risks
- Debug-build timings are relative-only.
- Edge faces on X/Z world boundary are not emitted (solid policy): world sides look open from outside.

## Initial description
Implement the dirty-flag per-chunk GPU mesh design from research task 2026-09-29-performance-gaps-caching.

## Q&A
- Worldgen bug: fix in this task. Unloaded neighbor: solid; refined to solid X/Z, air ±Y (critical gap).
- Benchmark: key toggle. Profiler: SDL_Log, no test target.
- Dirty API: setBlock/removeBlock. Chunk create on edit: lazy, dirty neighbors. Benchmark conditions: log, don't force. Bounds: exclusive. Threading: single-threaded. Cleanup: minimal.
- User journey: developer launches app, presses key for benchmark, reads log; gameplay unchanged (confirmed).
- Reuse: 12-face winding/vertex layout (App.cpp:1515-1533), Face.cpp normal/tangent/UV, existing raycast/chunk-lookup helpers, Quit release and early-return-submit patterns; Chunk lowerCamel / App PascalCase naming (confirmed, no other references).
- Visual assets: none; compare against baseline screenshot taken before changes.

## Functional requirements summary
1. FrameProfiler + benchmark camera path. 2. meshDirty replaces ChunkMeshCache/LOD path. 3. Per-chunk GPU buffers (outside Chunk), pooled transfer buffer, regrow. 4. Chunk-local vertices + chunk offset uniform. 5. Neighbor-aware face culling. 6. setBlock/removeBlock dirty API. 7. Worldgen z fix + exclusive bounds. 8. Chunk AABB -0.5 offset for culling. 9. FPS bookkeeping fix optional only if touched.

## Scope boundaries
Out: build presets/LTO, external profilers, greedy meshing, indexed vertices, indirect draw, texture atlas/mips, compact block ids, real LOD, occlusion culling, streaming, threaded gen, noise-offset fix, other unrelated bugs, on-screen stats.

## Technical considerations
See analysis/codebase-analysis.md and gap-analysis.md.
