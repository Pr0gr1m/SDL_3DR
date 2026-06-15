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

static int t = 0;
// static constexpr int kVertexCount = 100000;
// static constexpr Uint32 kVertexBufferSize = kVertexCount * sizeof(Vertex3D);

//To get rid of max vertex count, dynamic buffers should be introduced
//16x16x16 chunk for each vertex buffer?
//and only chunks within range are loaded, at runtime and stored in RAM?

// static constexpr int chunkSizeXYZ = 16; //starting from 0,0,0 it goes to -8,-8,-8 and to 8,8,8
// static constexpr Uint32 chunkVertexBufferSize = chunkSizeXYZ * chunkSizeXYZ * chunkSizeXYZ * sizeof(Vertex3D); //not optimized, as there will ALWAYS be less verticies with f. culling, greedy meshing etc

static constexpr float screenWidth = 1600;
static constexpr float screenHeight = 900;
static constexpr float aspectRatio = screenWidth / screenHeight;

static constexpr float kMouseLookSensitivity = 0.2f;
static constexpr float kMoveSpeed = 0.15f;
static constexpr float kJumpForceMagnitude = 1;
static constexpr float kBlockHalfExtent = 0.5f;
static constexpr float kCameraHeightAboveGround = 1.f;
static constexpr float kGroundSnapDistance = 0.55f;
static constexpr float kGravityMultiplier = 0.75f; //prevent instant snapping to ground

static std::string pathToNormalTexture = "src\\img\\dirtNormal.jpg";

Vector startingCameraPos = Vector(-1.f, 15.f, 0.f);
Vector degreesCameraEulerAngle = Vector(0.f, 0.f, 0.f);
Camera *sceneCamera = nullptr;

SimplexNoise *noise;

//TODO: Replace fixed size of numObjectsInScene, maybe predict with gen. algorythm number of naturally gen. blocks and have a vector/map of player placed objects? / Or a very big array
//TODO: Also replace Simulation class as its unecessary since I could just refactor everything into Object.h class? (ignoring that definitions are there whatever)
//TODO: Also find out why adding normals reduced fps to like 30 from 500. I could cache sampler and texture info for normal texture but im not sure how to do it

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
            MoveCameraHorizontal(Vector(0.f, 0.f, 1.f), kMoveSpeed);
        }

        if (sceneCamera->GetMoveState(Camera::Backward)) {
            MoveCameraHorizontal(Vector(0.f, 0.f, -1.f), kMoveSpeed);
        }

        if (sceneCamera->GetMoveState(Camera::Left)) {
            MoveCameraHorizontal(Vector(-1.f, 0.f, 0.f), kMoveSpeed);
        }

        if (sceneCamera->GetMoveState(Camera::Right)) {
            MoveCameraHorizontal(Vector(1.f, 0.f, 0.f), kMoveSpeed);
        }

        if (sceneCamera->GetMoveState(Camera::Up)) {
            //MoveCameraLocal(Vector(0.f, 1.f, 0.f), kMoveSpeed);
            MoveCameraWithForce(App::GetGravityVector().Normalized().Negated(), kJumpForceMagnitude);
        }

        if (sceneCamera->GetMoveState(Camera::Down)) {
            //MoveCameraLocal(Vector(0.f, -1.f, 0.f), kMoveSpeed);
        }
    }
}

App::App(int argc, char **argv) : m_Window(nullptr, &SDL_DestroyWindow), m_gpuDevice(nullptr, &SDL_DestroyGPUDevice) {
}

App::~App() = default;

