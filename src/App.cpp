#include "App.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include <SDL3/SDL.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
#include <limits>
#include <list>
#include <mutex>
#include <optional>
#include <ranges>
#include <thread>
#include <utility>

#include "core/noise/SimplexNoise.h"
#include "core/Vector.h"
#include "core/Camera.h"
#include "core/Chunk.h"
#include "core/ChunkMesher.h"
#include "core/Matrix4D.h"
#include "core/Vertex3D.h"

//POSITION IS FROM -1 TO 1
//COLOR FROM 0 to 1
//N. COORDS FROM 0 TO 1

//To conpile shaders use glslc commands:
//  glslc -fshader-stage=vertex src/shaders/vertex.glsl -o src/shaders/vertex.spv
//  glslc -fshader-stage=fragment src/shaders/fragment.glsl -o src/shaders/fragment.spv
//  glslc -fshader-stage=fragment src/shaders/linefragment.glsl -o src/shaders/lifragment.spv

static SimplexNoise *noise;

Vector startingCameraPos = Vector(0.f, 50.f, 0.f);
Vector degreesCameraEulerAngle = Vector(0.f, 0.f, 0.f);

inline constexpr Uint32 BytesPerPixel = 4; //8 bits from red, green, blue, alpha channels = 32 bits = 4 bytes
inline constexpr float cameraMinYPosition = -5;
inline constexpr float epsilon = +0.0001f;
inline constexpr int groundZeroYLevel = -2; //only 1 chunk for test

//TODO: Before full release, change CMakeList.txt to put built shaders in build dir
//TODO: Add caching to texture manager, maybe some CMake commands to recache
//TODO: Optimize with diff. cullings
//TODO: Add screen space GI?

// namespace {
void App::BuildCameraFromState() {
    sceneCamera = std::make_unique<Camera>(startingCameraPos, degreesCameraEulerAngle.x, degreesCameraEulerAngle.y, degreesCameraEulerAngle.z, defaultAspectRatio);
    cameraPositionSnapshot = sceneCamera->Position;
}

Vector HorizontalDirection(Vector direction) {
    direction.y = 0.f;
    return direction.Normalized();
}

inline constexpr Uint64 kBenchmarkHoldFrames = 300;
inline constexpr Uint64 kBenchmarkRevolutionFrames = 600;
inline constexpr Uint64 kBenchmarkLoopFrames = kBenchmarkHoldFrames + kBenchmarkRevolutionFrames;
inline constexpr float kBenchmarkOrbitRadius = 28.f;
inline constexpr float kBenchmarkCameraHeight = 30.f;

struct BenchmarkPose {
    Vector position;
    Vector lookDegrees;
};

//Orbit around the middle of the four starting chunks; holds still for the first frames of every loop
BenchmarkPose BenchmarkPoseForFrame(const Uint64 frame) {
    const Vector orbitCentre(15.5f, 3.f, 15.5f);
    const Uint64 loopFrame = frame % kBenchmarkLoopFrames;
    const float angle = loopFrame < kBenchmarkHoldFrames
                            ? 0.f
                            : 2.f * static_cast<float>(M_PI) * static_cast<float>(loopFrame - kBenchmarkHoldFrames) / static_cast<float>(kBenchmarkRevolutionFrames);

    const Vector position(orbitCentre.x + kBenchmarkOrbitRadius * std::sin(angle), kBenchmarkCameraHeight, orbitCentre.z + kBenchmarkOrbitRadius * std::cos(angle));
    const Vector direction = (orbitCentre - position).Normalized();

    //Inverse of the forward vector in Camera::UpdateDirectionVectors
    const float pitchDegrees = std::asin(direction.y) / DEG_2_RAD;
    const float yawDegrees = std::atan2(direction.x, -direction.z) / DEG_2_RAD;
    return {position, Vector(pitchDegrees, yawDegrees, 0.f)};
}

//Must match the Uniforms block in vertex.glsl (std140: mat4 followed by vec4)
struct ChunkUniforms {
    float viewProjection[16];
    float chunkOffset[4];
};

static_assert(sizeof(ChunkUniforms) == 80, "ChunkUniforms must match the vertex shader uniform block");

//Blocks are centred on integer coordinates, so a chunk spans [atPosition - 0.5, atPosition + chunkSize - 0.5] on every axis
bool IsChunkInFrustum(const Camera &camera, const Int3 &chunkLookup) {
    const float minCorner[3] = {
        static_cast<float>(chunkLookup.a) - kBlockHalfExtent,
        static_cast<float>(chunkLookup.b) - kBlockHalfExtent,
        static_cast<float>(chunkLookup.c) - kBlockHalfExtent
    };
    constexpr float chunkExtent = static_cast<float>(ChunkManager::chunkSizeXYZ);

    for (const auto &plane: camera.frustrumPlanes) {
        //Corner furthest along the plane normal, if even that one is behind the plane the box is outside
        const float x = plane.A >= 0.f ? minCorner[0] + chunkExtent : minCorner[0];
        const float y = plane.B >= 0.f ? minCorner[1] + chunkExtent : minCorner[1];
        const float z = plane.C >= 0.f ? minCorner[2] + chunkExtent : minCorner[2];
        if (plane.A * x + plane.B * y + plane.C * z + plane.D < 0.f) return false;
    }

    return true;
}

#ifdef __VERSION__
inline constexpr const char *kCompilerVersion = __VERSION__;
#else
inline constexpr const char *kCompilerVersion = "unknown";
#endif

#ifdef NDEBUG
inline constexpr const char *kNdebugState = __DATE__; //"defined";
#else
inline constexpr const char *kNdebugState = "not defined";
#endif

int BlockCoordinateFromPoint(const float point) {
    return static_cast<int>(std::floor(point + kBlockHalfExtent));
}

Vector BlockPositionFromPoint(const Vector &point) {
    return Vector(
        static_cast<float>(BlockCoordinateFromPoint(point.x)),
        static_cast<float>(BlockCoordinateFromPoint(point.y)),
        static_cast<float>(BlockCoordinateFromPoint(point.z))
    );
}

void App::MoveCameraHorizontal(const Vector &localDirection, const float distance) {
    sceneCamera->UpdateDirectionVectors();
    const Vector worldOffset =
            (HorizontalDirection(sceneCamera->right) * localDirection.x) +
            (HorizontalDirection(sceneCamera->forward) * localDirection.z);

    if (worldOffset.Magnitude() > 0.f) {
        sceneCamera->Position += worldOffset.Normalized() * distance;
    }
}

void App::MoveCameraWithForce(const Vector &forceDir, const float mag) {
    sceneCamera->AddForceThisTick(forceDir, mag);
}

void App::RotateCameraLocal(const Vector degreesCameraEulerAngle) {
    sceneCamera->pitch = degreesCameraEulerAngle.x;
    sceneCamera->yaw = degreesCameraEulerAngle.y;
    sceneCamera->roll = degreesCameraEulerAngle.z;

    sceneCamera->UpdateDirectionVectors();
}

