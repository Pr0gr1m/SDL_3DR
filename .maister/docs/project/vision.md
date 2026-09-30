# Project Vision

## Overview
SDL_3DR is a personal 3D voxel renderer built with SDL3 (GPU API on Vulkan). It renders a chunked, procedurally generated block world with a first-person camera.

## Current State
- **Age**: ~3.5 months (first commit June 2026, 23 commits)
- **Status**: Active development, functional prototype
- **Users**: Single developer (personal project)
- **Tech Stack**: C++23, CMake/Ninja, vcpkg, SDL3, GLSL → SPIR-V

## Purpose
A hands-on vehicle for learning graphics programming: writing the renderer, math library, meshing and culling systems from scratch instead of using an engine.

## Goals (Next 6-12 Months)
- **Learn graphics programming**: deepen understanding of the GPU pipeline, shaders, and the voxel rendering techniques already in progress.
- **Performance optimization**: continue improving frame rate through culling, LOD, mesh caching, and profiling (earlier work took a scene from about 79 to 89 FPS).
- Improve supporting infrastructure: README/build docs, tests for math and chunk code, cleanup of committed build logs.

## Evolution
Development so far progressed from face culling → occlusion culling → LOD → texture management → DDA raycasting → optimization passes. The focus is on rendering; gameplay is not a current goal.
