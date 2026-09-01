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
#include <execution>
#include <limits>
#include <ranges>
#include <utility>

#include "core/noise/SimplexNoise.h"
#include "core/Vector.h"
#include "core/Camera.h"
#include "core/Chunk.h"
#include "core/Matrix4D.h"
#include "core/Object.h"
#include "core/Vertex3D.h"

//POSITION IS FROM -1 TO 1
//COLOR FROM 0 to 1
//N. COORDS FROM 0 TO 1

//To conpile shaders use glslc commands:
//  glslc -fshader-stage=vertex src/shaders/vertex.glsl -o src/shaders/vertex.spv
//  glslc -fshader-stage=fragment src/shaders/fragment.glsl -o src/shaders/fragment.spv
//  glslc -fshader-stage=fragment src/shaders/linefragment.glsl -o src/shaders/lifragment.spv

static SimplexNoise *noise;

Vector startingCameraPos = Vector(0.f, 15.f, 0.f);
Vector degreesCameraEulerAngle = Vector(0.f, 0.f, 0.f);
std::unique_ptr<Camera> sceneCamera = nullptr;

//TODO: Before full release, change CMakeList.txt to put built shaders in build dir
//TODO: Improve RaycastRay accuracy / reliability
//TODO: Add caching to texture manager, maybe some CMake commands to recache
//TODO: Optimize with diff. cullings
//TODO: Add screen space GI?

namespace {
    void BuildCameraFromState() {
        sceneCamera = std::make_unique<Camera>(startingCameraPos, degreesCameraEulerAngle.x, degreesCameraEulerAngle.y, degreesCameraEulerAngle.z, defaultAspectRatio);
    }

    Vector HorizontalDirection(Vector direction) {
        direction.y = 0.f;
        return direction.Normalized();
    }

    int BlockCoordinateFromPoint(const float point) {
        return static_cast<int>(std::floor(point + kBlockHalfExtent));
    }

    int ChunkCoordinateFromBlockCoordinate(const int blockCoordinate) {
        return static_cast<int>(std::floor(static_cast<float>(blockCoordinate) / ChunkManager::chunkSizeXYZ)) * ChunkManager::chunkSizeXYZ;
    }

    Vector BlockPositionFromPoint(const Vector &point) {
        return Vector(
            static_cast<float>(BlockCoordinateFromPoint(point.x)),
            static_cast<float>(BlockCoordinateFromPoint(point.y)),
            static_cast<float>(BlockCoordinateFromPoint(point.z))
        );
    }

    Int3 ChunkLookupFromBlockPosition(const Vector &blockPosition) {
        return {
            ChunkCoordinateFromBlockCoordinate(static_cast<int>(blockPosition.x)),
            ChunkCoordinateFromBlockCoordinate(static_cast<int>(blockPosition.y)),
            ChunkCoordinateFromBlockCoordinate(static_cast<int>(blockPosition.z))
        };
    }

    [[deprecated]] [[maybe_unused]] void MoveCameraLocal(const Vector &localDirection, const float distance) {
        sceneCamera->UpdateDirectionVectors();
        const Vector worldOffset =
                (sceneCamera->right * localDirection.x) +
                (sceneCamera->up * localDirection.y) +
                (sceneCamera->forward * localDirection.z);
        sceneCamera->Position += worldOffset * distance;
    }

    void MoveCameraHorizontal(const Vector &localDirection, const float distance) {
        sceneCamera->UpdateDirectionVectors();
        const Vector worldOffset =
                (HorizontalDirection(sceneCamera->right) * localDirection.x) +
                (HorizontalDirection(sceneCamera->forward) * localDirection.z);

        if (worldOffset.Magnitude() > 0.f) {
            sceneCamera->Position += worldOffset.Normalized() * distance;
        }
    }

    void MoveCameraWithForce(const Vector &forceDir, const float mag) {
        sceneCamera->AddForceThisTick(forceDir, mag);
    }

    void RotateCameraLocal(const Vector degreesCameraEulerAngle) {
        sceneCamera->pitch = degreesCameraEulerAngle.x;
        sceneCamera->yaw = degreesCameraEulerAngle.y;
        sceneCamera->roll = degreesCameraEulerAngle.z;

        sceneCamera->UpdateDirectionVectors();
    }

    void MoveCameraBasedOnStates() {
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
}

App::App(int argc, char **argv) : m_Window(nullptr, &SDL_DestroyWindow), m_gpuDevice(nullptr, &SDL_DestroyGPUDevice) {
}

SDL_AppResult App::Init() {
    SDL_SetAppMetadata("2DRenderer", "1.0.0", "com.cozyprogramming.renderer2d");

    SDL_Log("Initializing SDL library %llu ms...", SDL_GetTicks());
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not initialize SDL library: %s", SDL_GetError());
        return FAILURE;
    }

    SDL_Log("Initializing SDL Window %llu ms...", SDL_GetTicks());
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

    SDL_Log("Base path: %s", this->basePath);

    //Choose driver based on preferred drivers
    const std::array<std::string, 2> preferredDrives
    {
        std::string{"vulkan"},
        std::string{"direct3d12"},
    };

    auto numGPUDrivers = SDL_GetNumGPUDrivers();
    std::vector<std::string> gpuDrivers;
    gpuDrivers.reserve(numGPUDrivers);

