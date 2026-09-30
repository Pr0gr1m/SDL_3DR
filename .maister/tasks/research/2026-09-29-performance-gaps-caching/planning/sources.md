# Research Sources

## TL;DR
All sources are local; verified to exist. No shaders/ at root (use src/shaders). External sources optional.

## Key Decisions
- Exclude vcpkg/. Root logs used only for build evidence.

## Codebase Sources
Root: C:\Users\artur\code\sdl_3dr
- src/App.cpp, src/App.h, src/main.cpp, src/GlobalVariables.h
- src/core/: Camera.{h,cpp}, Chunk.h, ChunkCache.h, ChunkManager.h, Face.{h,cpp}, Int3.{h,cpp}, Matrix3D.h, Matrix4D.h, Mesh.h, Object.h, Ray.h, ShaderHelper.h, TextureManager.{h,cpp}, Vector.{h,cpp}, Vertex3D.h
- src/core/noise/SimplexNoise.{h,cpp}
- src/shaders/vertex.glsl, fragment.glsl (vertex.spv, fragment.spv binary; skip)
- src/img/ (texture assets; list only)

## Configuration Sources
- CMakeLists.txt, CMakePresets.json, vcpkg.json, vcpkg-configuration.json
- Logs: build-output.txt, build-last.txt, current-build.txt, patched-build.txt, reconfigure.txt, app-errors.txt, main-errors.txt, *_compile.err/out

## Documentation Sources
- CLAUDE.md, README.md
- .maister/docs/INDEX.md, .maister/docs/project/{vision,roadmap,tech-stack,architecture}.md, .maister/docs/standards/
- Inline code comments in src/

## External Sources (optional, low priority)
- SDL3 GPU API docs (transfer buffers, cycling, pipeline creation)
- Voxel meshing/caching practices (greedy meshing, chunk dirty flags)
