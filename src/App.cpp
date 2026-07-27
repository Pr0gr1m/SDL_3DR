#include "App.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

#include "core/noise/SimplexNoise.h"
#include "core/Vector.h"

#include "Ray.h"
#include "core/Camera.h"
#include "core/Chunk.h"
#include "core/Matrix4D.h"
#include "core/Object.h"
#include "core/Vertex3D.h"

#define CONTINUE SDL_APP_CONTINUE
#define SUCCESS SDL_APP_SUCCESS
#define FAILURE SDL_APP_FAILURE

//POSITION IS FROM -1 TO 1
//COLOR FROM 0 to 1
//N. COORDS FROM 0 TO 1

// static int t = 0;
// static constexpr int kVertexCount = 100000;
// static constexpr Uint32 kVertexBufferSize = kVertexCount * sizeof(Vertex3D);

//To get rid of max vertex count, dynamic buffers should be introduced
//16x16x16 chunk for each vertex buffer?
//and only chunks within range are loaded, at runtime and stored in RAM?

static constexpr float defaultScreenWidth = 1600;
static constexpr float defaultScreenHeight = 900;
static constexpr float aspectRatio = defaultScreenWidth / defaultScreenHeight;

static constexpr float kMouseLookSensitivity = 0.2f;
static constexpr float kMoveSpeed = 1.75f;
static constexpr float kJumpForceMagnitude = 0.3f;
static constexpr float kBlockHalfExtent = 0.5f;
static constexpr float kCameraHeightAboveGround = 1.5f;
static constexpr float kGravityMultiplier = 1;

static SimplexNoise *noise;

Vector startingCameraPos = Vector(0.f, 2.f, 3.f);
Vector degreesCameraEulerAngle = Vector(0.f, 0.f, 0.f);
Camera *sceneCamera = nullptr;

//TODO: Before full release, change CMakeList.txt to put built shaders in build dir, im not sure how building app works here
//TODO: Improve RaycastRay accuracy / reliability
//TODO: Add caching to texture manager, maybe some CMake commands to recache
//TODO: Optimize with diff. cullings
//TODO: Add screen space GI?

namespace {
    Camera BuildCameraFromState() {
        sceneCamera = new Camera(startingCameraPos, degreesCameraEulerAngle.x, degreesCameraEulerAngle.y, degreesCameraEulerAngle.z, aspectRatio);
        return *sceneCamera;
    }

    Vector HorizontalDirection(Vector direction) {
        direction.y = 0.f;
        return direction.Normalized();
    }

    [[deprecated]]
    void MoveCameraLocal(const Vector &localDirection, const float distance) {
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

    const Uint64 start = SDL_GetTicksNS();

    SDL_Log("Initializing SDL library.. %f ms", start / 1000000.0);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not initialize SDL library: %s", SDL_GetError());
        return FAILURE;
    }

    SDL_Log("Initializing SDL Window.. %f ms", start / 1000000.0);
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

    SDL_Log("Initializing SDL GPU device.. %f ms", start / 1000000.0);
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