void App::MoveCameraBasedOnStates() {
    if (sceneCamera->GetMoveState(Camera::Forward)) {
        MoveCameraHorizontal(Vector(0.f, 0.f, 1.f), kMoveSpeed * (float) sceneCamera->deltaTimeMS / 1000.f);
    }

    if (sceneCamera->GetMoveState(Camera::Backward)) {
        MoveCameraHorizontal(Vector(0.f, 0.f, -1.f), kMoveSpeed * (float) sceneCamera->deltaTimeMS / 1000.f);
    }

    if (sceneCamera->GetMoveState(Camera::Left)) {
        MoveCameraHorizontal(Vector(-1.f, 0.f, 0.f), kMoveSpeed * (float) sceneCamera->deltaTimeMS / 1000.f);
    }

    if (sceneCamera->GetMoveState(Camera::Right)) {
        MoveCameraHorizontal(Vector(1.f, 0.f, 0.f), kMoveSpeed * (float) sceneCamera->deltaTimeMS / 1000.f);
    }

    if (sceneCamera->GetMoveState(Camera::Up)) {
        MoveCameraWithForce(App::GetGravityVector().Normalized().Negated(), kJumpForceMagnitude);
    }

    if (sceneCamera->GetMoveState(Camera::Down)) {
        MoveCameraWithForce(App::GetGravityVector().Normalized(), kJumpForceMagnitude);
    }
}

// }

App::App(int argc, char **argv) : m_Window(nullptr, &SDL_DestroyWindow), m_gpuDevice(nullptr, &SDL_DestroyGPUDevice) {
}