SDL_AppResult App::Init() {
    SDL_SetAppMetadata("2DRenderer", "1.0.0", "com.cozyprogramming.renderer2d");

    const Uint64 start = SDL_GetTicksNS();

    //SDL_Log("Initializing IMG library.. %f ms", start / 1000000.0);


    SDL_Log("Initializing SDL library.. %f ms", start / 1000000.0);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not initialize SDL library: %s", SDL_GetError());
        return FAILURE;
    }

    SDL_Log("Initializing SDL Window.. %f ms", start / 1000000.0);
    m_Window.reset(SDL_CreateWindow("SDL1", screenWidth, screenHeight, SDL_WINDOW_HIDDEN));

    SDL_SetWindowRelativeMouseMode(m_Window.get(), true); //Fullscreen mode

    if (!m_Window) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not initialize SDL window: %s", SDL_GetError());
        return FAILURE;
    }

    const std::array preferredDrives
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

    std::string prefferedDriver;
    for (const auto driver: preferredDrives) {
        if (std::ranges::find(gpuDrivers, driver) != gpuDrivers.end()) {
            SDL_Log("Using preffered driver: %s", driver.c_str());
            prefferedDriver = driver;
            break;
        }
    }

    m_gpuDevice.reset(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, false,
        prefferedDriver.empty() ? nullptr : prefferedDriver.c_str()));

    if (!m_gpuDevice) {
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

    SDL_Log("Loading and creating vertex shaders.. %f ms", start / 1000000.0);
    size_t vertexShaderCodeSize;
    void *vertexShaderCode = SDL_LoadFile(kVertexShaderPath, &vertexShaderCodeSize);

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
    SDL_free(vertexShaderCode);

    SDL_Log("Loading and creating fragment shaders.. %f ms", start / 1000000.0);
    size_t fragmentShaderCodeSize;
    void *fragmentShaderCode = SDL_LoadFile(kFragmentShaderPath, &fragmentShaderCodeSize);

    SDL_GPUShaderCreateInfo fragmentShaderInfo{
        .code_size = fragmentShaderCodeSize,
        .code = static_cast<Uint8 *>(fragmentShaderCode),
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 1,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 0,
        .props = 0
    };

    SDL_GPUShader *fragmentShader = SDL_CreateGPUShader(m_gpuDevice.get(), &fragmentShaderInfo);
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
    depthInfo.width = screenWidth;
    depthInfo.height = screenHeight;
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
    lineGraphicsPipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice.get(), &pipelineLineInfo);

    if (!graphicsPipeline || !lineGraphicsPipeline) {
        SDL_LogError(APP_LOG_CATEGORY_VIDEO, "Pipeline creation failed");
        return FAILURE;
    }

    // we don't need to store the shaders after creating the pipeline
    SDL_ReleaseGPUShader(m_gpuDevice.get(), vertexShader);
    SDL_ReleaseGPUShader(m_gpuDevice.get(), fragmentShader);

    if (!LoadNormalTexture()) {
        return FAILURE;
    }

    SDL_Log("Initalizing custom components.. %f ms", start / 1000000.0);

    /*
    //for some reason x is y and y is x
    // static Vector cubeVerticies[] =
    // {
    //     {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f},
    //     {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}
    // };

    static Int3 cubeTriangles[] = {
        // Top
        {0, 1, 2}, {0, 2, 3},
        // Front
        {0, 5, 1}, {0, 4, 5},
        // Right
        {0, 3, 7}, {0, 7, 4},
        // Back
        {3, 2, 6}, {3, 6, 7},
        // Left
        {1, 5, 6}, {1, 6, 2},
        // Bottom
        {4, 7, 6}, {4, 6, 5}
    };

    Mesh cubeMesh(cubeVerticies, std::size(cubeVerticies), cubeTriangles, std::size(cubeTriangles));

    // Vector rot(0, 0, 0);
    // Vector rotRadians(rot.x / 6.28f, rot.y / 6.28f, rot.z / 6.28f); //0.785375
    std::vector<Int3> objects;
    for (int x = -10; x < 10; x++) {
        for (int z = -10; z < 10; z++) {
            // Object cubeObject(cubeMesh, Vector(0, 0, 0), Vector(0, 0, 0));
            // simulation->RegisterObjectInScene(cubeObject);
            objects.push_back(Int3(x, 0, z));
        }
    }
    */

    BuildCameraFromState();
    currentMillisecondsSinceStart = SDL_GetTicks();

    cubeMesh = new Mesh(cubeVerticies, std::size(cubeVerticies), cubeTriangles, std::size(cubeTriangles));
    simulation = std::make_unique<Simulation>(chunkSizeXYZ * chunkSizeXYZ * chunkSizeXYZ);

    noise = new SimplexNoise(0.15f, 3, 0, 0);
    ConstructChunkAt(Vector(0, 0, 0));
    ConstructChunkAt(Vector(0, 0, 16));
    ConstructChunkAt(Vector(0, 0, -16));
    ConstructChunkAt(Vector(16, 0, 0));
    ConstructChunkAt(Vector(-16, 0, 0));
    ConstructChunkAt(Vector(-16, 0, -16));

    //ConstructChunkAt(Vector(-16, 0, 16));
    //ConstructChunkAt(Vector(16, 0, -16));
    //ConstructChunkAt(Vector(16, 0, 16));

    ReupdateVertexBuffers();

    SDL_Log("Showing the window.. %f ms", start / 1000000.0);
    //Creating gpu device in swap chain takes long time, people would see empty or trashed window, thus we show window after some time
    if (!SDL_ShowWindow(m_Window.get())) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not show SDL window: %s", SDL_GetError());
        return FAILURE;
    }

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
                // MoveCameraLocal(Vector(0.f, .2f, 0.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Up, true);
            }

            if (event->key.key == SDLK_LSHIFT) {
                // MoveCameraLocal(Vector(0.f, -.2f, 0.f), kCameraMoveStep);
                sceneCamera->SetMoveState(Camera::Down, true);
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
            degreesCameraEulerAngle.y += static_cast<float>(event->motion.xrel) * kMouseLookSensitivity;
            degreesCameraEulerAngle.x -= static_cast<float>(event->motion.yrel) * kMouseLookSensitivity;
            degreesCameraEulerAngle.x = std::clamp(degreesCameraEulerAngle.x, -89.0f, 89.0f);

            RotateCameraLocal(degreesCameraEulerAngle);

        default:
            return CONTINUE;
    }
    return CONTINUE;
}

