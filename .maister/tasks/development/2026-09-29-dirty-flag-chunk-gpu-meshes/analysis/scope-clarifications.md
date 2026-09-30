# Scope Clarifications (Phase 2)

## TL;DR
- Unloaded neighbors: solid on X/Z, air on +Y/-Y. Dirty writes go through a new setBlock/removeBlock API.
- Lazy chunk creation kept (dirty new chunk + neighbors); PositionInBounds made exclusive; remesh single-threaded.
- Benchmark logs conditions only; cleanup limited to replaced code + dead lambdas App.cpp:1334-1353.

## Key Decisions
- Vertical boundary: solid X/Z only, air ±Y — literal all-6-sides solid would hide terrain tops/bottoms.
- Dirty bypass: setBlock/removeBlock mark dirty (incl. boundary neighbors); E-key routed through them; mesher uses const reads.
- Chunk creation on edit: keep lazy creation, dirty new chunk and neighbors.
- Benchmark: log present mode/build info, force nothing.
- PositionInBounds: make exclusive.
- Remesh: single-threaded, in-frame, before render pass.
- Cleanup: replaced code + dead lambdas only.