    SDL_Log("Creating GPU driver device.. %f ms", start / 1000000.0);
    m_gpuDevice.reset(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, false,
        selectedDriver.empty() ? nullptr : selectedDriver.c_str()));

    if (m_gpuDevice == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create GPU device: %s", SDL_GetError());
        return FAILURE;
    }

    SDL_Log("Selected GPU driver: %s", SDL_GetGPUDeviceDriver(m_gpuDevice.get()));
    SDL_Log("Claiming window for GPU device.. %f ms", start / 1000000.0);
    if (!SDL_ClaimWindowForGPUDevice(m_gpuDevice.get(), m_Window.get())) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not claim SDL window to GPU device: %s", SDL_GetError());
        return FAILURE;
    }

    SDL_GPUPresentMode presentMode = SDL_GPU_PRESENTMODE_VSYNC; //VSYNC is supported everywhere, but if mailbox is supported use it as its faster
    if (SDL_WindowSupportsGPUPresentMode(m_gpuDevice.get(), m_Window.get(), SDL_GPU_PRESENTMODE_MAILBOX)) {
        presentMode = SDL_GPU_PRESENTMODE_MAILBOX;
    }

    SDL_Log("Setting GPU swapchain parameters.. %f ms", start / 1000000.0);
    SDL_SetGPUSwapchainParameters(m_gpuDevice.get(), m_Window.get(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR, presentMode);

    //Exit out of build directory
    std::string vPath = std::string(basePath) + "/shaders/vertex.spv";
    std::string fPath = std::string(basePath) + "/shaders/fragment.spv";

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

    SDL_Log("Loading and creating fragment shaders.. %f ms", start / 1000000.0);
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

    SDL_GPUShader *fragmentShader = SDL_CreateGPUShader(m_gpuDevice.get(), &fragmentShaderInfo);

    if (!fragmentShader) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create fragment shader: %s", SDL_GetError());
    }

    SDL_Log("Fragment shader created: %p", fragmentShader);

    SDL_free(fragmentShaderCode);

    SDL_Log("Creating GPU pipelines infos.. %f ms", start / 1000000.0);

    //Create the graphics pipeline info for the TRIANGLE LIST pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST
    };

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

    //Update pipeline info
    pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDesctiptions;

    //Update pipeline info
    pipelineInfo.vertex_input_state.num_vertex_attributes = sizeof(vertexAttributes) / sizeof(SDL_GPUVertexAttribute); //2: position, color
    pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

    //Culling modes (for now None)
    pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

    //Describe the color target
    SDL_GPUColorTargetDescription colorTargetDescriptions{};
    colorTargetDescriptions.blend_state.enable_blend = true;
    colorTargetDescriptions.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    colorTargetDescriptions.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    colorTargetDescriptions.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorTargetDescriptions.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTargetDescriptions.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorTargetDescriptions.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTargetDescriptions.format = SDL_GetGPUSwapchainTextureFormat(m_gpuDevice.get(), m_Window.get());

    //Update pipeline info
    pipelineInfo.target_info.num_color_targets = 1;
    pipelineInfo.target_info.color_target_descriptions = &colorTargetDescriptions;

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
        // Fallback to 24‑bit depth
        depthInfo.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
        depthTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &depthInfo);
        if (!depthTexture) {
            SDL_LogError(APP_LOG_CATEGORY_VIDEO, "Couldn't generate depth texture.");
            return FAILURE;
        }
    }

    pipelineInfo.target_info.has_depth_stencil_target = true;
    pipelineInfo.target_info.depth_stencil_format = depthInfo.format;

    //Create the pipeline for LINE LIST
    SDL_GPUGraphicsPipelineCreateInfo pipelineLineInfo{
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST
    };

    //reuse vertex buffer attributes, descriptions and color target
    pipelineLineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineLineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDesctiptions;

    pipelineLineInfo.vertex_input_state.num_vertex_attributes = sizeof(vertexAttributes) / sizeof(SDL_GPUVertexAttribute);
    pipelineLineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

    pipelineLineInfo.target_info.num_color_targets = 1;
    pipelineLineInfo.target_info.color_target_descriptions = &colorTargetDescriptions;
    pipelineLineInfo.target_info.has_depth_stencil_target = true;
    pipelineLineInfo.target_info.depth_stencil_format = depthInfo.format;

    // Enable depth testing
    pipelineInfo.depth_stencil_state.enable_depth_test = true;
    pipelineInfo.depth_stencil_state.enable_depth_write = true;
    pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    pipelineInfo.depth_stencil_state.compare_mask = 0xFF;
    pipelineInfo.depth_stencil_state.write_mask = 0xFF;

    // For lines you may want:
    pipelineLineInfo.depth_stencil_state.enable_depth_test = true;
    pipelineLineInfo.depth_stencil_state.enable_depth_write = false;
    pipelineLineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    pipelineLineInfo.depth_stencil_state.compare_mask = 0xFF;
    pipelineLineInfo.depth_stencil_state.write_mask = 0xFF;

    SDL_Log("Creating the GPU pipeline.. %f ms", start / 1000000.0);
    graphicsPipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice.get(), &pipelineInfo);

    SDL_Log("Graphics pipeline created: %p", graphicsPipeline);
    lineGraphicsPipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice.get(), &pipelineLineInfo);
    SDL_Log("Line graphics pipeline created: %p", lineGraphicsPipeline);

    if (!graphicsPipeline || !lineGraphicsPipeline) {
        SDL_LogError(APP_LOG_CATEGORY_VIDEO, "Pipeline creation failed");
        return FAILURE;
    }

    // we don't need to store the shaders after creating the pipeline
    SDL_ReleaseGPUShader(m_gpuDevice.get(), vertexShader);
    SDL_ReleaseGPUShader(m_gpuDevice.get(), fragmentShader);

    SDL_WaitForGPUIdle(m_gpuDevice.get());

    BuildCameraFromState();
    sceneCamera->UpdateCameraFrustrumCorners();

    this->cubeMesh = new Mesh(cubeVerticies, std::size(cubeVerticies), cubeTriangles, std::size(cubeTriangles));

    SDL_Log("Creating managers.");
    this->chunkManager = new ChunkManager();
    this->textureManager = new TextureManager(this->basePath);

    //texture manager config - to be replaced with threaded loading system and caching
    this->textureManager->AddEntryForBlockType(TextureManager::Dirt, "img\\dirt\\");
    this->textureManager->AddEntryForBlockType(TextureManager::OakLog, "img\\oak_log\\");

    if (!UploadDirtTexturesToGPU()) {
        return FAILURE;
    }

    noise = new SimplexNoise(0.15f, 3, 0, 0);

    for (int cX = -1; cX <= 1; cX++) {
        for (int cY = -1; cY <= 1; cY++) {
            ConstructChunkAt(Vector(ChunkManager::chunkSizeXYZ * cX, 0, ChunkManager::chunkSizeXYZ * cY));
        }
    }

    SDL_WaitForGPUIdle(m_gpuDevice.get()); //Block rednering and showing window until gpu is idle

    SDL_Log("Showing the window.. %f ms", start / 1000000.0);
    //Creating gpu device in swap chain takes long time, people would see empty or trashed window, thus we show window after some time
    if (!SDL_ShowWindow(m_Window.get())) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not show SDL window: %s", SDL_GetError());
        return FAILURE;
    }

    currentMillisecondsSinceStart = SDL_GetTicks();

    SDL_Log("All done.. %f ms", start / 1000000.0);
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
            return OnQuit();
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (SDL_GetWindowID(m_Window.get()) == event->window.windowID) //For multiple windows
                return OnQuit();
            return CONTINUE;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) {
                return OnQuit();
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
                auto raycastHit = RaycastRay(sceneCamera->Position, GetGravityVector().Normalized(), kCameraHeightAboveGround);
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
                RaycastRay(sceneCamera->Position, Vector(0, -1, 0).Normalized());
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
    (void) result;

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

    if (sceneCamera) {
        delete sceneCamera;
    }

    SDL_ReleaseWindowFromGPUDevice(m_gpuDevice.get(), m_Window.get()); //Destroys window's swapchain texture
}