SDL_AppResult App::Init() {
    SDL_SetAppMetadata("2DRenderer", "1.0.0", "com.cozyprogramming.renderer2d");

    SLog1("Initializing SDL library %llu ms...", SDL_GetTicks());
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not initialize SDL library: %s", SDL_GetError());
        return FAILURE;
    }

    SLog1("Initializing SDL Window %llu ms...", SDL_GetTicks());
    m_Window.reset(SDL_CreateWindow("SDL1", defaultScreenWidth, defaultScreenHeight, SDL_WINDOW_HIDDEN));

    if (m_Window == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not initialize SDL window: %s", SDL_GetError());
        return FAILURE;
    }

    SDL_SetWindowRelativeMouseMode(m_Window.get(), true); //Fullscreen mode

    //Get base path to source directionary
    //TODO: Add every texture to the build directory and fix this
    char *buildBasePath = const_cast<char *>(SDL_GetBasePath());
    std::string basePathStr(buildBasePath);
    SDL_free(buildBasePath);

    // Remove "build\" or "build/" from the end if it exists
    if (basePathStr.ends_with("build\\") || basePathStr.ends_with("build/")) {
        basePathStr.erase(basePathStr.length() - 6);
    } else if (basePathStr.ends_with("build")) {
        basePathStr.erase(basePathStr.length() - 5);
    }

    basePathStr += "src\\";

    this->basePath = static_cast<char *>(SDL_malloc(basePathStr.length() + 1));
    SDL_strlcpy(this->basePath, basePathStr.c_str(), basePathStr.length() + 1);

    SLog2("Base path: %s", this->basePath);

    //Choose driver based on preferred drivers
    const std::array<std::string, 2> preferredDrives
    {
        std::string{"vulkan"},
        std::string{"direct3d12"},
    };

    auto numGPUDrivers = SDL_GetNumGPUDrivers();
    std::vector<std::string> gpuDrivers;
    gpuDrivers.reserve(numGPUDrivers);

    SLog1("Initializing SDL GPU device %llu ms...", SDL_GetTicks());
    SLog1("Supported GPU drivers: ");
    for (int i = 0; i < numGPUDrivers; i += 1) {
        SLog1("\tDetected driver: %s", SDL_GetGPUDriver(i));
        gpuDrivers.emplace_back(SDL_GetGPUDriver(i));
    }

    std::string selectedDriver;
    for (const auto &driver: preferredDrives) {
        if (std::ranges::find(gpuDrivers, driver) != gpuDrivers.end()) {
            SLog1("Using preffered driver: %s", driver.c_str());
            selectedDriver = driver;
            break;
        }
    }

    SLog2("Creating GPU driver device %llu ms...", SDL_GetTicks());
    m_gpuDevice.reset(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, false,
        selectedDriver.empty() ? nullptr : selectedDriver.c_str()));

    if (m_gpuDevice == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create GPU device: %s", SDL_GetError());
        return FAILURE;
    }

    SLog2("Selected GPU driver: %s", SDL_GetGPUDeviceDriver(m_gpuDevice.get()));
    SLog2("Claiming window for GPU device %llu ms...", SDL_GetTicks());
    if (!SDL_ClaimWindowForGPUDevice(m_gpuDevice.get(), m_Window.get())) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not claim SDL window to GPU device: %s", SDL_GetError());
        return FAILURE;
    }

    SDL_GPUPresentMode presentMode = SDL_GPU_PRESENTMODE_VSYNC; //VSYNC is supported everywhere, but if mailbox is supported use it as its faster
    presentModeName = "VSYNC";
    if (SDL_WindowSupportsGPUPresentMode(m_gpuDevice.get(), m_Window.get(), SDL_GPU_PRESENTMODE_MAILBOX)) {
        presentMode = SDL_GPU_PRESENTMODE_MAILBOX;
        presentModeName = "MAILBOX";
    }

    SLog2("Setting GPU swapchain parameters %llu ms...", SDL_GetTicks());
    if (!SDL_SetGPUSwapchainParameters(m_gpuDevice.get(), m_Window.get(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR, presentMode)) {
        SDL_LogWarn(APP_LOG_CATEGORY_GENERIC, "Failed to set swapchain present mode %s: %s", presentModeName, SDL_GetError());
        presentModeName = "unknown";
    }

    //Exit out of build directory
    std::string vPath = std::string(basePath) + kVertexShaderPath; //"/shaders/vertex.spv";
    std::string fPath = std::string(basePath) + kFragmentShaderPath; //"/shaders/fragment.spv";

    size_t vertexShaderCodeSize;
    void *vertexShaderCode = SDL_LoadFile(vPath.c_str(), &vertexShaderCodeSize);

    if (vertexShaderCodeSize == 0) {
        SDL_free(vertexShaderCode);
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to load vertex shader: %s", SDL_GetError());
        return FAILURE;
    }

    SLog2("Loaded vertex shader from %s, size: %zu", vPath.c_str(), vertexShaderCodeSize);
    SDL_GPUShaderCreateInfo vertexShaderInfo{
        .code_size = vertexShaderCodeSize,
        .code = static_cast<Uint8 *>(vertexShaderCode),
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 1,
        .props = 0
    };

    SDL_GPUShader *vertexShader = SDL_CreateGPUShader(m_gpuDevice.get(), &vertexShaderInfo);

    if (vertexShader == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create vertex shader: %s", SDL_GetError());
        return FAILURE;
    }

    SLog2("Vertex shader created: %p", vertexShader);
    SDL_free(vertexShaderCode);

    SLog2("Loading and creating fragment shaders %llu ms...", SDL_GetTicks());
    size_t fragmentShaderCodeSize;
    void *fragmentShaderCode = SDL_LoadFile(fPath.c_str(), &fragmentShaderCodeSize);
    if (fragmentShaderCodeSize == 0) {
        SDL_free(fragmentShaderCode);
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to load fragment shader: %s", SDL_GetError());
        return FAILURE;
    }

    SLog2("Loaded fragment shader from %s, size: %zu", fPath.c_str(), fragmentShaderCodeSize);

    SDL_GPUShaderCreateInfo fragmentShaderInfo{
        .code_size = fragmentShaderCodeSize,
        .code = static_cast<Uint8 *>(fragmentShaderCode),
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 2,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 0,
        .props = 0
    };

    size_t lineFragmentCodeSize;
    void *lineFragmentShaderCode = SDL_LoadFile(fPath.c_str(), &lineFragmentCodeSize);
    if (lineFragmentCodeSize == 0) {
        SDL_free(lineFragmentShaderCode);
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to load fragment shader: %s", SDL_GetError());
        return FAILURE;
    }

    SLog2("Loaded fragment shader from %s, size: %zu", fPath.c_str(), lineFragmentCodeSize);

    SDL_GPUShaderCreateInfo lineFragmentCreationInfo{
        .code_size = lineFragmentCodeSize,
        .code = static_cast<Uint8 *>(lineFragmentShaderCode),
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 2,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 0,
        .props = 0
    };

    SDL_GPUShader *fragmentShader = SDL_CreateGPUShader(m_gpuDevice.get(), &fragmentShaderInfo);
    SDL_GPUShader *lineFragmentShader = SDL_CreateGPUShader(m_gpuDevice.get(), &lineFragmentCreationInfo);

    if (!fragmentShader) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create fragment shader: %s", SDL_GetError());
        SDL_free(fragmentShaderCode);
        SDL_free(lineFragmentShaderCode);
        return FAILURE;
    }

    SLog2("Fragment shader created: %p", fragmentShader);

    if (!lineFragmentShader) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create line fragment shader: %s", SDL_GetError());
        SDL_free(fragmentShaderCode);
        SDL_free(lineFragmentShaderCode);
        return FAILURE;
    }

    SLog2("Line fragment shader created: %p", lineFragmentShader);

    SDL_free(fragmentShaderCode);
    SDL_free(lineFragmentShaderCode);

    SLog2("Creating GPU pipelines infos %llu ms...", SDL_GetTicks());

    //Create the graphics pipeline info for the TRIANGLE LIST pipeline

    //Describe the vertex buffers
    SDL_GPUVertexBufferDescription vertexBufferDesctiptions[1];
    vertexBufferDesctiptions[0].slot = 0;
    vertexBufferDesctiptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBufferDesctiptions[0].instance_step_rate = 0;
    vertexBufferDesctiptions[0].pitch = sizeof(Vertex3D);

    // describe the vertex attribute
    SDL_GPUVertexAttribute vertexAttributes[5];

    //Vertex3D layout is:
    //vec3, vec4, vec3, vec2, vec3

    // a_position
    vertexAttributes[0].buffer_slot = 0;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[0].offset = offsetof(Vertex3D, Position);

    // a_color
    vertexAttributes[1].buffer_slot = 0;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    vertexAttributes[1].offset = offsetof(Vertex3D, Color);

    // a_normal
    vertexAttributes[2].buffer_slot = 0;
    vertexAttributes[2].location = 2;
    vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[2].offset = offsetof(Vertex3D, Normal);

    // a_texcoord
    vertexAttributes[3].buffer_slot = 0;
    vertexAttributes[3].location = 3;
    vertexAttributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttributes[3].offset = offsetof(Vertex3D, TexCoordU);

    // a_tangent
    vertexAttributes[4].buffer_slot = 0;
    vertexAttributes[4].location = 4;
    vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[4].offset = offsetof(Vertex3D, Tangent);

    SDL_GPUColorTargetDescription colorTargetDescriptions{};
    colorTargetDescriptions.blend_state.enable_blend = true;
    colorTargetDescriptions.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    colorTargetDescriptions.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    colorTargetDescriptions.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorTargetDescriptions.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTargetDescriptions.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorTargetDescriptions.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTargetDescriptions.format = SDL_GetGPUSwapchainTextureFormat(m_gpuDevice.get(), m_Window.get());

    //Depth texture
    SDL_GPUTextureCreateInfo depthInfo = {};
    depthInfo.type = SDL_GPU_TEXTURETYPE_2D;
    depthInfo.width = defaultScreenWidth;
    depthInfo.height = defaultScreenHeight;
    depthInfo.layer_count_or_depth = 1;
    depthInfo.num_levels = 1;
    depthInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    depthInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depthInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    depthTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &depthInfo);
    if (!depthTexture) {
        //Fallback to 24‑bit depth
        depthInfo.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
        depthTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &depthInfo);
        if (!depthTexture) {
            SDL_LogError(APP_LOG_CATEGORY_VIDEO, "Couldn't generate depth texture.");
            return FAILURE;
        }
    }

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST
    };

    //Create the pipeline for LINE LIST
    SDL_GPUGraphicsPipelineCreateInfo pipelineLineInfo{
        .vertex_shader = vertexShader,
        .fragment_shader = lineFragmentShader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST
    };

    pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDesctiptions;
    pipelineInfo.vertex_input_state.num_vertex_attributes = sizeof(vertexAttributes) / sizeof(SDL_GPUVertexAttribute); //2: position, color
    pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

    //reuse vertex buffer attributes, descriptions and color target
    pipelineLineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineLineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDesctiptions;
    pipelineLineInfo.vertex_input_state.num_vertex_attributes = sizeof(vertexAttributes) / sizeof(SDL_GPUVertexAttribute);
    pipelineLineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

    pipelineInfo.target_info.num_color_targets = 1;
    pipelineInfo.target_info.color_target_descriptions = &colorTargetDescriptions;
    pipelineInfo.target_info.has_depth_stencil_target = true;
    pipelineInfo.target_info.depth_stencil_format = depthInfo.format;

    pipelineLineInfo.target_info.num_color_targets = 1;
    pipelineLineInfo.target_info.color_target_descriptions = &colorTargetDescriptions;
    pipelineLineInfo.target_info.has_depth_stencil_target = true;
    pipelineLineInfo.target_info.depth_stencil_format = depthInfo.format;

    //enable depth testing
    pipelineInfo.depth_stencil_state.enable_depth_test = true;
    pipelineInfo.depth_stencil_state.enable_depth_write = true;
    pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    pipelineInfo.depth_stencil_state.compare_mask = 0xFF;
    pipelineInfo.depth_stencil_state.write_mask = 0xFF;

    pipelineLineInfo.depth_stencil_state.enable_depth_test = true;
    pipelineLineInfo.depth_stencil_state.enable_depth_write = false;
    pipelineLineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    pipelineLineInfo.depth_stencil_state.compare_mask = 0xFF;
    pipelineLineInfo.depth_stencil_state.write_mask = 0xFF;

    //Culling modes (for now None)
    pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

    pipelineLineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipelineLineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

    SLog2("Creating GPU pipelines %llu ms...", SDL_GetTicks());
    graphicsPipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice.get(), &pipelineInfo);
    lineGraphicsPipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice.get(), &pipelineLineInfo);
    SLog2("Graphics pipelines created: %p", graphicsPipeline);

    if (!graphicsPipeline || !lineGraphicsPipeline) {
        SDL_LogError(APP_LOG_CATEGORY_VIDEO, "Pipeline creation failed");
        return FAILURE;
    }

    //we don't need to store the shaders after creating the pipeline
    SDL_ReleaseGPUShader(m_gpuDevice.get(), vertexShader);
    SDL_ReleaseGPUShader(m_gpuDevice.get(), fragmentShader);
    SDL_ReleaseGPUShader(m_gpuDevice.get(), lineFragmentShader);

    SDL_WaitForGPUIdle(m_gpuDevice.get());

    BuildCameraFromState();
    sceneCamera->UpdateCameraFrustrumCorners();

    this->cubeMesh = std::make_unique<Mesh>(cubeVerticies, std::size(cubeVerticies), cubeTriangles, std::size(cubeTriangles));

    SLog2("Creating managers...");
    this->chunkManager = std::make_unique<ChunkManager>();
    this->textureManager = std::make_unique<TextureManager>(this->basePath);

    //texture manager config - to be replaced with threaded loading system and caching
    this->textureManager->AddEntryForBlockType(TextureManager::Dirt, "img\\dirt\\");
    this->textureManager->AddEntryForBlockType(TextureManager::OakLog, "img\\oak_log\\");

    if (!UploadDirtTexturesToGPU()) {
        return FAILURE;
    }

    noise = new SimplexNoise(0.15f, 3, 0, 0);

    toBeConstructedChunkPositions.reserve(8);

    //Construct default chunk
    ConstructChunkAtLine(Vector(0, 0, 0));

    chunkConstructionThread = std::jthread([this](std::stop_token st) {
        int safetyLimit = 0;
        while (!st.stop_requested()) {
            if (safetyLimit >= 1000) break;
            safetyLimit += 1;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (st.stop_requested()) break;

            const Int3 chunkLookup = ChunkManager::chunkLookupFromBlockPosition(cameraPositionSnapshot);

            // std::scoped_lock lock(chunkQueueMutex);
            toBeConstructedChunkPositions.push_back(chunkLookup + Int3(-ChunkManager::chunkSizeXYZ, 0, 0));
            toBeConstructedChunkPositions.push_back(chunkLookup + Int3(ChunkManager::chunkSizeXYZ, 0, 0));
        }
    });

    SLog1("%zu chunks", chunkManager->worldChunks.size());

    chunkMeshStore = std::make_unique<ChunkMeshStore>(m_gpuDevice.get());

    frameProfiler.StartWindow(kFreeModeProfilerWindowFrames, "free");
    LogProfilerHeader();

    SDL_WaitForGPUIdle(m_gpuDevice.get()); //Block rendering and showing window until gpu is idle

    SLog1("Showing the window %llu ms...", SDL_GetTicks());

    //Creating gpu device in swap chain takes long time, people would see empty or trashed window, so we show window after some time
    if (!SDL_ShowWindow(m_Window.get())) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not show SDL window: %s", SDL_GetError());
        return FAILURE;
    }

    currentMillisecondsSinceStart = SDL_GetTicks();

    SLog1("All done %llu ms...", SDL_GetTicks());
    return CONTINUE;
}

