# Technology Stack

## Overview
This document describes the technology choices and rationale for SDL_3DR, a personal 3D voxel renderer built on SDL3.

## Languages

### C++ (C++23)
- **Usage**: ~100% of application code (about 3,350 lines across 26 source files), plus GLSL shaders
- **Rationale**: Low-level control for real-time graphics; a learning vehicle for graphics programming
- **Key Features Used**: Templates (`Chunk<N>`), `constexpr`, `[[nodiscard]]`, `std::unique_ptr` with custom deleters

### GLSL
- **Usage**: Vertex and fragment shaders in `src/shaders/`
- **Rationale**: Compiled to SPIR-V with `glslangValidator` for the SDL3 GPU (Vulkan) backend

## Frameworks & Libraries

### Graphics / Platform
- **SDL3** (vcpkg, `vulkan` feature) — windowing, input, and the SDL GPU API. It uses the callback-based app model (`SDL_AppInit` / `SDL_AppIterate` / `SDL_AppEvent` / `SDL_AppQuit`).
- **SDL3_image** (`jpeg` feature) — block texture loading
- **SDL3_ttf** — text rendering

### Testing
None yet (0% coverage, no test framework).

## Database
None.

## Build Tools & Package Management
- **CMake** 3.20+ with **Ninja** generator (`CMakePresets.json`)
- **vcpkg** as a git submodule, integrated via its CMake toolchain file; dependencies in `vcpkg.json`
- **glslangValidator** for shader compilation (external tool, invoked from `CMakeLists.txt`)
- Compiler observed in build logs: MinGW/GCC on Windows

## Infrastructure
- **Containerization**: none
- **CI/CD**: none
- **Hosting**: none (local desktop application)

## Development Tools

### Linting & Formatting
None configured (no `.clang-format` or `.clang-tidy`).

### Documentation
Doxygen-style comments throughout `src/core/*.h`.

### IDE
JetBrains CLion (`.idea/`).

## Key Dependencies
SDL3, SDL3_image, SDL3_ttf (all via vcpkg); `glslangValidator`.

## Version Management
Dependency versions are resolved by the vcpkg submodule baseline (`vcpkg-configuration.json`).

---
*Last Updated*: 2026-09-29
*Auto-detected*: languages, build system, dependencies, tooling. *User-provided*: project purpose (personal learning and performance-focused renderer).