    SDL_Log("Initializing SDL GPU device %llu ms...", SDL_GetTicks());
    SDL_Log("Supported GPU drivers: ");
    for (int i = 0; i < numGPUDrivers; i += 1) {
        SDL_Log("\tDetected driver: %s", SDL_GetGPUDriver(i));
        gpuDrivers.emplace_back(SDL_GetGPUDriver(i));
    }

    std::string selectedDriver;
    for (const auto driver: preferredDrives) {
        if (std::ranges::find(gpuDrivers, driver) != gpuDrivers.end()) {
            SDL_Log("Using preffered driver: %s", driver.c_str());
            selectedDriver = driver;
            break;
        }
    }

    SDL_Log("Creating GPU driver device %llu ms...", SDL_GetTicks());
    m_gpuDevice.reset(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, false,
        selectedDriver.empty() ? nullptr : selectedDriver.c_str()));

    if (m_gpuDevice == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create GPU device: %s", SDL_GetError());
        return FAILURE;
    }

    SDL_Log("Selected GPU driver: %s", SDL_GetGPUDeviceDriver(m_gpuDevice.get()));
    SDL_Log("Claiming window for GPU device %llu ms...", SDL_GetTicks());
    if (!SDL_ClaimWindowForGPUDevice(m_gpuDevice.get(), m_Window.get())) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not claim SDL window to GPU device: %s", SDL_GetError());
        return FAILURE;
    }

    SDL_GPUPresentMode presentMode = SDL_GPU_PRESENTMODE_VSYNC; //VSYNC is supported everywhere, but if mailbox is supported use it as its faster
    if (SDL_WindowSupportsGPUPresentMode(m_gpuDevice.get(), m_Window.get(), SDL_GPU_PRESENTMODE_MAILBOX)) {
        presentMode = SDL_GPU_PRESENTMODE_MAILBOX;
    }

    SDL_Log("Setting GPU swapchain parameters %llu ms...", SDL_GetTicks());
    SDL_SetGPUSwapchainParameters(m_gpuDevice.get(), m_Window.get(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR, presentMode);

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

    SDL_Log("Loaded vertex shader from %s, size: %zu", vPath.c_str(), vertexShaderCodeSize);
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

    SDL_Log("Vertex shader created: %p", vertexShader);
    SDL_free(vertexShaderCode);

    SDL_Log("Loading and creating fragment shaders %llu ms...", SDL_GetTicks());
    size_t fragmentShaderCodeSize;
    void *fragmentShaderCode = SDL_LoadFile(fPath.c_str(), &fragmentShaderCodeSize);
    if (fragmentShaderCodeSize == 0) {
        SDL_free(fragmentShaderCode);
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to load fragment shader: %s", SDL_GetError());
        return FAILURE;
    }

    SDL_Log("Loaded fragment shader from %s, size: %zu", fPath.c_str(), fragmentShaderCodeSize);

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

    SDL_Log("Loaded fragment shader from %s, size: %zu", fPath.c_str(), lineFragmentCodeSize);

    SDL_GPUShaderCreateInfo lineFragmentCreationInfo{
        .code_size = lineFragmentCodeSize,
        .code = static_cast<Uint8 *>(lineFragmentShaderCode),
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 0,
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

    SDL_Log("Fragment shader created: %p", fragmentShader);

    if (!lineFragmentShader) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create line fragment shader: %s", SDL_GetError());
        SDL_free(fragmentShaderCode);
        SDL_free(lineFragmentShaderCode);
        return FAILURE;
    }

    SDL_Log("Line fragment shader created: %p", lineFragmentShader);

    SDL_free(fragmentShaderCode);
    SDL_free(lineFragmentShaderCode);

    SDL_Log("Creating GPU pipelines infos %llu ms...", SDL_GetTicks());

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

    SDL_Log("Creating GPU pipelines %llu ms...", SDL_GetTicks());
    graphicsPipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice.get(), &pipelineInfo);
    lineGraphicsPipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice.get(), &pipelineLineInfo);
    SDL_Log("Graphics pipelines created: %p", graphicsPipeline);

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

    SDL_Log("Creating managers...");
    this->chunkManager = std::make_unique<ChunkManager>();
    this->textureManager = std::make_unique<TextureManager>(this->basePath);

    //texture manager config - to be replaced with threaded loading system and caching
    this->textureManager->AddEntryForBlockType(TextureManager::Dirt, "img\\dirt\\");
    this->textureManager->AddEntryForBlockType(TextureManager::OakLog, "img\\oak_log\\");

    if (!UploadDirtTexturesToGPU()) {
        return FAILURE;
    }

    noise = new SimplexNoise(0.15f, 3, 0, 0);

    ConstructChunkAt(Vector(0, 0, 0));
    ConstructChunkAt(Vector(16, 0, 0));
    ConstructChunkAt(Vector(0, 0, 16));
    ConstructChunkAt(Vector(16, 0, 16));
    ConstructChunkAt(Vector(-16, 0, 0));
    ConstructChunkAt(Vector(0, 0, -16));
    ConstructChunkAt(Vector(-16, 0, -16));

    delete noise;

    SDL_WaitForGPUIdle(m_gpuDevice.get()); //Block rendering and showing window until gpu is idle

    SDL_Log("Showing the window %llu ms...", SDL_GetTicks());

    //Creating gpu device in swap chain takes long time, people would see empty or trashed window, so we show window after some time
    if (!SDL_ShowWindow(m_Window.get())) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not show SDL window: %s", SDL_GetError());
        return FAILURE;
    }

    currentMillisecondsSinceStart = SDL_GetTicks();

    SDL_Log("All done %llu ms...", SDL_GetTicks());
    return CONTINUE;
}