SDL_AppResult App::Iterate() {
    frameProfiler.BeginFrame();

    if (sceneCamera != nullptr && !benchmarkActive) {
        MoveCameraBasedOnStates();
    }

    if (const auto result = OnUpdate(); result != SDL_APP_CONTINUE) {
        return result;
    }

    if (const auto result = OnRender(); result != SDL_APP_CONTINUE) {
        return result;
    }

    return CONTINUE;
}

SDL_AppResult App::Event(const SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SUCCESS;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (SDL_GetWindowID(m_Window.get()) == event->window.windowID) //For multiple windows
                //return OnQuit();
                return SUCCESS;
            return CONTINUE;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) {
                return SUCCESS;
            }
            if (event->key.key == SDLK_W) {
                //MoveCameraLocal(Vector(0.f, 0.f, 1.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Forward, true);
            }

            if (event->key.key == SDLK_S) {
                // MoveCameraLocal(Vector(0.f, 0.f, -1.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Backward, true);
            }

            if (event->key.key == SDLK_A) {
                // MoveCameraLocal(Vector(-1.f, 0.f, 0.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Left, true);
            }

            if (event->key.key == SDLK_D) {
                // MoveCameraLocal(Vector(1.f, 0.f, 0.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Right, true);
            }

            if (event->key.key == SDLK_SPACE) {
                //auto raycastHit = RaycastRay(sceneCamera->Position - Vector(0, 0.1f, 0), GetGravityVector().Normalized(), kCameraHeightAboveGround);
                //if (raycastHit.hit) {
                //    sceneCamera->SetMoveState(Camera::Up, true);
                //}
            }

            if (event->key.key == SDLK_F) {
                sceneCamera->UpdateCameraFrustrumCorners();
            }

            if (event->key.key == SDLK_B && !event->key.repeat) {
                ToggleBenchmark();
            }

            if (event->key.key == SDLK_E) {
                Vector cameraLookVector = sceneCamera->forward;

                SLog1("Camera looking at %f,%f,%f", cameraLookVector.x, cameraLookVector.y, cameraLookVector.z);
                auto result = RaycastRay(sceneCamera->Position, cameraLookVector.Normalized(), 5);
                if (result.hit) {
                    chunkManager->removeBlock(result.blockPosition);
                }
            }

            break;
        case SDL_EVENT_KEY_UP:
            if (event->key.key == SDLK_W && !event->key.down) {
                //MoveCameraLocal(Vector(0.f, 0.f, 1.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Forward, false);
            }

            if (event->key.key == SDLK_S && !event->key.down) {
                // MoveCameraLocal(Vector(0.f, 0.f, -1.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Backward, false);
            }

            if (event->key.key == SDLK_A && !event->key.down) {
                // MoveCameraLocal(Vector(-1.f, 0.f, 0.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Left, false);
            }

            if (event->key.key == SDLK_D && !event->key.down) {
                // MoveCameraLocal(Vector(1.f, 0.f, 0.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Right, false);
            }

            if (event->key.key == SDLK_SPACE && !event->key.down) {
                // MoveCameraLocal(Vector(0.f, .2f, 0.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Up, false);
            }

            if (event->key.key == SDLK_LSHIFT && !event->key.down) {
                // MoveCameraLocal(Vector(0.f, -.2f, 0.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Down, false);
            }
            break;

        // case SDL_EVENT_MOUSE_BUTTON_DOWN:
        //     break;

        case SDL_EVENT_MOUSE_MOTION:
            if (benchmarkActive) break;

            degreesCameraEulerAngle.y += (event->motion.xrel) * kMouseLookSensitivity;
            degreesCameraEulerAngle.x -= (event->motion.yrel) * kMouseLookSensitivity;
            degreesCameraEulerAngle.x = std::clamp(degreesCameraEulerAngle.x, -89.0f, 89.0f);

            RotateCameraLocal(degreesCameraEulerAngle);

        default:
            return CONTINUE;
    }

    return
            CONTINUE;
}

void App::Quit(SDL_AppResult result) {
    chunkConstructionThread.request_stop();
    chunkConstructionThread.join();
    chunkConstructionThread = std::jthread();

    frameProfiler.Report();

    auto quitStartTicks = SDL_GetTicks();
    SLog1("Quit event, freeing memory");

    SDL_WaitForGPUIdle(m_gpuDevice.get()); //Block thread until GPU is idle

    if (chunkMeshStore) {
        chunkMeshStore->ReleaseAll();
    }

    if (graphicsPipeline) {
        SDL_ReleaseGPUGraphicsPipeline(m_gpuDevice.get(), graphicsPipeline);
    }

    if (lineGraphicsPipeline) {
        SDL_ReleaseGPUGraphicsPipeline(m_gpuDevice.get(), lineGraphicsPipeline);
    }

    if (depthTexture) {
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), depthTexture);
    }

    if (normalTexture) {
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);
    }

    if (normalSampler) {
        SDL_ReleaseGPUSampler(m_gpuDevice.get(), normalSampler);
    }

    if (colourTexture) {
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
    }

    if (colourSampler) {
        SDL_ReleaseGPUSampler(m_gpuDevice.get(), colourSampler);
    }

    if (basePath) {
        SDL_free(basePath);
    }

    delete noise;
    noise = nullptr;

    cubeMesh.reset();
    chunkManager.reset();
    textureManager.reset();

    auto endTickDuration = (SDL_GetTicks() - quitStartTicks);
    SDL_Log("Quit took %zi ms", endTickDuration);

    SDL_ReleaseWindowFromGPUDevice(m_gpuDevice.get(), m_Window.get()); //Destroys window's swapchain texture

    m_Window.reset();
    m_gpuDevice.reset();
}

