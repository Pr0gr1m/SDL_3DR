# System Architecture

## Overview
A single-executable, monolithic real-time voxel renderer. An `App` class owns the SDL3 window and GPU device and coordinates subsystems each frame.

## Architecture Pattern
**Pattern**: Game-engine style application with manager subsystems.

`main.cpp` uses SDL3's callback model and delegates to `App`. `App` owns the render loop, the GPU pipeline and resources, and coordinates the chunk, texture and camera systems.

## System Structure

### Application
- **Location**: `src/`
- **Purpose**: entry point, main loop, GPU pipeline, frustum culling, raycasting, player physics, per-chunk GPU mesh store, frame profiler
- **Key Files**: `main.cpp`, `App.h/.cpp`, `GlobalVariables.h`, `ChunkMeshStore.h/.cpp`, `FrameProfiler.h/.cpp`

### World / Voxels
- **Location**: `src/core/`
- **Purpose**: chunk storage (`Chunk<N>`, N=16) with a per-chunk `meshDirty` flag, spatial index of chunks, chunk-local mesh building with face culling, block faces
- **Key Files**: `Chunk.h`, `ChunkManager.h`, `ChunkMesher.h/.cpp`, `Face.h/.cpp`, `Int3.h/.cpp`

### Rendering Support
- **Location**: `src/core/`, `src/shaders/`
- **Purpose**: meshes, vertices, textures, shader helpers, GLSL shaders
- **Key Files**: `Mesh.h`, `Object.h`, `Vertex3D.h`, `TextureManager.h/.cpp`, `ShaderHelper.h`, `shaders/vertex.glsl`, `shaders/fragment.glsl`

### Math and Camera
- **Location**: `src/core/`
- **Purpose**: vectors, 3x3/4x4 matrices, rays, first-person camera
- **Key Files**: `Vector.h/.cpp`, `Matrix3D.h`, `Matrix4D.h`, `Ray.h`, `Camera.h/.cpp`

### Terrain Generation
- **Location**: `src/core/noise/`
- **Purpose**: procedural terrain via simplex noise
- **Key Files**: `SimplexNoise.h/.cpp`

## Data Flow
Input events → `Camera` / player state → `App::Iterate` → remesh only chunks with `meshDirty` set (`buildChunkMesh`, hidden faces culled against neighbor chunks) → `ChunkMeshStore` uploads the changed chunk-local meshes to persistent per-chunk GPU vertex buffers (buffers grow by power-of-two capacity) → frustum-cull chunks → draw each chunk from its own buffer through the SDL GPU graphics pipeline (SPIR-V vertex/fragment shaders, view-projection and chunk-offset uniforms, block textures) → swapchain. Block removal marks the edited chunk (and a neighbor when the block lies on a border) dirty. Block interaction uses DDA raycasting against chunk data. `FrameProfiler` logs per-stage timings and counters over a frame window; the `B` key toggles a deterministic camera-orbit benchmark.

## External Integrations
SDL3, SDL3_image, SDL3_ttf (via vcpkg); Vulkan through the SDL GPU backend.

## Database Schema
None.

## Configuration
Compile-time constants in `src/GlobalVariables.h` (resolution, physics, camera sensitivity, debug level and logging macros). Shader paths are hardcoded.

## Deployment Architecture
Local desktop executable. CMake copies compiled shaders and texture assets next to the binary.

---
*Based on codebase analysis performed 2026-09-29*
