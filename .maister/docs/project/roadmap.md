# Development Roadmap

## Current State
- **Version**: unversioned prototype
- **Key Features**: chunked voxel world (16³), simplex-noise terrain, face culling, frustum culling, per-chunk dirty-flag GPU meshes, in-app frame profiler and `B` benchmark, texture manager, first-person camera, DDA raycasting
- **Recent Updates**: per-chunk dirty-flag GPU meshes replaced the per-frame mesh rebuild; the chunk mesh cache and LOD path were removed; runtime verification of this change is still pending (September 2026)

## Planned Enhancements (Next 3-6 Months)

### High Priority
- [x] **Profiling instrumentation** — in-app per-stage timings and counters (`FrameProfiler`, `B` benchmark); baseline and final benchmark runs are still pending
- [ ] **Runtime verification of dirty-flag meshes** — hold-pose comparison, seam check and block-removal checks listed in the task work-log
- [ ] **Further rendering performance work** — culling and meshing improvements guided by profiler data
- [ ] **README and build instructions** — the README is currently 2 lines

### Medium Priority
- [ ] **Unit tests** for Vector/Matrix math, chunk indexing and raycasting
- [ ] **Error handling** around SDL and shader-loading calls
- [ ] **Configuration** — replace magic numbers in `GlobalVariables.h` with a config

### Technical Debt
- [ ] **Remove committed build logs** and add them to `.gitignore`
- [ ] **Rename the CMake project** from "SDL1" to match SDL_3DR
- [ ] **Clean up commented-out code** and unify naming conventions

## Future Considerations
- **Feature Ideas**: lighting, block editing, more block types
- **Scalability**: larger view distances, threaded chunk generation/meshing