void App::Quit(SDL_AppResult result) const {
    (void) result; // ?????
    SDL_WaitForGPUIdle(m_gpuDevice.get()); //Block thread until GPU is idle

    if (sceneVertexBuffer) {
        SDL_ReleaseGPUBuffer(m_gpuDevice.get(), sceneVertexBuffer);
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

    SDL_ReleaseWindowFromGPUDevice(m_gpuDevice.get(), m_Window.get()); //Destroys window's swapchains
}

SDL_AppResult App::OnQuit() const {
    return SUCCESS;
}

bool App::LoadNormalTexture() {
    char *basePath = const_cast<char *>(SDL_GetBasePath());
    if (!basePath) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "SDL_GetBasePath failed: %s", SDL_GetError());
        return false;
    }

    std::string pathToNormal = std::string(basePath) + pathToNormalTexture;
    SDL_free(basePath);

    // SDL_Surface *loadedSurface = IMG_LoadJPG_IO(pathToNormal.c_str());
    SDL_IOStream *surfaceIOStream = SDL_IOFromFile(pathToNormal.c_str(), "r");
    SDL_Surface *loadedSurface = IMG_LoadJPG_IO(surfaceIOStream);
    SDL_CloseIO(surfaceIOStream);
    if (!loadedSurface) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to load normal texture %s: %s", pathToNormal.c_str(), SDL_GetError());
        return false;
    }

    SDL_Surface *rgbaSurface = SDL_ConvertSurface(loadedSurface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loadedSurface);
    if (!rgbaSurface) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to convert normal texture to RGBA32: %s", SDL_GetError());
        return false;
    }

    SDL_GPUTextureCreateInfo textureInfo{};
    textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    textureInfo.width = rgbaSurface->w;
    textureInfo.height = rgbaSurface->h;
    textureInfo.layer_count_or_depth = 1;
    textureInfo.num_levels = 1;
    textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    normalTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &textureInfo);
    if (!normalTexture) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create normal GPU texture: %s", SDL_GetError());
        SDL_DestroySurface(rgbaSurface);
        return false;
    }

    const Uint32 bytesPerPixel = 4;
    const Uint32 uploadSize = textureInfo.width * textureInfo.height * bytesPerPixel;
    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.size = uploadSize;
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice.get(), &transferInfo);
    if (!transferBuffer) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create normal texture transfer buffer: %s", SDL_GetError());
        SDL_DestroySurface(rgbaSurface);
        return false;
    }

    if (SDL_MUSTLOCK(rgbaSurface) && !SDL_LockSurface(rgbaSurface)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to lock normal texture surface: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);
        SDL_DestroySurface(rgbaSurface);
        return false;
    }

    auto *mappedData = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(m_gpuDevice.get(), transferBuffer, false));
    if (!mappedData) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to map normal texture transfer buffer: %s", SDL_GetError());
        if (SDL_MUSTLOCK(rgbaSurface)) {
            SDL_UnlockSurface(rgbaSurface);
        }
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);
        SDL_DestroySurface(rgbaSurface);
        return false;
    }

    const auto *sourcePixels = static_cast<const Uint8 *>(rgbaSurface->pixels);
    const Uint32 rowBytes = textureInfo.width * bytesPerPixel;
    for (Uint32 y = 0; y < textureInfo.height; ++y) {
        SDL_memcpy(mappedData + (y * rowBytes), sourcePixels + (y * rgbaSurface->pitch), rowBytes);
    }

    SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);
    if (SDL_MUSTLOCK(rgbaSurface)) {
        SDL_UnlockSurface(rgbaSurface);
    }
    SDL_DestroySurface(rgbaSurface);

    SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(m_gpuDevice.get());
    if (!cmdBuf) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to acquire command buffer for normal texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);
        return false;
    }

    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuf);
    if (!copyPass) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to begin normal texture copy pass: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);
        SDL_SubmitGPUCommandBuffer(cmdBuf);
        return false;
    }

    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = transferBuffer;
    source.offset = 0;
    source.pixels_per_row = textureInfo.width;
    source.rows_per_layer = textureInfo.height;

    SDL_GPUTextureRegion destination{};
    destination.texture = normalTexture;
    destination.mip_level = 0;
    destination.layer = 0;
    destination.x = 0;
    destination.y = 0;
    destination.z = 0;
    destination.w = textureInfo.width;
    destination.h = textureInfo.height;
    destination.d = 1;

    SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);

    if (!SDL_SubmitGPUCommandBuffer(cmdBuf)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to submit normal texture upload: %s", SDL_GetError());
        return false;
    }

    SDL_GPUSamplerCreateInfo samplerInfo{};
    samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerInfo.max_lod = 1.f;

    normalSampler = SDL_CreateGPUSampler(m_gpuDevice.get(), &samplerInfo);
    if (!normalSampler) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create normal texture sampler: %s", SDL_GetError());
        return false;
    }

    return true;
}