App::~App() = default;

void App::BuildToBeConstructedChunks() {
    std::scoped_lock lock(chunkQueueMutex);

    bool any = false;
    for (const auto &chunkPosition: toBeConstructedChunkPositions) {
        if (chunkManager->findChunk(chunkPosition) == nullptr) {
            ConstructChunkAtLine(chunkPosition.toVector());
            SDL_Log("Doing %i,%i,%i", chunkPosition.a, chunkPosition.b, chunkPosition.c);

            any = true;
        }
    }

    if (any) {
        auto lookup = ChunkManager::chunkLookupFromBlockPosition(cameraPositionSnapshot);
        chunkManager->markNeighborDirty(lookup, 0, 1);
        chunkManager->markNeighborDirty(lookup, 0, -1);
        chunkManager->markNeighborDirty(lookup, 1, 1);
        chunkManager->markNeighborDirty(lookup, 1, -1);
        chunkManager->markNeighborDirty(lookup, 2, 1);
        chunkManager->markNeighborDirty(lookup, 2, -1);
    }

    toBeConstructedChunkPositions.clear();
}

void App::ToggleBenchmark() {
    if (sceneCamera == nullptr) return;

    benchmarkActive = !benchmarkActive;

    if (benchmarkActive) {
        benchmarkSavedPosition = sceneCamera->Position;
        benchmarkFrame = 0;
        frameProfiler.StartWindow(kBenchmarkLoopFrames, "benchmark");
        LogProfilerHeader();
        return;
    }

    frameProfiler.Report();
    sceneCamera->Position = benchmarkSavedPosition;
    RotateCameraLocal(degreesCameraEulerAngle);
    sceneCamera->cameraVelocity = Vector(0.f, 0.f, 0.f);
    frameProfiler.StartWindow(kFreeModeProfilerWindowFrames, "free");
}

void App::ApplyBenchmarkPose() const {
    const BenchmarkPose pose = BenchmarkPoseForFrame(benchmarkFrame);
    sceneCamera->Position = pose.position;
    sceneCamera->pitch = pose.lookDegrees.x;
    sceneCamera->yaw = pose.lookDegrees.y;
    sceneCamera->roll = pose.lookDegrees.z;

    sceneCamera->UpdateDirectionVectors();
    // RotateCameraLocal(pose.lookDegrees);
}

void App::LogProfilerHeader() const {
#ifdef NDEBUG
    SDL_Log("[Profiler] header: mode=%s present=%s NDEBUG=%s compiler=%s debugLevel=%d worldChunks=%zu chunkMap=%zu",
            benchmarkActive ? "benchmark" : "free", presentModeName, kNdebugState, kCompilerVersion, debugLevel,
            chunkManager ? chunkManager->worldChunks.size() : 0, chunkManager ? chunkManager->chunkMap.size() : 0);
#else
    SDL_Log("[Profiler] header: mode=%s present=%s compiler=%s debugLevel=%d worldChunks=%zu chunkMap=%zu",
            benchmarkActive ? "benchmark" : "free", presentModeName, kCompilerVersion, debugLevel,
            chunkManager ? chunkManager->worldChunks.size() : 0, chunkManager ? chunkManager->chunkMap.size() : 0);
#endif
}

// SDL_AppResult App::OnQuit() {
// return SUCCESS;
// }

