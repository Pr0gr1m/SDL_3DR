# Research Plan: Performance gaps in SDL_3DR (caching focus)

## TL;DR
- Technical, static-analysis-only research of a small codebase (~25 source files under src/).
- Parallel gathering by 6 subsystem-focused instances; each records file:line evidence and impact/effort/confidence per gap.
- Caching inventory first (ChunkCache.h, ChunkManager.h, TextureManager), then missing-cache and general hot-path gaps.

## Key Decisions
- 6 gatherers split by subsystem, not by source type: codebase is small, docs/config are thin (CLAUDE.md, README, CMakeLists.txt, .maister/docs/project).
- No shaders/ top-level dir: shaders are in src/shaders (glsl + prebuilt spv).
- No ChunkMeshCache file exists; mesh caching is inside ChunkCache.h / ChunkManager.h / Chunk.h / Mesh.h (verify).
- External literature limited to brief comparison (voxel meshing/caching practices); low priority.
- Root-level logs (build-output.txt, app_*.err, etc.) are evidence for build config only.

## Open Questions / Risks
- No profiler data; impact estimates are heuristic (confidence must be stated).
- Commit history not available (not a git repo); the FPS 79->89 hint may only come from docs/CLAUDE.md.

## Research Overview
Question: What performance gaps exist in SDL_3DR (C++23 voxel renderer on SDL3 GPU/Vulkan), especially caching-related?
Type: Technical. Scope: src/App.*, src/core/*, src/shaders, CMake flags. Exclude vcpkg/.

## Methodology
Primary: codebase reading and pattern search (Glob/Grep/Read). Fallback: build logs and docs for build/flag evidence. Framework: (1) cache inventory (key, invalidation, eviction, thread-safety); (2) missing/ineffective caches; (3) per-frame recomputation, allocation, GPU upload/resource lifecycle; (4) build/optimization config; (5) prioritize by impact/effort/confidence.

## Research Phases
1. Broad discovery: list files, grep for cache/map/unordered_map/vector/new/create/release/upload/transfer.
2. Targeted reading: full read of each subsystem's files.
3. Deep dive: trace frame loop -> chunk load -> mesh build -> GPU upload -> draw; invalidation paths on block edit and camera move.
4. Verification: cross-check claims between gatherers; confirm line numbers; flag inconsistencies.

## Gathering Strategy

### Instances: 6

| # | Category ID | Focus Area | File globs | Output Prefix |
|---|-------------|-----------|-----------|---------------|
| 1 | chunk-mesh-caching | Existing caches: keying, invalidation, eviction, dirty flags, mesh rebuild triggers | src/core/ChunkCache.h, ChunkManager.h, Chunk.h, Mesh.h, Face.*, Vertex3D.h | chunk-mesh-caching |
| 2 | gpu-resources-and-frame-loop | GPU buffers/pipelines/textures lifecycle, uploads, per-frame allocs, draw calls, sync | src/App.*, src/main.cpp, src/core/TextureManager.*, Object.h, ShaderHelper.h, src/shaders/*.glsl | gpu-frame |
| 3 | culling-lod-visibility | Frustum/distance culling, LOD, visibility result reuse, camera-move recompute | src/core/Camera.*, ChunkManager.h, App.cpp (visibility parts) | culling-lod |
| 4 | world-gen-and-memory | Terrain/noise generation, chunk storage layout, allocation patterns, copies, threading | src/core/noise/*, Chunk.h, ChunkManager.h, Int3.*, GlobalVariables.h | worldgen-memory |
| 5 | math-raycast-hotpaths | Vector/Matrix/Ray hot paths, redundant matrix/normalize/trig, pass-by-value, non-inlined code | src/core/Vector.*, Matrix3D.h, Matrix4D.h, Ray.h, Int3.*, Camera.* | math-ray |
| 6 | build-config-and-history | CMake flags, optimization/LTO/presets, vcpkg config, build logs, docs/roadmap perf notes | CMakeLists.txt, CMakePresets.json, vcpkg.json, root *.txt/*.err/*.out logs, CLAUDE.md, README.md, .maister/docs/project/*.md | build-config |

Findings go to analysis/findings/[prefix]-*.md.

### Rationale
Subsystem split matches where caching decisions live and avoids overlap; small doc/config surface is folded into gatherer 6.

## Success Criteria
1. Every existing cache identified with key, invalidation, eviction.
2. Missing/ineffective caching opportunities listed with file:line evidence.
3. Other gaps (allocation, per-frame recompute, uploads, build config) listed and prioritized.
4. Each gap has impact/effort estimate and confidence.

## Expected Outputs
Synthesized report with prioritized gap table and recommendations (no code changes).
