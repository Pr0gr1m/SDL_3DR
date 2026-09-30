# Documentation Index

**IMPORTANT**: Read this file at the beginning of any development task to understand available documentation and standards.

## Quick Reference

### Project Documentation
Project-level documentation covering vision, goals, architecture, and technology choices.

### Technical Standards
Coding standards, conventions, and best practices organized by domain.

---

## Project Documentation

Located in `.maister/docs/project/`

### Vision (`project/vision.md`)
Overview of SDL_3DR as a personal 3D voxel renderer (SDL3 GPU API on Vulkan), its current state (active prototype, single developer), purpose as a graphics-programming learning vehicle, 6-12 month goals, and evolution.

### Roadmap (`project/roadmap.md`)
Current feature state (16^3 chunked world, simplex-noise terrain, culling, LOD, texture manager, camera, DDA raycasting) and planned work: high/medium priority items (profiling instrumentation, rendering performance, README/build docs), technical debt, and future considerations.

### Tech Stack (`project/tech-stack.md`)
C++23 and GLSL (to SPIR-V), SDL3 and testing libraries, CMake/Ninja with vcpkg, development tooling (linting, formatting, docs, IDE), key dependencies, and version management, with rationale.

### Architecture (`project/architecture.md`)
Monolithic single-executable design: `App` owns the SDL3 window/GPU device and coordinates chunk, texture and camera subsystems; covers world/voxels, rendering support, math and camera, terrain generation, data flow, configuration, and deployment.

---

## Technical Standards

### Global Standards

Located in `.maister/docs/standards/global/`

#### Coding Style (`standards/global/coding-style.md`)
Naming consistency, automatic formatting, descriptive names, focused functions, uniform indentation, no dead code, no backward compatibility unless required, DRY.

#### Commenting (`standards/global/commenting.md`)
Let code speak through structure and naming, comment sparingly, no change-log style comments.

#### Conventions (`standards/global/conventions.md`)
Predictable structure, up-to-date documentation, clean version control, environment variables for config, minimal dependencies, consistent reviews, testing standards, feature flags, changelog updates.

#### Error Handling (`standards/global/error-handling.md`)
Clear user messages, fail fast, typed exceptions, centralized handling, graceful degradation, retry with backoff, resource cleanup.

#### Minimal Implementation (`standards/global/minimal-implementation.md`)
Build only what's needed, clear purpose for every method, delete exploration artifacts, no future stubs, no speculative abstractions, review before commit, unused code is debt.

#### Validation (`standards/global/validation.md`)
Server-side always, client-side for feedback, validate early, specific errors, allowlists over blocklists, type and format checks, input sanitization, business rules, consistent enforcement.

### Frontend Standards

*Not initialized for this project. If you need frontend standards, you can:*
- *Add them manually using the docs-manager skill*
- *Run `/maister:standards-discover --scope=frontend` to auto-discover*

### Backend Standards

*Not initialized for this project. If you need backend standards, you can:*
- *Add them manually using the docs-manager skill*
- *Run `/maister:standards-discover --scope=backend` to auto-discover*

### Testing Standards

Located in `.maister/docs/standards/testing/`

#### Test Writing (`standards/testing/test-writing.md`)
Test behavior not implementation, clear test names, mock external dependencies, fast execution, risk-based testing, balance coverage and velocity, critical path focus, appropriate depth.

---

## How to Use This Documentation

1. **Start Here**: Always read this INDEX.md first to understand what documentation exists
2. **Project Context**: Read relevant project documentation before starting work
3. **Standards**: This index only points to the standards — open and follow the specific standard files relevant to your task; don't rely on the index alone
4. **Keep Updated**: Update documentation when making significant changes
5. **Customize**: Adapt all documentation to your project's specific needs

## Updating Documentation

- Project documentation should be updated when goals, tech stack, or architecture changes
- Technical standards should be updated when team conventions evolve
- Always update INDEX.md when adding, removing, or significantly changing documentation