bool App::UploadDirtTexturesToGPU() {
    // std::string colourTexturePath = std::string(basePath) + "..\\" + pathToColourTexture;
    // std::string normalTexturePath = std::string(basePath) + "..\\" + pathToNormalTexture;
    //
    // SDL_Log("Creating and uploading GPU texture from %s and %s", colourTexturePath.c_str(), normalTexturePath.c_str());
    // SDL_IOStream *colourTextureStream = SDL_IOFromFile(colourTexturePath.c_str(), "rb");
    // SDL_IOStream *normalTextureStream = SDL_IOFromFile(normalTexturePath.c_str(), "rb");
    //
    // if (colourTextureStream == nullptr || normalTextureStream == nullptr) {
    //     SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not open texture: %s or %s", colourTexturePath.c_str(), normalTexturePath.c_str());
    //     return false;
    // }
    //
    // //Load surfaces of the files
    // SDL_Surface *colourSurface = IMG_LoadJPG_IO(colourTextureStream);
    // SDL_CloseIO(colourTextureStream);
    //
    // SDL_Surface *normalSurface = IMG_LoadJPG_IO(normalTextureStream);
    // SDL_CloseIO(normalTextureStream);
    //
    // if (colourSurface == nullptr || normalSurface == nullptr) {
    //     SDL_LogError(APP_LOG_CATEGORY_GENERIC, "IMG_LoadJPG_IO failed: %s", SDL_GetError());
    //     return false;
    // }
    //
    // //Convert surface's format as allegedly IMG_LoadJPG_IO may return surfaces with (random?) weird pixel formats
    // SDL_Surface *convertedColourSurface = SDL_ConvertSurface(colourSurface, SDL_PIXELFORMAT_RGBA32);
    // SDL_DestroySurface(colourSurface);
    //
    // SDL_Surface *convertedNormalSurface = SDL_ConvertSurface(normalSurface, SDL_PIXELFORMAT_RGBA32);
    // SDL_DestroySurface(normalSurface);
    //
    // if (!convertedColourSurface || !convertedNormalSurface) {
    //     SDL_LogError(APP_LOG_CATEGORY_GENERIC, "SDL_ConvertSurface failed");
    //     SDL_DestroySurface(convertedColourSurface);
    //     SDL_DestroySurface(convertedNormalSurface);
    //
    //     return false;
    // }

    if (this->textureManager == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "No texture manager.");
        return false;
    }

    SDL_Surface *convertedColourSurface = this->textureManager->loadSurfaceFromTexture(TextureManager::Dirt, TextureManager::Colour);
    SDL_Surface *convertedNormalSurface = this->textureManager->loadSurfaceFromTexture(TextureManager::Dirt, TextureManager::Normal);

    // Create the GPU texture
    SDL_GPUTextureCreateInfo colourTextureInfo = {};
    colourTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    colourTextureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    colourTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    colourTextureInfo.width = (Uint32) convertedColourSurface->w;
    colourTextureInfo.height = (Uint32) convertedColourSurface->h;
    colourTextureInfo.layer_count_or_depth = 1;
    colourTextureInfo.num_levels = 1;
    colourTextureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTextureCreateInfo normalTextureInfo = {};
    normalTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    normalTextureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    normalTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    normalTextureInfo.width = (Uint32) convertedNormalSurface->w;
    normalTextureInfo.height = (Uint32) convertedNormalSurface->h;
    normalTextureInfo.layer_count_or_depth = 1;
    normalTextureInfo.num_levels = 1;
    normalTextureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    colourTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &colourTextureInfo);
    normalTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &normalTextureInfo);

    SLog1(
        "colour=%ux%u | normal=%ux%u",
        colourTextureInfo.width,
        colourTextureInfo.height,
        normalTextureInfo.width,
        normalTextureInfo.height
    );

    if (colourTexture == nullptr || normalTexture == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "SDL_CreateGPUTexture failed");
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);

        SDL_DestroySurface(convertedColourSurface);
        SDL_DestroySurface(convertedNormalSurface);
        return false;
    }

    //Transfer buffer
    const Uint32 colourSizeInBytes = colourTextureInfo.height * colourTextureInfo.width * BytesPerPixel;
    const Uint32 normalSizeInBytes = normalTextureInfo.height * normalTextureInfo.width * BytesPerPixel;

    SDL_GPUTransferBufferCreateInfo colourTransferBufferInfo = {};
    colourTransferBufferInfo.size = colourSizeInBytes;
    colourTransferBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    SDL_GPUTransferBufferCreateInfo normalTransferBufferInfo = {};
    normalTransferBufferInfo.size = normalSizeInBytes;
    normalTransferBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    SDL_GPUTransferBuffer *colourTransferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice.get(), &colourTransferBufferInfo);
    SDL_GPUTransferBuffer *normalTransferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice.get(), &normalTransferBufferInfo);

    if (colourTransferBuffer == nullptr || normalTransferBuffer == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Transfer buffer creation failed: %s", SDL_GetError());

        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);

        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), colourTransferBuffer);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), normalTransferBuffer);

        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);

        SDL_DestroySurface(convertedColourSurface);
        SDL_DestroySurface(convertedNormalSurface);
        return false;
    }

    //Map and copy row by row
    Uint8 *colourTransferMapped = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(m_gpuDevice.get(), colourTransferBuffer, false));
    Uint8 *normalTransferMapped = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(m_gpuDevice.get(), normalTransferBuffer, false));

    if (colourTransferMapped == nullptr || normalTransferMapped == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());

        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);

        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), colourTransferBuffer);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), normalTransferBuffer);

        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);
        SDL_DestroySurface(convertedColourSurface);
        SDL_DestroySurface(convertedNormalSurface);
        return false;
    }

    Uint8 *colourSourcePixels, *normalSourcePixels;
    colourSourcePixels = static_cast<Uint8 *>(convertedColourSurface->pixels);
    normalSourcePixels = static_cast<Uint8 *>(convertedNormalSurface->pixels);

    const Uint32 normalBytesPerRow = normalTextureInfo.width * BytesPerPixel;
    const Uint32 colourBytesPerRow = colourTextureInfo.width * BytesPerPixel;

    for (Uint32 y = 0; y < colourTextureInfo.height; ++y) {
        SDL_memcpy(colourTransferMapped + (y * colourBytesPerRow),
                   colourSourcePixels + (y * convertedColourSurface->pitch),
                   colourBytesPerRow);
    }
    for (Uint32 y = 0; y < normalTextureInfo.height; ++y) {
        SDL_memcpy(normalTransferMapped + (y * normalBytesPerRow),
                   normalSourcePixels + (y * convertedNormalSurface->pitch),
                   normalBytesPerRow);
    }

    SDL_DestroySurface(convertedColourSurface);
    SDL_DestroySurface(convertedNormalSurface);

    SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), colourTransferBuffer);
    SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), normalTransferBuffer);

    //Upload
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_gpuDevice.get());

    if (cmd == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Acquire command buffer failed: %s", SDL_GetError());

        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);

        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), colourTransferBuffer);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), normalTransferBuffer);

        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);
        return false;
    }

    SDL_GPUCopyPass *colourCopyPass = SDL_BeginGPUCopyPass(cmd);
    if (colourCopyPass == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Begin copy pass failed: %s", SDL_GetError());

        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);

        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), colourTransferBuffer);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), normalTransferBuffer);

        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);

        SDL_SubmitGPUCommandBuffer(cmd);
        return false;
    }

    SDL_GPUTextureTransferInfo colourSourceInfo = {};
    colourSourceInfo.transfer_buffer = colourTransferBuffer;
    colourSourceInfo.offset = 0;
    colourSourceInfo.pixels_per_row = colourTextureInfo.width;
    colourSourceInfo.rows_per_layer = colourTextureInfo.height;

    SDL_GPUTextureRegion colourDestinationInfo = {};
    colourDestinationInfo.texture = colourTexture;
    colourDestinationInfo.layer = 0;
    colourDestinationInfo.x = 0;
    colourDestinationInfo.y = 0;
    colourDestinationInfo.z = 0;
    colourDestinationInfo.w = colourTextureInfo.width;
    colourDestinationInfo.h = colourTextureInfo.height;
    colourDestinationInfo.d = 1;

    SDL_UploadToGPUTexture(colourCopyPass, &colourSourceInfo, &colourDestinationInfo, false);
    SDL_EndGPUCopyPass(colourCopyPass);

    SDL_GPUCopyPass *normalCopyPass = SDL_BeginGPUCopyPass(cmd);
    if (normalCopyPass == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Begin copy pass failed: %s", SDL_GetError());

        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);

        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), colourTransferBuffer);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), normalTransferBuffer);

        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);

        SDL_SubmitGPUCommandBuffer(cmd);
        return false;
    }

    SDL_GPUTextureTransferInfo normalSourceInfo = {};
    normalSourceInfo.transfer_buffer = normalTransferBuffer;
    normalSourceInfo.offset = 0;
    normalSourceInfo.pixels_per_row = normalTextureInfo.width;
    normalSourceInfo.rows_per_layer = normalTextureInfo.height;

    SDL_GPUTextureRegion normalDestinationInfo = {};
    normalDestinationInfo.texture = normalTexture;
    normalDestinationInfo.layer = 0;
    normalDestinationInfo.x = 0;
    normalDestinationInfo.y = 0;
    normalDestinationInfo.z = 0;
    normalDestinationInfo.w = normalTextureInfo.width;
    normalDestinationInfo.h = normalTextureInfo.height;
    normalDestinationInfo.d = 1;

    SDL_UploadToGPUTexture(normalCopyPass, &normalSourceInfo, &normalDestinationInfo, false);
    SDL_EndGPUCopyPass(normalCopyPass);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Submit command buffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), normalTexture);

        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), colourTransferBuffer);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), normalTransferBuffer);

        SDL_DestroySurface(convertedColourSurface);
        SDL_DestroySurface(convertedNormalSurface);
        return false;
    }

    SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), colourTransferBuffer);
    SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), normalTransferBuffer);

    SDL_DestroySurface(convertedColourSurface);
    SDL_DestroySurface(convertedNormalSurface);

    //Create sampler for the texture
    SDL_GPUSamplerCreateInfo samplerInfo = {};
    samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerInfo.max_lod = 1.0f;

    colourSampler = SDL_CreateGPUSampler(m_gpuDevice.get(), &samplerInfo);
    normalSampler = SDL_CreateGPUSampler(m_gpuDevice.get(), &samplerInfo);

    if (colourSampler == nullptr || normalSampler == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Sampler creation failed: %s", SDL_GetError());
        return false;
    }

    SLog1("Texture loaded successfully: %dx%d and %dx%d", colourTextureInfo.width, colourTextureInfo.height, normalTextureInfo.width, normalTextureInfo.height);
    return true;
}

