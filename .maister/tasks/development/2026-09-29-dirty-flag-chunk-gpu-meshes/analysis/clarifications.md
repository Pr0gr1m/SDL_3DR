# Clarifications (Phase 1)

## TL;DR
- Fix the worldgen z-position / duplicate-key bug in this task.
- Unloaded neighbors are treated as SOLID (user overrode the air default): faces on the outer world boundary are not emitted.
- Benchmark camera path is toggled by a key; profiler output is an SDL_Log report; no new test framework.

## Key Decisions
- Worldgen bug (App.cpp:1192 `atPos.y` as z, duplicate chunkMap keys) is in scope — needed to verify cross-chunk culling.
- Missing neighbor = solid — user choice; consequence: outer-boundary faces disappear, so the world's outer sides are open shells. Spec must state this and the visual effect.
- Benchmark: key toggle, frame-counter-driven fixed camera path.
- Profiler: unconditional SDL_Log periodic avg/p95/max; no test target (project has no test framework).
- UVs (default assumed, not asked): computed from world position so texturing matches baseline.

## Open Questions / Risks
- "Missing as solid" makes edge faces invisible from outside; camera looking at world sides will see through the terrain edge.