App::~App() = default;

SDL_AppResult App::OnQuit() {
    return SUCCESS;
}

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

    SDL_Surface *convertedColourSurface = this->textureManager->lodSurfaceFromTexture(TextureManager::Dirt, TextureManager::Colour);
    SDL_Surface *convertedNormalSurface = this->textureManager->lodSurfaceFromTexture(TextureManager::Dirt, TextureManager::Normal);

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
        return false;
    }

    // Transfer buffer
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
        return false;
    }

    Uint8 *colourSourcePixels, *normalSourcePixels;
    colourSourcePixels = static_cast<Uint8 *>(convertedColourSurface->pixels);
    normalSourcePixels = static_cast<Uint8 *>(convertedNormalSurface->pixels);

    const Uint32 normalBytesPerRow = normalTextureInfo.width * BytesPerPixel;
    const Uint32 colourBytesPerRow = colourTextureInfo.width * BytesPerPixel;

    // SDL_Log("%zu", sizeof(colourSourcePixels));
    // SDL_Log("%zu", sizeof(normalSourcePixels));

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

    SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), colourTransferBuffer);
    SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), normalTransferBuffer);

    // Upload
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

void App::ConstructChunkAt(Vector atPos, bool flat) {
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
            const Object obj = {cubeMesh, chunk.atPosition + Vector(x, 0, z)};

            chunk.blocks.emplace(pos, obj);

            //min limit is -1 presumably
            for (; y >= -1; y--) {
                chunk.blocks[Vector(x, y, z)] = {cubeMesh, chunk.atPosition + Vector(x, y, z)}; //for now we have either block or no block, to be replaced w enum?
            }
        }
    }

    chunkManager->worldChunks.push_back(chunk);
}