SDL_AppResult App::OnUpdate() {
    deltaTimeMS = SDL_GetTicks() - currentMillisecondsSinceStart;
    sceneCamera->deltaTimeMS = deltaTimeMS;

    currentMillisecondsSinceStart = SDL_GetTicks();

    // std::scoped_lock lock(chunkQueueMutex);
    cameraPositionSnapshot = sceneCamera->Position;
    BuildToBeConstructedChunks();

    if (benchmarkActive) {
        ApplyBenchmarkPose();
        benchmarkFrame += 1;
        return CONTINUE;
    }

    sceneCamera->MoveCameraBasedOnVelocity();

    auto hit = RaycastRay(sceneCamera->Position, GetGravityVector().Normalized(), kCameraHeightAboveGround);
    if (hit.hit) {
        sceneCamera->ResetVelocityAlongWorldAxis(GetGravityVector().Normalized());
        sceneCamera->Position.y = hit.blockPosition.y + kBlockHalfExtent + kCameraHeightAboveGround;
    } else {
        sceneCamera->AddForceThisTick((GetGravityVector() * ((float) deltaTimeMS / 1000.f * kGravityMultiplier)));
    }

    //temporary
    if (sceneCamera->Position.y < cameraMinYPosition) {
        sceneCamera->ResetVelocityAlongWorldAxis(GetGravityVector());
        sceneCamera->Position.y = cameraMinYPosition + kCameraHeightAboveGround;
    }

    return CONTINUE;
}

void App::ConstructChunkAtLine(Vector atPos, bool flat) const {
    std::vector<Chunk<ChunkManager::chunkSizeXYZ> > yChunks;
    for (int x = 0; x < ChunkManager::chunkSizeXYZ; x += 1) {
        for (int z = 0; z < ChunkManager::chunkSizeXYZ; z += 1) {
            //simplex noise generates value 0 in integer coordinates
            float rawNoiseVal = SimplexNoise::noise((x * noise->mFrequency) + epsilon, (z * noise->mFrequency) + epsilon);
            rawNoiseVal += 1;
            rawNoiseVal *= 2;

            if (true) {
                //rawNoiseVal = pow(2, rawNoiseVal);
            }

            int rawNoiseValInt = static_cast<int>(rawNoiseVal);

            int distToGroundZero = rawNoiseValInt - groundZeroYLevel;
            int firstChunkBlocks = distToGroundZero % 16;
            int numChunksOnY = static_cast<int>(ceil(distToGroundZero / 16.00)); //ceil rounds up

            //SDL_Log("Line %i, %i %i chunks with dist %i has noise value of %f (%i)", x, z, numChunksOnY, distToGroundZero, rawNoiseVal, rawNoiseValInt);

            Chunk<ChunkManager::chunkSizeXYZ> *currentlyWorkingChunk = nullptr; //TODO: replace w index

            //Chunk at index 0 is the "highest chunk"
            for (int chunkI = 0; chunkI < numChunksOnY; chunkI += 1) {
                Vector currentPlacePos = Vector(atPos.x, atPos.y - (chunkI * ChunkManager::chunkSizeXYZ), atPos.z);

                for (const auto &chunk: yChunks) {
                    //if (chunk.atPosition == currentPlacePos) {
                    if (chunk.PositionInBounds(currentPlacePos)) {
                        currentlyWorkingChunk = const_cast<Chunk<ChunkManager::chunkSizeXYZ> *>(&chunk);
                        SLog2("Found chunk at %f,%f,%f", currentPlacePos.x, currentPlacePos.y, currentPlacePos.z);
                        break;
                    }
                }

                if (currentlyWorkingChunk == nullptr) {
                    Chunk<ChunkManager::chunkSizeXYZ> newChunk(Vector(atPos.x, atPos.y - (chunkI * ChunkManager::chunkSizeXYZ), atPos.z));
                    yChunks.push_back(std::move(newChunk));
                    currentlyWorkingChunk = &yChunks.back();

                    SLog2("Creating new chunk at %f,%f,%f", currentPlacePos.x, currentPlacePos.y, currentPlacePos.z);
                }

                if (currentlyWorkingChunk == nullptr) {
                    throw std::runtime_error("Failed to create chunk");
                    return;
                }

                for (int chunkY = (chunkI == 0 ? firstChunkBlocks : 16) - 1; chunkY >= 0; chunkY--) {
                    currentlyWorkingChunk->tryInsert(Vector(x, chunkY, z), {cubeMesh.get(), Vector(x, -chunkI * ChunkManager::chunkSizeXYZ + chunkY, z)});
                    //currentlyWorkingChunk->blocks[Vector(x, chunkY, z)] = {cubeMesh.get(), Vector(x, -chunkI * ChunkManager::chunkSizeXYZ + chunkY, z)};
                    //SDL_Log("Creating new block at position %i,%i,%i", x, chunkY, z);
                }
            }
        }
    }

    for (const auto &chunk: yChunks) {
        chunkManager->worldChunks.push_back(chunk);
        chunkManager->chunkMap.insert({chunk.atPosition.toInt3(), chunkManager->worldChunks.size() - 1});
    }
}

RaycastHit App::CheckIsPointInsideAny(Vector point) const {
    const Vector worldBlockPosition = BlockPositionFromPoint(point);
    const Int3 chunkLookup = ChunkManager::chunkLookupFromBlockPosition(worldBlockPosition);

    auto iterator = chunkManager->chunkMap.find(chunkLookup); //std::make_pair(chunkX, chunkZ));
    if (iterator != chunkManager->chunkMap.end()) {
        const auto &chunk = chunkManager->worldChunks[iterator->second];
        const Vector blockPosition = worldBlockPosition - chunk.atPosition;

        // const auto block = chunk.blocks.find(blockPosition);
        const auto blockAt = chunk.tryGet(blockPosition);
        if (blockAt != nullptr && blockAt->has_value()) {
            return RaycastHit{
                .hit = true,
                .blockType = true,
                .blockPosition = blockAt->value().Position + chunk.atPosition
            };
        }
    }

    return RaycastHit_NULL;
}

