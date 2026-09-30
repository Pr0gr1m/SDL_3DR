# Research Brief: Performance gaps, especially caching

## TL;DR
Technical research into SDL_3DR's performance gaps with emphasis on caching (chunk mesh cache, texture/GPU resource caching, per-frame recomputation). Static code analysis only; no profiler data is available.

## Research Question
What performance gaps exist in SDL_3DR (C++23 voxel renderer on SDL3 GPU/Vulkan), especially related to caching?

## Type
Technical (codebase analysis), with light literature comparison for known voxel-renderer caching practices.

## Scope
- **Included**: `src/App.h/.cpp`, `src/core/*` (ChunkManager, ChunkCache/ChunkMeshCache, Chunk, Mesh, Object, TextureManager, Face, Ray, Camera, noise), shaders, CMake build flags; per-frame work, GPU buffer/pipeline/texture resource lifecycle, mesh generation and invalidation, LOD/culling result reuse, raycast and math recomputation, memory allocation patterns, build/optimization settings.
- **Excluded**: the vcpkg submodule, feature work, actual code changes, runtime profiling (none available).
- **Constraints**: read-only static analysis; committed build logs (`build-output.txt` etc.) may be used as evidence; FPS figures from commit history (79→89) are hints only.

## Success Criteria
1. Every existing cache is identified with its keying, invalidation and eviction behavior.
2. Missing or ineffective caching opportunities are listed with file/line evidence.
3. Other notable performance gaps (allocation, per-frame recompute, GPU upload, build config) are listed and prioritized.
4. Each gap has an impact/effort estimate and a confidence level.