RaycastHit App::CheckIsPointInsideAny(Vector point) const {
    for (auto &chunk: chunkManager->worldChunks) {
        const Vector localPoint = point - chunk.atPosition;

        const Vector blockPosition(
            std::floor(localPoint.x + kBlockHalfExtent),
            std::floor(localPoint.y + kBlockHalfExtent),
            std::floor(localPoint.z + kBlockHalfExtent)
        );

        if (blockPosition.x < 0 || blockPosition.x >= ChunkManager::chunkSizeXYZ ||
            blockPosition.z < 0 || blockPosition.z >= ChunkManager::chunkSizeXYZ) {
            continue;
        }

        const auto block = chunk.blocks.find(blockPosition);
        if (block != chunk.blocks.end()) {
            return RaycastHit{
                .hit = true,
                .blockType = true,
                .blockPosition = block->first + chunk.atPosition
            };
        }
    }

    return {};
}

RaycastHit App::RaycastRay(Vector pos, Vector normDir, float maxDistance) const {
    static float constexpr stepSize = 0.1f;
    const int maxIterations = static_cast<int>(std::ceil(maxDistance / stepSize));

    Ray newRay;
    newRay.position = pos;
    newRay.direction = normDir;

    for (int i = 0; i < maxIterations; i += 1) {
        newRay.position += newRay.direction * stepSize;
        RaycastHit hit = CheckIsPointInsideAny(newRay.position);

        if (!hit.hit) {
            //std::cout << "No hit at " << newRay.position.x << " " << newRay.position.y << " " << newRay.position.z << std::endl;
        } else {
            //std::cout << "HIT at " << hit.blockPosition.toInt3().a << " " << hit.blockPosition.toInt3().b << " " << hit.blockPosition.toInt3().c << std::endl;
            return hit;
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

    /*
    SDL_Log("PP %f %f %f", sceneCamera->Position.x, sceneCamera->Position.y, sceneCamera->Position.z);

    viewMatrix.toOutFloat16Array(float16Array);
    for (int i = 0; i < 16; i++) {
        SDL_Log("%f", float16Array[i]);
    }

    SDL_Log("");

    projectionMatrix.toOutFloat16Array(float16Array);
    for (int i = 0; i < 16; i++) {
        SDL_Log("%f", float16Array[i]);
    }

    SDL_Log("");

    viewProjection.toOutFloat16Array(float16Array);
    for (int i = 0; i < 16; i++) {
        SDL_Log("%f", float16Array[i]);
    }
    */

    SDL_PushGPUVertexUniformData(
        commandBuffer,
        0,
        float16Array,
        sizeof(float16Array)
    );

    auto getMeshDrawCallVerticies = [](Mesh *mesh) {
        return mesh->numTriangles > 0 ? mesh->numTriangles * 3 : 0; //count numTriangles cuz triangles is what we render not verticies
    };

    // auto isMeshInCamerFrustrum = [](Mesh *mesh, Vector objPosition) {
    //     if (mesh->numVerticies <= 0) return false;
    //     for (int i = 0; i < mesh->numVerticies; i++) {
    //         auto vertex = mesh->verticies[i];
    //         vertex += objPosition;
    //         if (!sceneCamera->IsPointInFrustum(vertex)) { return false; }
    //     }
    //     return true;
    // };

    auto isMeshInCamerFrustrum = [](Mesh *mesh, Vector objPosition) {
        if (mesh->numVerticies == 0) return false;

        for (const auto &plane: sceneCamera->frustrumPlanes) {
            bool allOutside = true;
            for (int i = 0; i < mesh->numVerticies; ++i) {
                Vector v = mesh->verticies[i] + objPosition;
                float d = plane.A * v.x + plane.B * v.y + plane.C * v.z + plane.D;
                if (d >= 0.0f) {
                    // inside (or on) the plane
                    allOutside = false;
                    break;
                }
            }
            if (allOutside) {
                return false; // completely outside this plane → cull
            }
        }
        return true;
    };

    int totalVertexNumber = 0;
    int totalLineVertexNumber = 0;
    std::vector<Object> nonFrustrumCulledObjects; //if this only has visible blocks it should be fine to store in 1 array as there probably wont be that many
    for (auto &chunk: chunkManager->worldChunks) {
        // SDL_Log(". Chunk");
        for (const auto kvp: chunk.blocks) {
            // SDL_Log("\t Block at %f,%f,%f", kvp.second.Position.x, kvp.second.Position.y, kvp.second.Position.z);
            if (isMeshInCamerFrustrum(kvp.second.mesh, kvp.second.Position)) {
                nonFrustrumCulledObjects.push_back(kvp.second);

                auto drawVertexCount = getMeshDrawCallVerticies(kvp.second.mesh);
                totalVertexNumber += drawVertexCount * 1;
                totalLineVertexNumber += drawVertexCount * 2;
                // SDL_Log("\t NT %i NV %i", kvp.second.mesh->numTriangles, kvp.second.mesh->numVerticies);
                // SDL_Log("\t So far TNV %i", totalVertexNumber);
            }
        }
    }

    SDL_Log("Finished fustrum culling. Num of not culled meshes: %i", nonFrustrumCulledObjects.size());
    SDL_Log("Num of verticies total: %i", totalVertexNumber);
    SDL_Log("FPS: %i", 1000 / deltaTimeMS);

    const int lineStartVertex = totalVertexNumber;
    const int totalUploadedVertexNumber = totalVertexNumber + totalLineVertexNumber;
    std::vector<Vertex3D> verticies(totalUploadedVertexNumber);

    const Uint32 vertexDataSize = totalUploadedVertexNumber * sizeof(Vertex3D);
    if (vertexDataSize > 0) {
        int cpyIndex = 0;
        for (auto &object: nonFrustrumCulledObjects) {
            const int drawVertexCount = getMeshDrawCallVerticies(object.mesh);
            Vertex3D *verts = object.GetObjectMeshDrawCallVerticies();

            if (verts != nullptr) {
                memcpy(verticies.data() + cpyIndex, verts, drawVertexCount * sizeof(Vertex3D));
                delete[] verts;
                cpyIndex += drawVertexCount;
            }
        }

        for (auto &object: nonFrustrumCulledObjects) {
            const int lineVertexCount = 2 * getMeshDrawCallVerticies(object.mesh);
            Vertex3D *verts = object.GetObjectLineVerticies();

            if (verts != nullptr) {
                memcpy(verticies.data() + cpyIndex, verts, lineVertexCount * sizeof(Vertex3D));
                delete[] verts; //prevent memory leak
                cpyIndex += lineVertexCount;
            }
        }

        if (!sceneVertexBuffer || sceneVertexBufferSize < vertexDataSize) {
            if (sceneVertexBuffer) {
                SDL_ReleaseGPUBuffer(m_gpuDevice.get(), sceneVertexBuffer);
            }

            SDL_GPUBufferCreateInfo bufferInfo{};
            bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            bufferInfo.size = vertexDataSize;
            bufferInfo.props = 0;
            sceneVertexBuffer = SDL_CreateGPUBuffer(m_gpuDevice.get(), &bufferInfo);
            if (!sceneVertexBuffer) {
                SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create scene vertex buffer: %s", SDL_GetError());
                SDL_SubmitGPUCommandBuffer(commandBuffer);
                return FAILURE;
            }

            sceneVertexBufferSize = vertexDataSize;
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
    colorTargetInfo.clear_color = (SDL_FColor){0.1f, 0.1f, 0.2f, 1.0f}; // dark blue-grey
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo depthTarget = {};
    depthTarget.texture = this->depthTexture;
    depthTarget.clear_depth = 1.0f; // far plane value
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