///3D implementation of DDA alg. from this source: https://aaaa.sh/creatures/dda-algorithm-interactive/
RaycastHit App::RaycastRay(Vector pos, Vector normDir, float maxDistance) const {
    normDir = normDir.Normalized();
    if (normDir.Magnitude() == 0.f || maxDistance < 0.f) {
        return RaycastHit_NULL;
    }

    Vector signVector = Vector(signOf(normDir.x), signOf(normDir.y), signOf(normDir.z));
    Vector mapCheck = BlockPositionFromPoint(pos);
    const float infinity = std::numeric_limits<float>::infinity();

    Vector rayUnitStepSize(
        normDir.x == 0.f ? infinity : std::fabs(1.f / normDir.x),
        normDir.y == 0.f ? infinity : std::fabs(1.f / normDir.y),
        normDir.z == 0.f ? infinity : std::fabs(1.f / normDir.z)
    );

    Vector rayLength(
        normDir.x > 0.f ? (mapCheck.x + kBlockHalfExtent - pos.x) * rayUnitStepSize.x : (pos.x - (mapCheck.x - kBlockHalfExtent)) * rayUnitStepSize.x,
        normDir.y > 0.f ? (mapCheck.y + kBlockHalfExtent - pos.y) * rayUnitStepSize.y : (pos.y - (mapCheck.y - kBlockHalfExtent)) * rayUnitStepSize.y,
        normDir.z > 0.f ? (mapCheck.z + kBlockHalfExtent - pos.z) * rayUnitStepSize.z : (pos.z - (mapCheck.z - kBlockHalfExtent)) * rayUnitStepSize.z
    );

    if (normDir.x == 0.f) rayLength.x = infinity;
    if (normDir.y == 0.f) rayLength.y = infinity;
    if (normDir.z == 0.f) rayLength.z = infinity;

    SLog2("Raycast debug:");
    SLog2("\tFrom %f,%f,%f", pos.x, pos.y, pos.z);
    SLog2("\tMap offset %f,%f,%f", mapCheck.x, mapCheck.y, mapCheck.z);
    SLog2("\tWith dir %f,%f,%f", normDir.x, normDir.y, normDir.z);
    SLog2("\tRay unit step size %f,%f,%f", rayUnitStepSize.x, rayUnitStepSize.y, rayUnitStepSize.z);
    SLog2("\tRay length %f,%f,%f", rayLength.x, rayLength.y, rayLength.z);

    float travelledDistance = 0.f;
    while (travelledDistance <= maxDistance) {
        SLog2("Checking position %f,%f,%f", mapCheck.x, mapCheck.y, mapCheck.z);

        RaycastHit result = CheckIsPointInsideAny(mapCheck);
        if (result.hit) {
            SLog2("Hit! At %f, %f, %f", mapCheck.x, mapCheck.y, mapCheck.z);
            return result;
        }

        if (rayLength.x <= rayLength.y && rayLength.x <= rayLength.z) {
            travelledDistance = rayLength.x;
            if (travelledDistance > maxDistance) break;
            mapCheck.x += signVector.x;
            rayLength.x += rayUnitStepSize.x;
        } else if (rayLength.y <= rayLength.z) {
            travelledDistance = rayLength.y;
            if (travelledDistance > maxDistance) break;
            mapCheck.y += signVector.y;
            rayLength.y += rayUnitStepSize.y;
        } else {
            travelledDistance = rayLength.z;
            if (travelledDistance > maxDistance) break;
            mapCheck.z += signVector.z;
            rayLength.z += rayUnitStepSize.z;
        }
    }

    return RaycastHit_NULL;
}

constexpr Vector App::GetGravityVector() {
    return Vector(0, -1, 0) * 9.81f;
}

SDL_AppResult App::OnRender() {
    SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(m_gpuDevice.get());
    if (commandBuffer == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to acquire command buffer: %s", SDL_GetError());
        return FAILURE;
    }

    sceneCamera->UpdateCameraFrustrumCorners();

    Matrix4D viewMatrix = sceneCamera->GetViewMatrix();
    Matrix4D projectionMatrix = sceneCamera->GetProjectionMatrix();

    ChunkUniforms uniforms{};
    (projectionMatrix * viewMatrix).toOutFloat16Array(uniforms.viewProjection);

    std::optional<FrameProfiler::StageScope> stageScope;
    stageScope.emplace(frameProfiler, FrameProfiler::Stage::Build);

    std::vector<ChunkMeshStore::MeshUpdate> meshUpdates;
    Uint64 bytesToUpload = 0;
    for (auto &chunk: chunkManager->worldChunks) {
        if (!chunk.meshDirty) continue;

        std::vector<Vertex3D> vertices = buildChunkMesh(chunk, *chunkManager);
        bytesToUpload += vertices.size() * sizeof(Vertex3D);
        meshUpdates.push_back({chunk.atPosition.toInt3(), std::move(vertices)});
        chunk.meshDirty = false;
    }
    frameProfiler.AddCount(FrameProfiler::Counter::ChunksRemeshed, meshUpdates.size());

    if (!meshUpdates.empty()) {
        stageScope.emplace(frameProfiler, FrameProfiler::Stage::Upload);
        if (!chunkMeshStore->Upload(commandBuffer, meshUpdates)) {
            SDL_SubmitGPUCommandBuffer(commandBuffer);
            return FAILURE;
        }
        frameProfiler.AddCount(FrameProfiler::Counter::BytesUploaded, bytesToUpload);
    }

    stageScope.emplace(frameProfiler, FrameProfiler::Stage::Acquire);
    //Acquire the swapchain texture for rendering
    SDL_GPUTexture *swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, m_Window.get(), &swapchainTexture, nullptr, nullptr)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not acquire swapchain texture: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return FAILURE;
    }

    if (swapchainTexture == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Swapchain texture is null");
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return FAILURE;
    }

    stageScope.emplace(frameProfiler, FrameProfiler::Stage::Record);

    //Set up the colour target info for the render pass
    SDL_GPUColorTargetInfo colorTargetInfo = {};
    colorTargetInfo.texture = swapchainTexture;
    colorTargetInfo.clear_color = (SDL_FColor){0.1f, 0.1f, 0.2f, 1.0f}; //dark blue-grey
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo depthTarget = {};
    depthTarget.texture = this->depthTexture;
    depthTarget.clear_depth = 1.0f; //far plane value
    depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    depthTarget.store_op = SDL_GPU_STOREOP_STORE;
    depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    //Begin render pass
    SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, &depthTarget);
    if (renderPass == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to begin render pass");
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return FAILURE;
    }

    SDL_GPUViewport viewport = {0, 0, (float) defaultScreenWidth, (float) defaultScreenHeight, 0.0f, 1.0f};
    SDL_SetGPUViewport(renderPass, &viewport);

    SDL_BindGPUGraphicsPipeline(renderPass, graphicsPipeline);

    SDL_GPUTextureSamplerBinding bindings[2] =
    {
        {colourTexture, colourSampler},
        {normalTexture, normalSampler},
    };
    SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 2);

    Uint64 verticesDrawn = 0;
    Uint64 chunksDrawn = 0;
    for (const auto &[chunkLookup, mesh]: chunkMeshStore->Meshes()) {
        if (mesh.vertexCount == 0 || !IsChunkInFrustum(*sceneCamera, chunkLookup)) continue;

        uniforms.chunkOffset[0] = static_cast<float>(chunkLookup.a);
        uniforms.chunkOffset[1] = static_cast<float>(chunkLookup.b);
        uniforms.chunkOffset[2] = static_cast<float>(chunkLookup.c);
        SDL_PushGPUVertexUniformData(commandBuffer, 0, &uniforms, sizeof(uniforms));

        const SDL_GPUBufferBinding vertexBinding = {mesh.buffer, 0};
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
        SDL_DrawGPUPrimitives(renderPass, mesh.vertexCount, 1, 0, 0);

        verticesDrawn += mesh.vertexCount;
        chunksDrawn += 1;
    }
    frameProfiler.AddCount(FrameProfiler::Counter::VerticesDrawn, verticesDrawn);
    frameProfiler.AddCount(FrameProfiler::Counter::ChunksDrawn, chunksDrawn);

    SDL_EndGPURenderPass(renderPass);

    stageScope.emplace(frameProfiler, FrameProfiler::Stage::Submit);
    if (!SDL_SubmitGPUCommandBuffer(commandBuffer)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to submit command buffer: %s", SDL_GetError());
        return FAILURE;
    }

    return CONTINUE;
}