SDL_AppResult App::OnUpdate() {
    deltaTimeMS = SDL_GetTicks() - currentMillisecondsSinceStart;
    currentMillisecondsSinceStart = SDL_GetTicks();

    sceneCamera->MoveCameraBasedOnVelocity();

    auto hit = RaycastRay(sceneCamera->Position, GetGravityVector().Normalized(), kGroundSnapDistance);
    if (hit.hit) {
        sceneCamera->ResetVelocityAlongAxis(GetGravityVector().Normalized());
        sceneCamera->Position.y = hit.blockPosition.y + kBlockHalfExtent + kCameraHeightAboveGround;
    } else {
        sceneCamera->AddForceThisTick(((GetGravityVector() * deltaTimeMS) / 1000) / 2);
    }

    return CONTINUE;
}

void App::ReupdateVertexBuffers() {
    //Clear objects, and add new ones from chunks?
    simulation->ClearObjects();
    for (auto &chunk: worldChunks) {
        for (const auto &[pos, isSolid]: chunk.blocks) {
            if (isSolid) {
                Object e{cubeMesh, chunk.atPosition + pos};
                simulation->RegisterObjectInScene(e);
            }
        }
    }
}

void App::ConstructChunkAt(Vector atPos, bool flat) {
    Chunk<chunkSizeXYZ> chunk(atPos);

    for (int x = 0; x < chunkSizeXYZ; x++) {
        for (int z = 0; z < chunkSizeXYZ; z++) {
            float rawNoiseVal = noise->noise(x * noise->mFrequency, z * noise->mFrequency);
            //for now, just make it so it goes from 0,1 instead of -1, 1
            rawNoiseVal += 0.5f;
            rawNoiseVal /= 2;

            rawNoiseVal *= noise->mAmplitude;

            // int y = flat ? 0 : (int) (noise->mAmplitude * rawNoiseVal);
            int y = flat ? 0 : (int) rawNoiseVal;

            //min limit is -1 presumably
            for (; y >= -1; y--) {
                chunk.blocks[Vector(x, y, z)] = true; //for now we have either block or no block, to be replaced w enum?
            }
        }
    }

    worldChunks.push_back(chunk);
}

