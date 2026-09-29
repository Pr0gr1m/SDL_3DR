# Build Config and History Findings

## TL;DR
- No optimization config: CMakeLists.txt sets no CMAKE_BUILD_TYPE, no -O flags, no LTO/-march; CMakePresets.json has one "vcpkg" preset with no build type. Logged compile lines (build-output.txt, UTF-16) show only `-std=gnu++23`, i.e. unoptimized unless the IDE profile is Release.
- Only measured perf number in repo: "79->89 FPS for small scene" (commit 9274f71); GlobalVariables.h:15 says debugLevel logging costs about 7 FPS. No profiler/benchmark.
- ChunkMeshCache (per-chunk LOD block cache) added in 5ecf119; TextureManager caching is still a TODO (App.cpp:50).
- Shader SPIR-V build is incremental (DEPENDS-based) but outputs into src tree and is unoptimized.

## Key Decisions
- Build logs are UTF-16 CLion output from another machine (C:/Users/szymo/...); converted with iconv.

## Build flags (CMakeLists.txt, CMakePresets.json, vcpkg.json)
- CMakeLists.txt:1-4: cmake_minimum 3.20, C++23. No CMAKE_BUILD_TYPE default, no target_compile_options, no INTERPROCEDURAL_OPTIMIZATION, no -march (grep found none).
- CMakePresets.json: single configure preset "vcpkg" (Ninja, binaryDir build, cacheVariables only CMAKE_TOOLCHAIN_FILE). No Release/RelWithDebInfo presets; single-config Ninja without build type = no optimization flags.
- vcpkg.json: sdl3 (vulkan), sdl3-image (jpeg), sdl3-ttf. Nothing perf-related.
- Logs: build-output.txt / patched-build.txt / current-build.txt: CLion bundled MinGW G++ with `-std=gnu++23 -isystem .../vcpkg_installed/x64-windows/include`, no -O flag (build-output.txt [1/3],[2/3]). current-build.txt ends `FAILED: SDL1.exe` (link failure; MinGW compiler with x64-windows/MSVC-triplet vcpkg libs). reconfigure.txt shows vcpkg install with Visual Studio 18 compiler detection. app_stderr.txt: Vulkan chosen; shaders loaded from `...\build\../src/shaders/*.spv`.
- Confidence: High that repo never forces optimization; Medium that real runs were Debug (CLion default profile; no -O in logs).

## Shader build caching (CMakeLists.txt:42-66)
- add_custom_command OUTPUT vertex.spv/fragment.spv in src/shaders with DEPENDS on .glsl: incremental. glslangValidator `-V` with no optimization pass.
- POST_BUILD copy_if_different of .spv to `<exe>/shaders`; copy_directory of src/img to `<exe>/src/img` on every build. TODOs App.cpp:49 (built shaders into build dir) and App.cpp:170 (textures in build dir).
- No offline texture compression; jpg decoded via SDL3_image at startup.

## Logging cost
- src/GlobalVariables.h:15 `inline constexpr int debugLevel = 0; //switch to 0/-1 for Release, adds around 7 FPS for small scene`; SLog1/SLog2 (lines 17-18) are dead code at level 0.
- Remaining ungated SDL_Log in App.cpp: 642 ("Camera looking at", input path, frequency unverified), 725 (avg FPS at exit), 777 (quit). Timing logs at 1560/1564 and FPS log 1567/1569 use SLog1 (gated).

## Performance history (git log, 23 commits, Jul-Sep 2026)
| Commit | Date | Change |
|---|---|---|
| adfe668 | 07-17 | Texture manager added, frustum culling fixed |
| f36f85d | 07-27 | "Occlusion culling done" |
| 9976959 | 07-28 | Face culling instead of object culling (untested); Mesh.h +276 |
| 373fa52 | 07-28 | Face culling fix |
| 1de1dc7 | 08-28 | "Fixed and improved optimizations": Face.h/.cpp, Ray.h, Camera rework, Mesh.h -317 lines, CMake min 4.2 -> 3.20 |
| 5ecf119 | 09-26 | "Massively optimized": Chunk.blocks std::map -> std::array<optional<Object>,N^3> (comment "faster 10x-100x"); ChunkMeshCache {lodBlocks, cachedLOD, isValidCache} in Chunk.h; LOD cache hit path in App.cpp; per-thread vector reserve; SDL_Log lines commented out; average-FPS tracker |
| 9274f71 | 09-28 | "Optimized logging (79->89 FPS for small scene)": debugLevel + SLog1/SLog2, TextureManager.cpp:7 SDL_Log -> SLog1, LODDBLock.numFaces added, renames |
- FPS evidence: only 9274f71 message and .maister/docs/project/vision.md:17. Only ~7 FPS attributed (to logging); other optimizations have no numbers. "Small scene" undefined. No FPS in root logs.
- FPS measured as integer `1000 / deltaTimeMS` (App.cpp:1569,1579); avg at App.cpp:725 is `totalFPS / numFPS` integer division (fraction lost).

## Cache-related details from history (from diffs, not re-verified at HEAD)
- LODDBLock holds `std::array<Face,12>` per block (Chunk.h); numFaces byte added in 9274f71 but storage still fixed 12 slots: memory-heavy cache.
- Cache hit in 5ecf119: `chunkLODBlocks[idx] = chunk.cache.lodBlocks;` is a vector copy, not a reference; miss path moves into cache then copies again.
- Cache stores a single LOD (`cachedLOD` int): LOD changes evict/thrash. Invalidation on edit not checked here.
- Int3.h:14 TODO: operator< vs unordered_map for Int3 keys.

## TODO/FIXME across src
- App.cpp:49 shaders to build dir; :50 "Add caching to texture manager, maybe some CMake commands to recache"; :51 "Optimize with diff. cullings"; :52 screen space GI; :170 textures to build dir; Int3.h:14; fragment.glsl:34 ambient from AO.
- Docs: roadmap.md:12 "Further rendering performance work - meshing, culling, LOD tuning" unchecked; vision.md:17 lists "mesh caching, and profiling" as future; architecture.md:39 frame flow incl. ChunkCache. README.md/CLAUDE.md have no fps/cache mentions.

## Gaps (build/history scope)
1. No default Release/optimization, LTO, -march, or presets (impact High, effort Low, confidence High config / Medium runtime).
2. Toolchain mismatch (MinGW vs MSVC-triplet vcpkg) causing link failure (Medium/Low/Medium).
3. No measurement infrastructure: integer FPS, no frame-time stats or benchmark scene; numbers unreproducible.
4. Texture manager caching TODO unresolved; textures recopied every build.
5. Mesh cache copy-on-hit, 12-face fixed storage, single-LOD slot.
6. Shaders compiled without SPIR-V optimization, loaded via source-relative path.
7. Ungated SDL_Log at App.cpp:642.

## Open Questions / Risks
- Actual build type behind 79->89 FPS unknown (likely Debug).
- Logs come from another machine/user path; may not reflect current setup.
- Diff-derived claims (cache copy on hit) need confirmation against HEAD code by codebase gatherers.