SDL_AppResult App::Iterate() {
    if (sceneCamera != nullptr) {
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
            // std::cout << sceneCamera->aspect;
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
                auto raycastHit = RaycastRay(sceneCamera->Position - Vector(0, 0.1f, 0), GetGravityVector().Normalized(), kCameraHeightAboveGround);
                if (raycastHit.hit) {
                    sceneCamera->SetMoveState(Camera::Up, true);
                }
            }

            if (event->key.key == SDLK_F) {
                sceneCamera->UpdateCameraFrustrumCorners();
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

            if (event->key.key == SDLK_E) {
                Vector cameraLookVector = sceneCamera->forward;

                SDL_Log("Camera looking at %f,%f,%f", cameraLookVector.x, cameraLookVector.y, cameraLookVector.z);
                auto result = RaycastRay(sceneCamera->Position, cameraLookVector.Normalized(), 5, true);
                if (result.hit) {
                    const Vector towerBase = result.blockPosition + Vector(0.f, 1.f, 0.f);
                    const std::array<Vector, 4> towerOffsets = {
                        Vector(0.f, 0.f, 0.f),
                        Vector(1.f, 0.f, 0.f),
                        Vector(1.f, 0.f, 1.f),
                        Vector(0.f, 0.f, 1.f)
                    };

                    for (const Vector &towerOffset: towerOffsets) {
                        for (int i = 0; i < 5; i += 1) {
                            const Vector worldBlockPosition = towerBase + towerOffset + Vector(0.f, static_cast<float>(i), 0.f);
                            const Int3 chunkLookup = ChunkLookupFromBlockPosition(worldBlockPosition);
                            auto iterator = chunkManager->chunkMap.find(chunkLookup);
                            if (iterator == chunkManager->chunkMap.end()) {
                                const size_t newChunkIndex = chunkManager->worldChunks.size();
                                chunkManager->worldChunks.emplace_back(chunkLookup.toVector());
                                iterator = chunkManager->chunkMap.insert({chunkLookup, newChunkIndex}).first;
                            }

                            auto &chunk = chunkManager->worldChunks[iterator->second];
                            const Vector localBlockPosition = worldBlockPosition - chunk.atPosition;

                            SDL_Log("Adding block at %f,%f,%f", worldBlockPosition.x, worldBlockPosition.y, worldBlockPosition.z);
                            chunk.blocks[localBlockPosition] = Object{cubeMesh.get(), worldBlockPosition};
                            chunk.didUserEditChunk = true;
                        }
                    }
                }
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            degreesCameraEulerAngle.y += (event->motion.xrel) * kMouseLookSensitivity;
            degreesCameraEulerAngle.x -= (event->motion.yrel) * kMouseLookSensitivity;
            degreesCameraEulerAngle.x = std::clamp(degreesCameraEulerAngle.x, -89.0f, 89.0f);

            RotateCameraLocal(degreesCameraEulerAngle);

        default:
            return CONTINUE;
    }
    return CONTINUE;
}

void App::Quit(SDL_AppResult result) const {
    //Disable compiler warn about unused result arg: https://stackoverflow.com/questions/58019275/what-is-the-purpose-of-voidvariable-in-c
    //(void) result;

    SDL_Log("Quit event, freeing memory");

    SDL_WaitForGPUIdle(m_gpuDevice.get()); //Block thread until GPU is idle

    if (sceneVertexBuffer) {
        SDL_ReleaseGPUBuffer(m_gpuDevice.get(), sceneVertexBuffer);
    }

    if (uniformBuffer) {
        SDL_ReleaseGPUBuffer(m_gpuDevice.get(), uniformBuffer);
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

    SDL_ReleaseWindowFromGPUDevice(m_gpuDevice.get(), m_Window.get()); //Destroys window's swapchain texture
}

App::~App() = default;

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

    SDL_Log(
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
    constexpr Uint32 BytesPerPixel = 4; //8 bits from red, green, blue, alpha channels = 32 bits = 4 bytes
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

    SDL_Log("Texture loaded successfully: %dx%d and %dx%d", colourTextureInfo.width, colourTextureInfo.height, normalTextureInfo.width, normalTextureInfo.height);
    return true;
}

SDL_AppResult App::OnUpdate() {
    deltaTimeMS = SDL_GetTicks() - currentMillisecondsSinceStart;
    sceneCamera->deltaTimeMS = deltaTimeMS;

    currentMillisecondsSinceStart = SDL_GetTicks();

    sceneCamera->MoveCameraBasedOnVelocity();

    if (sceneCamera->Position.y >= kCameraHeightAboveGround) {
        sceneCamera->AddForceThisTick((GetGravityVector() * ((float) deltaTimeMS / 1000.f * kGravityMultiplier)));
    } else {
        // SDL_Log("%f", sceneCamera->Position.y);
        sceneCamera->ResetVelocityAlongWorldAxis(Vector(0, 1, 0));
        sceneCamera->Position.y = 0 + kCameraHeightAboveGround;
    }

    auto hit = RaycastRay(sceneCamera->Position, GetGravityVector().Normalized(), kCameraHeightAboveGround);
    if (hit.hit) {
        sceneCamera->ResetVelocityAlongWorldAxis(GetGravityVector().Normalized());
        sceneCamera->Position.y = hit.blockPosition.y + kBlockHalfExtent + kCameraHeightAboveGround;
    }

    return CONTINUE;
}

void App::ConstructChunkAt(Vector atPos, bool flat) const {
    Chunk<ChunkManager::chunkSizeXYZ> chunk(atPos);
    for (int x = 0; x < ChunkManager::chunkSizeXYZ; x++) {
        for (int z = 0; z < ChunkManager::chunkSizeXYZ; z++) {
            float rawNoiseVal = SimplexNoise::noise(x * noise->mFrequency, z * noise->mFrequency);
            rawNoiseVal += 0.5f;
            rawNoiseVal /= 2;

            rawNoiseVal *= noise->mAmplitude;

            // int y = flat ? 0 : (int) (noise->mAmplitude * rawNoiseVal);
            int y = flat ? 0 : (int) rawNoiseVal;

            //chunk.blocks[Vector(x, 0, z)] = {cubeMesh, chunk.atPosition + Vector(x, 0, z)};

            // SDL_Log("x=%d, z=%d", x, z);
            const Vector pos = Vector(x, 0, z);
            // SDL_Log("pos=(%f,%f,%f)", pos.x, pos.y, pos.z);
            const Object obj = {cubeMesh.get(), chunk.atPosition + Vector(x, 0, z)};

            chunk.blocks.emplace(pos, std::move(obj));

            //min limit is -1 presumably
            for (; y >= -1; y--) {
                chunk.blocks[Vector(x, y, z)] = {cubeMesh.get(), chunk.atPosition + Vector(x, y, z)}; //for now we have either block or no block, to be replaced w enum?
                //chunk.blocks.insert(Object{cubeMesh.get(), chunk.atPosition + Vector(x, y, z)}); //for now we have either block or no block, to be replaced w enum?)
            }
        }
    }

    chunkManager->worldChunks.push_back(std::move(chunk));
    // chunkManager->chunkMap[std::make_pair((int) atPos.x, (int) atPos.z)] = chunkManager->worldChunks.size() - 1;
    chunkManager->chunkMap.insert({atPos.toInt3(), chunkManager->worldChunks.size() - 1});
}

// void App::ConstructChunkAt(int atXPos, int atZPos, bool flat) const {
//     for (size_t x = atXPos; x < atXPos + ChunkManager::chunkSizeXYZ; x++) {
//         for (size_t z = atZPos; z < atZPos + ChunkManager::chunkSizeXYZ; z++) {
//             size_t rawNoiseVal = SimplexNoise::noise(x * noise->mFrequency, z * noise->mFrequency);
//             rawNoiseVal *= noise->mAmplitude;
//
//             size_t y = flat ? 0 : rawNoiseVal;
//             int numChunksToGround = 2; //1 + static_cast<int>((y / ChunkManager::chunkSizeXYZ));
//             int topChunkMaxBLockY = y % ChunkManager::chunkSizeXYZ;
//             for (int y = 0; y < numChunksToGround; y++) {
//                 int chunkY = y * ChunkManager::chunkSizeXYZ;
//                 Chunk<ChunkManager::chunkSizeXYZ> chunk(Vector(atXPos, chunkY, atZPos));
//
//                 //if (y == numChunksToGround) {
//                 //for (int blockY = 0; blockY < topChunkMaxBLockY; blockY += 1) {
//                 //    chunk.blocks[Vector(x, y, z)] = {cubeMesh.get(), Vector(atXPos + x, chunkY + y, atZPos + z)};
//                 //}
//                 //} else {
//                 for (int blockY = 0; blockY < 16; blockY += 1) {
//                     chunk.blocks[Vector(x, y, z)] = {cubeMesh.get(), Vector(atXPos + x, chunkY + y, atZPos + z)};
//                 }
//                 //}
//
//                 chunkManager->worldChunks.push_back(std::move(chunk));
//
//                 //const Object obj = {cubeMesh.get(), chunk.atPosition + Vector(x, 0, z)};
//                 //chunk.blocks.emplace(pos, std::move(obj));
//                 //min limit is -1 presumably
//                 //for (; y >= -1; y--) {
//                 //    chunk.blocks[Vector(x, y, z)] = {cubeMesh.get(), chunk.atPosition + Vector(x, y, z)}; //for now we have either block or no block, to be replaced w enum?
//                 //}
//             }
//         }
//     }
//     // chunkManager->worldChunks.push_back(std::move(chunk));
// }

RaycastHit App::CheckIsPointInsideAny(Vector point) const {
    const Vector worldBlockPosition = BlockPositionFromPoint(point);
    const Int3 chunkLookup = ChunkLookupFromBlockPosition(worldBlockPosition);

    auto iterator = chunkManager->chunkMap.find(chunkLookup); //std::make_pair(chunkX, chunkZ));
    if (iterator != chunkManager->chunkMap.end()) {
        const auto &chunk = chunkManager->worldChunks[iterator->second];
        const Vector blockPosition = worldBlockPosition - chunk.atPosition;

        const auto block = chunk.blocks.find(blockPosition);
        if (block != chunk.blocks.end()) {
            return RaycastHit{
                .hit = true,
                .blockType = true,
                .blockPosition = block->first + chunk.atPosition
            };
        }
    }

    return RaycastHit_NULL;
}

///Following this: https://aaaa.sh/creatures/dda-algorithm-interactive/
RaycastHit App::RaycastRay(Vector pos, Vector normDir, float maxDistance, bool fullDebug) const {
    normDir = normDir.Normalized();
    if (normDir.Magnitude() == 0.f || maxDistance < 0.f) {
        return RaycastHit_NULL;
    }

    Vector signVector = Vector(sign(normDir.x), sign(normDir.y), sign(normDir.z));
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

    if (fullDebug) {
        SDL_Log("Raycast debug:");
        SDL_Log("\tFrom %f,%f,%f", pos.x, pos.y, pos.z);
        SDL_Log("\tMap offset %f,%f,%f", mapCheck.x, mapCheck.y, mapCheck.z);
        SDL_Log("\tWith dir %f,%f,%f", normDir.x, normDir.y, normDir.z);
        SDL_Log("\tRay unit step size %f,%f,%f", rayUnitStepSize.x, rayUnitStepSize.y, rayUnitStepSize.z);
        SDL_Log("\tRay length %f,%f,%f", rayLength.x, rayLength.y, rayLength.z);
    }

    float travelledDistance = 0.f;
    while (travelledDistance <= maxDistance) {
        if (fullDebug)
            SDL_Log("Checking position %f,%f,%f", mapCheck.x, mapCheck.y, mapCheck.z);

        RaycastHit result = CheckIsPointInsideAny(mapCheck);
        if (result.hit) {
            if (fullDebug) SDL_Log("Hit! At %f, %f, %f", mapCheck.x, mapCheck.y, mapCheck.z);
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

Vector App::GetGravityVector() {
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
    Matrix4D viewProjection = projectionMatrix * viewMatrix;

    float float16Array[16];
    viewProjection.toOutFloat16Array(float16Array);

    SDL_PushGPUVertexUniformData(
        commandBuffer,
        0,
        float16Array,
        sizeof(float16Array)
    );

    auto getMeshDrawCallVerticies = [](Mesh *mesh) {
        return mesh->numTriangles > 0 ? mesh->numTriangles * 3 : 0; //count numTriangles cuz triangles is what we render not verticies
    };

    auto isFaceInCameraFrustrum = [](Face *globalFace) {
        if (globalFace == nullptr) return false;

        for (const auto &plane: sceneCamera->frustrumPlanes) {
            auto v1 = globalFace->globalPoint1;
            auto v2 = globalFace->globalPoint2;
            auto v3 = globalFace->globalPoint3;

            //manually check 3 verticies
            float d1 = plane.A * v1.x + plane.B * v1.y + plane.C * v1.z + plane.D;
            float d2 = plane.A * v2.x + plane.B * v2.y + plane.C * v2.z + plane.D;
            float d3 = plane.A * v3.x + plane.B * v3.y + plane.C * v3.z + plane.D;
            if (d1 < 0 && d2 < 0 && d3 < 0) { return false; }
        }
        return true;
    };

    std::atomic<size_t> totalVertexNumber = 0;
    std::atomic<size_t> totalLineVertexNumber = 0;

    auto startTicks = SDL_GetTicks();

    std::vector<std::vector<Face> > chunkLocalFaces(chunkManager->worldChunks.size());

    std::for_each(std::execution::par, chunkManager->worldChunks.begin(), chunkManager->worldChunks.end(), [&](auto &chunk) {
        size_t idx = &chunk - chunkManager->worldChunks.data();
        
        //Check if chunk is in camera frustrum
        Vector chunkBoundsMin = chunk.atPosition;
        Vector chunkBoundsMax = chunk.atPosition + Vector(ChunkManager::chunkSizeXYZ, ChunkManager::chunkSizeXYZ, ChunkManager::chunkSizeXYZ);

        for (const auto &plane: sceneCamera->frustrumPlanes) {
            const float x = (plane.A > 0) ? chunkBoundsMax.x : chunkBoundsMin.x;
            const float y = (plane.B > 0) ? chunkBoundsMax.y : chunkBoundsMin.y;
            const float z = (plane.C > 0) ? chunkBoundsMax.z : chunkBoundsMin.z;
            if (plane.A * x + plane.B * y + plane.C * z + plane.D < 0) return;
        }

        constexpr float halfChunk = ChunkManager::chunkSizeXYZ / 2.0f;
        Vector chunkCenter = chunk.atPosition + Vector(halfChunk, halfChunk, halfChunk);
        Vector distVector = chunkCenter - sceneCamera->Position;
        distVector.y = 0;

        //Distance from chunk center to player
        float distance = distVector.Magnitude();

        auto LOD = chunk.didUserEditChunk ? 0 : GetLevelOfDetailFromDistance(distance);

        int LODBlockSize = std::min(static_cast<int>(std::pow(ChunkManager::smallestChunkSizeLogNumber, LOD)), ChunkManager::chunkSizeXYZ);
        const int maxBlockCountInLOD = LODBlockSize * LODBlockSize * LODBlockSize;

        //SDL_Log("LOD of %i at %f,%f,%f so %i scsln %i", LOD, chunk.atPosition.x, chunk.atPosition.y, chunk.atPosition.z, LODBlockSize, ChunkManager::smallestChunkSizeLogNumber);

        //Ignore empty chunks (f.e sky or ungenerated underground)
        if (chunk.blocks.empty()) return;

        //Prefill a 3D grid for faster access within the chunk
        bool blockPresence[ChunkManager::chunkSizeXYZ][ChunkManager::chunkSizeXYZ][ChunkManager::chunkSizeXYZ]{};
        for (const auto &[pos, obj]: chunk.blocks) {
            int ix = static_cast<int>(pos.x);
            int iy = static_cast<int>(pos.y);
            int iz = static_cast<int>(pos.z);
            if (ix >= 0 && ix < ChunkManager::chunkSizeXYZ &&
                iy >= 0 && iy < ChunkManager::chunkSizeXYZ &&
                iz >= 0 && iz < ChunkManager::chunkSizeXYZ) {
                blockPresence[ix][iy][iz] = true;
            }
        }

        for (int y = 0; y < ChunkManager::chunkSizeXYZ; y += LODBlockSize) {
            for (int x = 0; x < ChunkManager::chunkSizeXYZ; x += LODBlockSize) {
                for (int z = 0; z < ChunkManager::chunkSizeXYZ; z += LODBlockSize) {
                    //Precautions just in case LODBlockSize is not a logarythm of chunkSizeXYZ
                    int currentLODX = std::min(LODBlockSize, ChunkManager::chunkSizeXYZ - x);
                    int currentLODY = std::min(LODBlockSize, ChunkManager::chunkSizeXYZ - y);
                    int currentLODZ = std::min(LODBlockSize, ChunkManager::chunkSizeXYZ - z);

                    int blockCount = 0;
                    for (int yObj = 0; yObj < currentLODY; yObj++) {
                        for (int xObj = 0; xObj < currentLODX; xObj++) {
                            for (int zObj = 0; zObj < currentLODZ; zObj++) {
                                if (blockPresence[x + xObj][y + yObj][z + zObj]) {
                                    blockCount++;
                                }
                            }
                        }
                    }

                    if (blockCount > 0 && (static_cast<float>(blockCount) / maxBlockCountInLOD) > 0.5f) {
                        Vector LODBlockBoundMin = chunk.atPosition + Vector(x, y, z);
                        Vector LODBlockBoundMax = LODBlockBoundMin + Vector(currentLODX, currentLODY, currentLODZ);

                        for (const auto &plane: sceneCamera->frustrumPlanes) {
                            const float x = (plane.A > 0) ? LODBlockBoundMax.x : LODBlockBoundMin.x;
                            const float y = (plane.B > 0) ? LODBlockBoundMax.y : LODBlockBoundMin.y;
                            const float z = (plane.C > 0) ? LODBlockBoundMax.z : LODBlockBoundMin.z;
                            if (plane.A * x + plane.B * y + plane.C * z + plane.D < 0) continue;
                        }

                        const float minX = static_cast<float>(x) - kBlockHalfExtent;
                        const float minY = static_cast<float>(y) - kBlockHalfExtent;
                        const float minZ = static_cast<float>(z) - kBlockHalfExtent;
                        const float maxX = static_cast<float>(x + currentLODX) - kBlockHalfExtent;
                        const float maxY = static_cast<float>(y + currentLODY) - kBlockHalfExtent;
                        const float maxZ = static_cast<float>(z + currentLODZ) - kBlockHalfExtent;

                        Vector v1 = Vector(minX, minY, minZ);
                        Vector v2 = Vector(minX, minY, maxZ);
                        Vector v3 = Vector(maxX, minY, maxZ);
                        Vector v4 = Vector(maxX, minY, minZ);
                        Vector v5 = Vector(minX, maxY, minZ);
                        Vector v6 = Vector(minX, maxY, maxZ);
                        Vector v7 = Vector(maxX, maxY, maxZ);
                        Vector v8 = Vector(maxX, maxY, minZ);

                        std::array<Face, 12> blockFaces = {
                            {
                                {v5, v6, v7}, {v5, v7, v8}, // top, +Y
                                {v1, v3, v2}, {v1, v4, v3}, // bottom, -Y
                                {v4, v8, v7}, {v4, v7, v3}, // right, +X
                                {v1, v2, v6}, {v1, v6, v5}, // left, -X
                                {v2, v3, v7}, {v2, v7, v6}, // front, +Z
                                {v1, v5, v8}, {v1, v8, v4} // back, -Z
                            }
                        };

                        for (auto &blockFace: blockFaces) {
                            blockFace += chunk.atPosition;
                            if (isFaceInCameraFrustrum(&blockFace)) {
                                chunkLocalFaces[idx].push_back(std::move(blockFace));
                                totalVertexNumber.fetch_add(3);
                                totalLineVertexNumber.fetch_add(3 * 2);
                            }
                        }
                    }
                }
            }
        }
    });

    // //For every chunk, get distance beetwen player and its center
    // float halfChunk = ChunkManager::chunkSizeXYZ / 2.0f;
    // Vector chunkCenter = chunk.atPosition + Vector(halfChunk, halfChunk, halfChunk);
    //
    // Vector distVector = chunkCenter - sceneCamera->Position;
    // distVector.y = 0;
    // float distance = distVector.Magnitude();
    //
    // auto LOD = GetLevelOfDetailFromDistance(distance);
    //
    // //LODBlockSize is size of block based on parent chunk LOD
    // int LODBlockSize = std::min(static_cast<int>(std::pow(smallestChunkSizeLogNumber, LOD)), ChunkManager::chunkSizeXYZ);
    //
    // //N is maximum number of blocks in chunk based on LOD
    // int N = std::max(std::pow(ChunkManager::chunkSizeXYZ / LODBlockSize, 3), 1.0);
    // Face *LODFaces = new Face[12 * N]; //Each block has 12 faces
    //     int LODBlockIndex = 0;
    //     for (int y = 0; y < ChunkManager::chunkSizeXYZ; y += LODBlockSize) {
    //         for (int x = 0; x < ChunkManager::chunkSizeXYZ; x += LODBlockSize) {
    //             for (int z = 0; z < ChunkManager::chunkSizeXYZ; z += LODBlockSize) {
    //                 //For every axis, check number of blocks inside given "part" of the chunk that has size of 1 LOD block
    //                 int currentLODX = std::min(LODBlockSize, ChunkManager::chunkSizeXYZ - x);
    //                 int currentLODY = std::min(LODBlockSize, ChunkManager::chunkSizeXYZ - y);
    //                 int currentLODZ = std::min(LODBlockSize, ChunkManager::chunkSizeXYZ - z);
    //
    //                 int blockCount = 0;
    //                 const int maxBlockCount = std::pow(LODBlockSize, 3);
    //
    //                 for (int xObj = 0; xObj < currentLODX; xObj += 1) {
    //                     for (int yObj = 0; yObj < currentLODY; yObj += 1) {
    //                         for (int zObj = 0; zObj < currentLODZ; zObj += 1) {
    //                             if (chunk.blocks.contains(Vector(x + xObj, y + yObj, z + zObj))) {
    //                                 blockCount += 1;
    //                             }
    //                         }
    //                     }
    //                 }
    //
    //                 //If there are more than half of any block in that "part", render 1 LOD block
    //                 if (static_cast<float>(blockCount / maxBlockCount) > 0.5f) {
    //                     int extendX = std::min(LODBlockSize, ChunkManager::chunkSizeXYZ - x);
    //                     int extendY = std::min(LODBlockSize, ChunkManager::chunkSizeXYZ - y);
    //                     int extendZ = std::min(LODBlockSize, ChunkManager::chunkSizeXYZ - z);
    //
    //                     Vector verticie1 = Vector(x, y, z);
    //                     Vector verticie2 = Vector(x, y, z + extendZ);
    //                     Vector verticie3 = Vector(x + extendX, y, z + extendZ);
    //                     Vector verticie4 = Vector(x + extendX, y, z);
    //
    //                     Vector verticie5 = Vector(x, y + extendY, z);
    //                     Vector verticie6 = Vector(x, y + extendY, z + extendZ);
    //                     Vector verticie7 = Vector(x + extendX, y + extendY, z + extendZ);
    //                     Vector verticie8 = Vector(x + extendX, y + extendY, z);
    //
    //                     //top
    //                     LODFaces[LODBlockIndex * 12 + 0] = {verticie1, verticie2, verticie3};
    //                     LODFaces[LODBlockIndex * 12 + 1] = {verticie1, verticie3, verticie4};
    //
    //                     //bottom
    //                     LODFaces[LODBlockIndex * 12 + 2] = {verticie5, verticie6, verticie7};
    //                     LODFaces[LODBlockIndex * 12 + 3] = {verticie5, verticie7, verticie8};
    //
    //                     //left
    //                     LODFaces[LODBlockIndex * 12 + 4] = {verticie1, verticie5, verticie8};
    //                     LODFaces[LODBlockIndex * 12 + 5] = {verticie1, verticie4, verticie8};
    //
    //                     //right
    //                     LODFaces[LODBlockIndex * 12 + 6] = {verticie2, verticie6, verticie7};
    //                     LODFaces[LODBlockIndex * 12 + 7] = {verticie2, verticie3, verticie7};
    //
    //                     //front
    //                     LODFaces[LODBlockIndex * 12 + 8] = {verticie1, verticie5, verticie2};
    //                     LODFaces[LODBlockIndex * 12 + 9] = {verticie2, verticie5, verticie6};
    //
    //                     //back
    //                     LODFaces[LODBlockIndex * 12 + 10] = {verticie3, verticie4, verticie8};
    //                     LODFaces[LODBlockIndex * 12 + 11] = {verticie3, verticie7, verticie8};
    //
    //                     LODBlockIndex += 1;
    //                 }
    //             }
    //         }
    //     }
    //
    //     for (int i = 0; i < LODBlockIndex; i += 1) {
    //         for (int triangleIndex = 0; triangleIndex < 12; triangleIndex += 1) {
    //             Face LODFace = LODFaces[i * 12 + triangleIndex];
    //             LODFace += chunk.atPosition;
    //
    //             if (isFaceInCameraFrustrum(&LODFace)) {
    //                 nonFrustrumCulledFaces.push_back(std::move(LODFace)); //no real effect as Face is trivially copyable but still good to move
    //
    //                 totalVertexNumber += 3;
    //                 totalLineVertexNumber += 3 * 2;
    //             }
    //         }
    //     }
    //
    //     delete[] LODFaces;
    // }

    SDL_Log("Frustrum culling took: %llu ms...", (SDL_GetTicks() - startTicks));
    SDL_Log("Num of verticies total: %zi", totalVertexNumber.load());

    if (deltaTimeMS == 0) {
        SDL_Log("FPS: 0 (0DMS)");
    } else {
        SDL_Log("FPS: %llu", 1000 / deltaTimeMS);
    }

    const size_t lineStartVertex = totalVertexNumber.load();
    const size_t totalUploadedVertexNumber = totalVertexNumber.load() + totalLineVertexNumber.load();
    std::vector<Vertex3D> verticies(totalUploadedVertexNumber);

    const Uint32 vertexDataSize = totalUploadedVertexNumber * sizeof(Vertex3D);
    if (vertexDataSize > 0) {
        //Combine vector of vectors of faces into one vector
        std::vector<Face> nonFrustrumCulledFaces;

        size_t totalFaces = 0;
        for (const auto &vec: chunkLocalFaces) {
            totalFaces += vec.size();
        }

        nonFrustrumCulledFaces.reserve(totalFaces);

        for (auto &vec: chunkLocalFaces) {
            //Move iterator is abstraction of normal iterator that when using dereferencing ( * ) gives T&& instead of T& allowing compiler to do same thing as std::move but to iterator
            nonFrustrumCulledFaces.insert(nonFrustrumCulledFaces.end(),
                                          std::make_move_iterator(vec.begin()),
                                          std::make_move_iterator(vec.end()));
        }

        std::atomic<size_t> cpyIndex{0};

        std::for_each(std::execution::par_unseq, std::begin(nonFrustrumCulledFaces), std::end(nonFrustrumCulledFaces), [&](Face &face) {
            auto drawcallVerts = face.GetFaceDrawCallVerticies();
            auto start = cpyIndex.fetch_add(3);
            memcpy(verticies.data() + start, drawcallVerts.data(), 3 * sizeof(Vertex3D));
        });

        std::for_each(std::execution::par_unseq, std::begin(nonFrustrumCulledFaces), std::end(nonFrustrumCulledFaces), [&](Face &face) {
            auto linecallVerts = face.GetFaceLineCallVerticies();
            auto start = cpyIndex.fetch_add(6);
            memcpy(verticies.data() + start, linecallVerts.data(), 6 * sizeof(Vertex3D));
        });

        if (sceneVertexBuffer == nullptr) {
            SDL_GPUBufferCreateInfo bufferInfo{};
            bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            bufferInfo.size = vertexDataSize;
            bufferInfo.props = 0;
            sceneVertexBuffer = SDL_CreateGPUBuffer(m_gpuDevice.get(), &bufferInfo);
            lastSceneVertexBufferDataSize = vertexDataSize;
            if (!sceneVertexBuffer) {
                SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create scene vertex buffer: %s", SDL_GetError());
                SDL_SubmitGPUCommandBuffer(commandBuffer);
                return FAILURE;
            }
        } else {
            constexpr bool canCreateVertexBufferEveryFrame = true;
            if (canCreateVertexBufferEveryFrame) {
                if (lastSceneVertexBufferDataSize != vertexDataSize) {
                    SDL_GPUBufferCreateInfo bufferInfo{};
                    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                    bufferInfo.size = vertexDataSize;
                    bufferInfo.props = 0;
                    sceneVertexBuffer = SDL_CreateGPUBuffer(m_gpuDevice.get(), &bufferInfo);
                    lastSceneVertexBufferDataSize = vertexDataSize;
                    if (!sceneVertexBuffer) {
                        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create scene vertex buffer: %s", SDL_GetError());
                        SDL_SubmitGPUCommandBuffer(commandBuffer);
                        return FAILURE;
                    }
                }
            }
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.size = vertexDataSize;
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        SDL_GPUTransferBuffer *frameTransferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice.get(), &transferInfo);
        if (!frameTransferBuffer) {
            SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create scene's vertex transfer buffer: %s", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(commandBuffer);
            return FAILURE;
        }

        Vertex3D *mappedData = static_cast<Vertex3D *>(SDL_MapGPUTransferBuffer(m_gpuDevice.get(), frameTransferBuffer, true));
        if (!mappedData) {
            SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to map transfer buffer: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), frameTransferBuffer);
            SDL_SubmitGPUCommandBuffer(commandBuffer);
            return FAILURE;
        }

        SDL_memcpy(mappedData, verticies.data(), vertexDataSize);
        SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), frameTransferBuffer);

        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        if (!copyPass) {
            SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to begin copy pass");
            SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), frameTransferBuffer);
            SDL_SubmitGPUCommandBuffer(commandBuffer);
            return FAILURE;
        }

        SDL_GPUTransferBufferLocation src = {frameTransferBuffer, 0};
        SDL_GPUBufferRegion dst = {sceneVertexBuffer, 0, vertexDataSize};
        SDL_UploadToGPUBuffer(copyPass, &src, &dst, true);
        SDL_EndGPUCopyPass(copyPass);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), frameTransferBuffer);
    }

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

    if (totalUploadedVertexNumber > 0 && sceneVertexBuffer != nullptr) {
        SDL_BindGPUGraphicsPipeline(renderPass, graphicsPipeline);

        //Bind our vertex buffer/s
        SDL_GPUBufferBinding vertexBinding = {sceneVertexBuffer, 0};
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

        SDL_GPUTextureSamplerBinding bindings[2] =
        {
            {colourTexture, colourSampler},
            {normalTexture, normalSampler},
        };

        SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 2);
        SDL_DrawGPUPrimitives(renderPass, totalVertexNumber, 1, 0, 0);
        SDL_BindGPUGraphicsPipeline(renderPass, lineGraphicsPipeline);
        SDL_DrawGPUPrimitives(renderPass, totalLineVertexNumber, 1, lineStartVertex, 0);
    }

    SDL_EndGPURenderPass(renderPass);

    if (!SDL_SubmitGPUCommandBuffer(commandBuffer)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to submit command buffer: %s", SDL_GetError());
        return FAILURE;
    }

    return CONTINUE;
}