RaycastHit App::CheckIsPointInsideAny(Vector point) const {
    for (auto &chunk: worldChunks) {
        const Vector localPoint = point - chunk.atPosition;

        const Vector blockPosition(
            std::floor(localPoint.x + kBlockHalfExtent),
            std::floor(localPoint.y + kBlockHalfExtent),
            std::floor(localPoint.z + kBlockHalfExtent)
        );

        if (blockPosition.x < 0 || blockPosition.x >= chunkSizeXYZ ||
            blockPosition.z < 0 || blockPosition.z >= chunkSizeXYZ) {
            continue;
        }

        const auto block = chunk.blocks.find(blockPosition);
        if (block != chunk.blocks.end() && block->second) {
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
    return Vector(0, -1, 0) * 9.81f * kGravityMultiplier;
}

SDL_AppResult App::OnRender() {
    SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(m_gpuDevice.get());
    if (!cmdBuf) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to acquire command buffer: %s", SDL_GetError());
        return FAILURE;
    }

    t += 1;

    //Push view matrix as uniform vertex data
    //pitch - up/down yaw - left/right

    // float r = 3;
    // int xIncreaseMult = (camZ > 0) ? 1 : -1;
    // int zIncreaseMult = (camX > 0) ? -1 : 1;

    // camX += (xIncreaseMult * t) / 100000000.00;
    // camZ += (zIncreaseMult * t) / 100000000.00;

    // std::cout << camX << " " << camZ << std::endl;

    // Camera camera({static_cast<float>(camX), 0, static_cast<float>(camZ)}, 0, 0, 0, aspectRatio);
    // simulation->objectsInScene[0].Position += Vector(0, 0, t / 1280000.f);
    // simulation->objectsInScene[0].rotationAngleRadians += Vector(0, t / 3200000.f, 0);
    // simulation->objectsInScene[0].RecalculateRotationMatrix();
    // std::cout << simulation->objectsInScene[0].rotationAngleRadians.z << std::endl;

    Camera camera = *sceneCamera;
    Matrix4D viewMatrix = camera.GetViewMatrix();
    Matrix4D projectionMatrix = camera.GetProjectionMatrix();

    float float16Array[16];
    Matrix4D viewProjection = projectionMatrix * viewMatrix;
    viewProjection.toOutFloat16Array(float16Array);

    SDL_PushGPUVertexUniformData(cmdBuf, 0, float16Array, sizeof(float16Array));
    //
    // simulation->objectsInScene[0].rotationAngleRadians += Vector(0.f, 0.0001f, 0);
    // simulation->objectsInScene[0].RecalculateRotationMatrix();
    //
    // simulation->objectsInScene[1].rotationAngleRadians += Vector(0.f, 0.0001f, 0);
    // simulation->objectsInScene[1].RecalculateRotationMatrix();
    //
    // simulation->objectsInScene[2].rotationAngleRadians += Vector(0.f, 0.0001f, 0);
    // simulation->objectsInScene[2].RecalculateRotationMatrix();
    //
    // simulation->objectsInScene[3].rotationAngleRadians += Vector(0.f, 0.0001f, 0);
    // simulation->objectsInScene[3].RecalculateRotationMatrix();

    auto getDrawVertexCount = [](Mesh *mesh) {
        return mesh->numTriangles > 0 ? mesh->numTriangles * 3 : mesh->numVerticies;
    };

    auto getLineVertexCount = [](Mesh *mesh) {
        return mesh->numTriangles > 0 ? mesh->numTriangles * 6 : mesh->numVerticies * 2;
    };

    int totalVertexNumber = 0;
    int totalLineVertexNumber = 0;

    for (int i = 0; i < simulation->registerObjectIndex; i += 1) {
        totalVertexNumber += getDrawVertexCount(simulation->objectsInScene[i].mesh);
        totalLineVertexNumber += getLineVertexCount(simulation->objectsInScene[i].mesh);
    }

    const int lineStartVertex = totalVertexNumber;
    const int totalUploadedVertexNumber = totalVertexNumber + totalLineVertexNumber;
    std::vector<Vertex3D> verticies(totalUploadedVertexNumber);

    //verticies for both objects and lines
    int cpyIndex = 0;
    for (int i = 0; i < simulation->registerObjectIndex; ++i) {
        Object o = simulation->objectsInScene[i];
        if (o.mesh != nullptr) {
            const int drawVertexCount = getDrawVertexCount(o.mesh);
            Vertex3D *verts = o.GetObjectDrawCallVerticies();
            if (verts != nullptr) {
                memcpy(verticies.data() + cpyIndex, verts, drawVertexCount * sizeof(Vertex3D));
                delete[] verts; // prevent memory leak
                cpyIndex += drawVertexCount;
            }
        }
    }

    for (int i = 0; i < simulation->registerObjectIndex; ++i) {
        Object o = simulation->objectsInScene[i];

        if (o.mesh != nullptr) {
            const int lineVertexCount = getLineVertexCount(o.mesh);
            Vertex3D *verts = o.GetObjectLineVerticies();
            if (verts != nullptr) {
                memcpy(verticies.data() + cpyIndex, verts, lineVertexCount * sizeof(Vertex3D));
                delete[] verts; //prevent memory leak
                cpyIndex += lineVertexCount;
            }
        }
    }

    const Uint32 vertexDataSize = totalUploadedVertexNumber * sizeof(Vertex3D);
    if (vertexDataSize > 0) {
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
                SDL_SubmitGPUCommandBuffer(cmdBuf);
                return FAILURE;
            }

            sceneVertexBufferSize = vertexDataSize;
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.size = vertexDataSize;
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        SDL_GPUTransferBuffer *frameTransferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice.get(), &transferInfo);
        if (!frameTransferBuffer) {
            SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create transfer buffer: %s", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(cmdBuf);
            return FAILURE;
        }

        Vertex3D *mappedData = static_cast<Vertex3D *>(SDL_MapGPUTransferBuffer(m_gpuDevice.get(), frameTransferBuffer, false));
        if (!mappedData) {
            SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to map transfer buffer: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), frameTransferBuffer);
            SDL_SubmitGPUCommandBuffer(cmdBuf);
            return FAILURE;
        }

        SDL_memcpy(mappedData, verticies.data(), vertexDataSize);
        SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), frameTransferBuffer);

        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuf);
        if (!copyPass) {
            SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to begin copy pass");
            SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), frameTransferBuffer);
            SDL_SubmitGPUCommandBuffer(cmdBuf);
            return FAILURE;
        }

        SDL_GPUTransferBufferLocation src = {frameTransferBuffer, 0};
        SDL_GPUBufferRegion dst = {sceneVertexBuffer, 0, vertexDataSize};
        SDL_UploadToGPUBuffer(copyPass, &src, &dst, true);
        SDL_EndGPUCopyPass(copyPass);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), frameTransferBuffer);
    }

    // ========== END: UPDATE VERTEX DATA ==========

    // 4. Acquire the swapchain texture for rendering
    SDL_GPUTexture *swapchainTexture = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuf, m_Window.get(), &swapchainTexture, nullptr, nullptr)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not acquire swapchain texture: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmdBuf);
        return FAILURE;
    }
    if (!swapchainTexture) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Swapchain texture is null");
        SDL_SubmitGPUCommandBuffer(cmdBuf);
        return FAILURE;
    }

    // 5. Set up the colour target info for the render pass
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

    // Begin render pass
    SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTargetInfo, 1, &depthTarget);
    if (!renderPass) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to begin render pass");
        SDL_SubmitGPUCommandBuffer(cmdBuf);
        return FAILURE;
    }

    // 6. Bind the graphics pipeline (created in Init)
    SDL_BindGPUGraphicsPipeline(renderPass, graphicsPipeline);

    // 7. Bind our vertex buffer (the one we just updated)
    if (vertexDataSize > 0) {
        SDL_GPUBufferBinding vertexBinding = {sceneVertexBuffer, 0};
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

        SDL_GPUTextureSamplerBinding normalTextureBinding{normalTexture, normalSampler};
        SDL_BindGPUFragmentSamplers(renderPass, 0, &normalTextureBinding, 1);

        SDL_DrawGPUPrimitives(renderPass, totalVertexNumber, 1, 0, 0);

        SDL_BindGPUGraphicsPipeline(renderPass, lineGraphicsPipeline);
        SDL_DrawGPUPrimitives(renderPass, totalLineVertexNumber, 1, lineStartVertex, 0);
    }

    // End render pass
    SDL_EndGPURenderPass(renderPass);

    // 9. Submit everything to the GPU
    if (!SDL_SubmitGPUCommandBuffer(cmdBuf)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to submit command buffer: %s", SDL_GetError());
        return FAILURE;
    }

    return CONTINUE;
}
