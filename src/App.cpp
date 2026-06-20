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

// static constexpr int chunkSizeXYZ = 16; //starting from 0,0,0 it goes to -8,-8,-8 and to 8,8,8
// static constexpr Uint32 chunkVertexBufferSize = chunkSizeXYZ * chunkSizeXYZ * chunkSizeXYZ * sizeof(Vertex3D); //not optimized, as there will ALWAYS be less verticies with f. culling, greedy meshing etc

static constexpr float screenWidth = 1600;
static constexpr float screenHeight = 900;
static constexpr float aspectRatio = screenWidth / screenHeight;

static constexpr float kMouseLookSensitivity = 0.2f;
static constexpr float kMoveSpeed = 0.15f;
static constexpr float kJumpForceMagnitude = 3.f;
static constexpr float kBlockHalfExtent = 0.5f;
static constexpr float kCameraHeightAboveGround = 1.5f;
static constexpr float kGravityMultiplier = 1;

static std::string pathToNormalTexture = "src\\img\\dirtNormal.jpg"; //To be replaced with some sort of TextureManager
static std::string pathToColourTexture = "src\\img\\dirt2c.jpg";
//also maybe the textures should be cached?

Vector startingCameraPos = Vector(0.f, 2.f, -3.f);
Vector degreesCameraEulerAngle = Vector(0.f, 0.f, 0.f);
Camera *sceneCamera = nullptr;

SimplexNoise *noise;

//TODO: Replace fixed size of numObjectsInScene, maybe predict with gen. algorythm number of naturally gen. blocks and have a vector/map of player placed objects? / Or a very big array
//TODO: Also replace Simulation class as its unecessary since I could just refactor everything into Object.h class? (ignoring that definitions are there whatever)
//TODO: Also find out why adding normals reduced fps to like 30 from 500. I could cache sampler and texture info for normal texture but im not sure how to do it
//TODO: Also find a better dirt textures. Or at least find out why and how to prevent tiling 4 on 1 block

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
            MoveCameraWithForce(App::GetGravityVector().Normalized().Negated(), kJumpForceMagnitude);
        }

        if (sceneCamera->GetMoveState(Camera::Down)) {
            MoveCameraWithForce(App::GetGravityVector().Normalized(), kJumpForceMagnitude);
        }
    }
}

App::App(int argc, char **argv) : m_Window(nullptr, &SDL_DestroyWindow), m_gpuDevice(nullptr, &SDL_DestroyGPUDevice) {
}

App::~App() {
};

SDL_AppResult App::Init() {
    SDL_SetAppMetadata("2DRenderer", "1.0.0", "com.cozyprogramming.renderer2d");

    const Uint64 start = SDL_GetTicksNS();

    SDL_Log("Initializing SDL library.. %f ms", start / 1000000.0);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not initialize SDL library: %s", SDL_GetError());
        return FAILURE;
    }

    this->basePath = const_cast<char *>(SDL_GetBasePath());

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

    char *basePath = const_cast<char *>(SDL_GetBasePath());
    std::string vPath = std::string(basePath) + "src/shaders/vertex.spv";
    std::string fPath = std::string(basePath) + "src/shaders/fragment.spv";

    // If it's running from build folder, basePath might point to build.
    // Let's try to see if we can find them in parent dir if size is 0
    size_t vertexShaderCodeSize;
    void *vertexShaderCode = SDL_LoadFile(vPath.c_str(), &vertexShaderCodeSize);
    if (vertexShaderCodeSize == 0) {
        SDL_free(vertexShaderCode);
        vPath = std::string(basePath) + "../src/shaders/vertex.spv";
        vertexShaderCode = SDL_LoadFile(vPath.c_str(), &vertexShaderCodeSize);
    }
    SDL_Log("Loaded vertex shader from %s, size: %zu", vPath.c_str(), vertexShaderCodeSize);
    fflush(stdout);

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
    if (!vertexShader) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create vertex shader: %s", SDL_GetError());
    }
    const char *err = SDL_GetError();
    SDL_Log("SDL Error: %s", err);

    SDL_Log("Vertex shader created: %p", vertexShader);
    fflush(stdout);
    SDL_free(vertexShaderCode);

    SDL_Log("Loading and creating fragment shaders.. %f ms", start / 1000000.0);
    size_t fragmentShaderCodeSize;
    void *fragmentShaderCode = SDL_LoadFile(fPath.c_str(), &fragmentShaderCodeSize);
    if (fragmentShaderCodeSize == 0) {
        SDL_free(fragmentShaderCode);
        fPath = std::string(basePath) + "../src/shaders/fragment.spv";
        fragmentShaderCode = SDL_LoadFile(fPath.c_str(), &fragmentShaderCodeSize);
    }
    SDL_Log("Loaded fragment shader from %s, size: %zu", fPath.c_str(), fragmentShaderCodeSize);
    fflush(stdout);

    if (fragmentShaderCodeSize > 0) {
        SDL_Log("First 50 chars: %.50s", (char *) fragmentShaderCode);
    }

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
    if (!fragmentShader) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create fragment shader: %s", SDL_GetError());
    }
    SDL_Log("Fragment shader created: %p", fragmentShader);
    fflush(stdout);
    SDL_free(fragmentShaderCode);
    SDL_free(basePath);

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

    err = SDL_GetError();
    SDL_Log("SDL Error: %s", err);

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

    if (!UploadDirtTexturesToGPU()) {
        return FAILURE;
    }
    SDL_WaitForGPUIdle(m_gpuDevice.get());

    //SDL_Log("Initalizing custom components.. %f ms", start / 1000000.0);
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
    //ConstructChunkAt(Vector(-16, 0, -16));
    //ConstructChunkAt(Vector(-16, 0, 16));
    //ConstructChunkAt(Vector(16, 0, -16));
    //ConstructChunkAt(Vector(16, 0, 16));

    ReupdateVertexBuffers();

    SDL_WaitForGPUIdle(m_gpuDevice.get()); //Block rednering and showing window until gpu is idle
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

    if (colourTexture) {
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
    }

    if (colourSampler) {
        SDL_ReleaseGPUSampler(m_gpuDevice.get(), colourSampler);
    }

    SDL_ReleaseWindowFromGPUDevice(m_gpuDevice.get(), m_Window.get()); //Destroys window's swapchains
}

SDL_AppResult App::OnQuit() const {
    return SUCCESS;
}

bool App::UploadDirtTexturesToGPU() {
    std::string path = std::string(basePath) + "..\\" + pathToColourTexture;
    SDL_Log("Creating and uploading GPU texture from %s", path.c_str());
    SDL_IOStream *stream = SDL_IOFromFile(path.c_str(), "rb");

    if (stream == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not open texture: %s", path.c_str());
        SDL_Log("Getting the base path again.");
        auto newBasePath = const_cast<char *>(SDL_GetBasePath());
        SDL_Log("Reaccquired base path: %s", newBasePath);

        if (newBasePath == basePath) {
            SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not reaccquire base path. Restart.");
        }

        return false;
    }

    SDL_Surface *surface = IMG_LoadJPG_IO(stream);
    SDL_CloseIO(stream);

    if (surface == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "IMG_LoadJPG_IO failed: %s", SDL_GetError());
        return false;
    }

    //Convert surface's format as allegedly IMG_LoadJPG_IO may return surfaces with (random?) weird pixel formats
    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);

    if (!converted) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "SDL_ConvertSurface failed");
        return false;
    }

    // Create the GPU texture
    SDL_GPUTextureCreateInfo texInfo = {};
    texInfo.type = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texInfo.width = (Uint32) converted->w;
    texInfo.height = (Uint32) converted->h;
    texInfo.layer_count_or_depth = 1;
    texInfo.num_levels = 1;
    texInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    colourTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &texInfo);
    if (colourTexture == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "SDL_CreateGPUTexture failed");
        return false;
    }

    // Transfer buffer
    const Uint32 BytesPerPixel = 4; //8 bits from red, green, blue, alpha channels = 32 bits = 4 bytes
    const Uint32 totalSize = texInfo.height * texInfo.width * BytesPerPixel;

    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.size = totalSize;
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(m_gpuDevice.get(), &transferInfo);
    if (transfer == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Transfer buffer creation failed");
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        return false;
    }

    //Map and copy row by row
    Uint8 *mapped = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(m_gpuDevice.get(), transfer, false));
    if (mapped == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "SDL_MapGPUTransferBuffer failed");
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transfer);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        return false;
    }

    Uint8 *srcPixels;
    srcPixels = static_cast<Uint8 *>(converted->pixels);

    SDL_Log("%i", totalSize);
    SDL_Log("%i", sizeof(srcPixels));

    SDL_memcpy(mapped, srcPixels, totalSize); //since no pitch

    SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), transfer);

    // Upload
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_gpuDevice.get());
    if (!cmd) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Acquire command buffer failed");
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transfer);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        return false;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Begin copy pass failed");
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transfer);
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        SDL_SubmitGPUCommandBuffer(cmd);
        return false;
    }

    SDL_GPUTextureTransferInfo srcInfo = {};
    srcInfo.transfer_buffer = transfer;
    srcInfo.offset = 0;
    srcInfo.pixels_per_row = texInfo.width;
    srcInfo.rows_per_layer = texInfo.height;

    SDL_GPUTextureRegion dstInfo = {};
    dstInfo.texture = colourTexture;
    dstInfo.layer = 0;
    dstInfo.x = 0;
    dstInfo.y = 0;
    dstInfo.z = 0;
    dstInfo.w = texInfo.width; //changing leads to fucking GPU corruptions
    dstInfo.h = texInfo.height;
    dstInfo.d = 1;

    SDL_UploadToGPUTexture(copy, &srcInfo, &dstInfo, false);
    SDL_EndGPUCopyPass(copy);
    SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transfer);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Submit command buffer failed");
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        return false;
    }

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

    if (!colourSampler) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Sampler creation failed");
        SDL_ReleaseGPUTexture(m_gpuDevice.get(), colourTexture);
        return false;
    }

    SDL_Log("Texture loaded successfully: %dx%d", texInfo.width, texInfo.height);
    return true;
}

/*
bool App::UploadDirtTexturesToGPU() {
    char *basePath = const_cast<char *>(SDL_GetBasePath());
    if (!basePath) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Getting base path failed: %s", SDL_GetError());
        return false;
    }

    std::string pathToColour = std::string(basePath) + pathToColourTexture;
    SDL_free(basePath);

    SDL_Log("Loading texture from: %s", pathToColour.c_str());

    SDL_Surface *loadedColourSurface = IMG_Load(pathToColour.c_str());

    if (!loadedColourSurface) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to load colour texture %s: %s", pathToColour.c_str(), SDL_GetError());
        return false;
    }

    SDL_Log("Loaded surface: colour(%dx%d)", loadedColourSurface->w, loadedColourSurface->h);

    SDL_Surface *colourRGBASurface = SDL_ConvertSurface(loadedColourSurface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loadedColourSurface);

    if (!colourRGBASurface) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to convert colour texture to RGBA32: %s", SDL_GetError());
        return false;
    }

    SDL_GPUTextureCreateInfo colourTextureInfo{};
    colourTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    colourTextureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    colourTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    colourTextureInfo.width = colourRGBASurface->w;
    colourTextureInfo.height = colourRGBASurface->h;
    colourTextureInfo.layer_count_or_depth = 1;
    colourTextureInfo.num_levels = 1;
    colourTextureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    colourTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &colourTextureInfo);

    if (!colourTexture) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create colour GPU texture: %s", SDL_GetError());
        SDL_DestroySurface(colourRGBASurface);
        return false;
    }

    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.size = colourRGBASurface->w * colourRGBASurface->h * 4;
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice.get(), &transferInfo);
    if (!transferBuffer) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create transfer buffer: %s", SDL_GetError());
        SDL_DestroySurface(colourRGBASurface);
        return false;
    }

    Uint8 *mappedData = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(m_gpuDevice.get(), transferBuffer, false));
    const Uint8 *srcPixels = static_cast<const Uint8 *>(colourRGBASurface->pixels);

    for (int row = 0; row < colourRGBASurface->h; row++) {
        SDL_memcpy(mappedData + row * colourRGBASurface->w * 4, srcPixels + row * colourRGBASurface->pitch, colourRGBASurface->w * 4);
    }

    SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);
    SDL_DestroySurface(colourRGBASurface);

    SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(m_gpuDevice.get());
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);

    SDL_GPUTextureTransferInfo textureSourceInfo{
        .transfer_buffer = transferBuffer,
        .offset = 0,
        .pixels_per_row = static_cast<Uint32>(colourTextureInfo.width),
        .rows_per_layer = static_cast<Uint32>(colourTextureInfo.height)
    };

    SDL_GPUTextureRegion textureDestinationInfo{
        .texture = colourTexture,
        .w = static_cast<Uint32>(colourTextureInfo.width),
        .h = static_cast<Uint32>(colourTextureInfo.height),
        .d = 1
    };

    SDL_UploadToGPUTexture(copyPass, &textureSourceInfo, &textureDestinationInfo, false);
    SDL_EndGPUCopyPass(copyPass);

    SDL_GPUFence *uploadFence = SDL_SubmitGPUCommandBufferAndAcquireFence(commandBuffer);
    if (uploadFence) {
        SDL_WaitForGPUFences(m_gpuDevice.get(), true, &uploadFence, 1);
        SDL_ReleaseGPUFence(m_gpuDevice.get(), uploadFence);
    }

    SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);

    SDL_GPUSamplerCreateInfo samplerInfo = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .mip_lod_bias = 0.0f,
        .max_anisotropy = 1.0f,
        .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
        .min_lod = 0.0f,
        .max_lod = 0.0f,
        .enable_anisotropy = false,
        .enable_compare = false,
    };

    colourSampler = SDL_CreateGPUSampler(m_gpuDevice.get(), &samplerInfo);
    if (!colourSampler) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create colour sampler: %s", SDL_GetError());
        return false;
    }

    return true;
}

bool App::UploadDirtTexturestoGPU() {
    //Load surfaces
    SDL_Log("Starting UploadDirtTexturestoGPU");

    char *basePath = const_cast<char *>(SDL_GetBasePath());
    if (!basePath) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Getting base path failed: %s", SDL_GetError());
        return false;
    } else {
        //std::cout << "Base path is = " << basePath << std::endl;
    }

    std::string overriteBasePath = "C:\\Users\\szymo\\CLionProjects\\SDL1\\";
    std::string pathToColour = std::string(overriteBasePath) + pathToColourTexture;

    SDL_Log("Path to colour is: %s", pathToColour.c_str());

    SDL_free(basePath);
    SDL_IOStream *iostream = SDL_IOFromFile(pathToColour.c_str(), "rb");

    if (iostream == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not open file %s: %s", pathToColour.c_str(), SDL_GetError());
        return false;
    }

    SDL_Surface *surface = IMG_LoadJPG_IO(iostream);
    SDL_CloseIO(iostream);

    if (surface == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Surface is null %s", SDL_GetError());
        return false;
    }

    SDL_Surface *convertedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);

    if (convertedSurface == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Converted surface resulted in nullptr:  %s", SDL_GetError());
        return false;
    }

    //Create the GPU texture
    SDL_GPUTextureCreateInfo textureInfo{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = static_cast<Uint32>(convertedSurface->w),
        .height = static_cast<Uint32>(convertedSurface->h),
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = 0
    };
    SDL_GPUTexture *gpuTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &textureInfo);
    colourTexture = gpuTexture;

    auto bytesPerPixel = 4; //32 bits from red, green, blue, alpha 8 each = 4 bytes

    //Create a transfer buffer and copy surfaces data into it
    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.size = bytesPerPixel * convertedSurface->w * convertedSurface->h;
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    SDL_Log("Transfer info size is composed of %d, %d, %d", convertedSurface->w, convertedSurface->h, transferInfo.size);

    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice.get(), &transferInfo);

    //Cast to Uint8 in order to be able to shift later on (pitch)
    auto mappedData = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(m_gpuDevice.get(), transferBuffer, false));
    const Uint8 *srcPixels = static_cast<const Uint8 *>(convertedSurface->pixels);

    //Account for pitch. The GPU expects tightly packed rows with no padding.
    for (int row = 0; row < convertedSurface->h; row++) {
        Uint8 *dst = mappedData + row * textureInfo.width * bytesPerPixel;
        const Uint8 *src = srcPixels + row * convertedSurface->pitch;

        //SDL_memcpy is just a macro for memcpy, for some reason
        memcpy(dst, src, bytesPerPixel * textureInfo.width);
        // SDL_memcpy(dst, src, bytesPerPixel * textureInfo.width);
    }

    SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);
    SDL_DestroySurface(convertedSurface);

    //Accquire command buffer and do a copy pass
    SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(m_gpuDevice.get());
    if (commandBuffer == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Command buffer is null:  %s", SDL_GetError());
        return false;
    }

    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    if (copyPass == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Copy pass is null:  %s", SDL_GetError());
        return false;
    }

    //Set up source / dest
    SDL_GPUTextureTransferInfo textureSourceInfo{
        .transfer_buffer = transferBuffer,
        .offset = 0,
        .pixels_per_row = textureInfo.width,
        .rows_per_layer = textureInfo.height
    };

    SDL_GPUTextureRegion textureDestinationInfo{
        .texture = colourTexture,
        .layer = 0,
        .x = 0,
        .y = 0,
        .z = 0,
        .w = textureInfo.width,
        .h = textureInfo.height,
        .d = 1
    };

    //Upload data to GPU texture
    SDL_UploadToGPUTexture(copyPass, &textureSourceInfo, &textureDestinationInfo, false);

    //End copy pass, submit command buffer and release transfer buffer
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);

    //Creatre samplers for the textures
    SDL_GPUSamplerCreateInfo samplerInfo = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST, // No mipmaps, so this is fine
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .mip_lod_bias = 0.0f,
        .max_anisotropy = 1.0f,
        .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
        .min_lod = 0.0f,
        .max_lod = 1.0f,
        .enable_anisotropy = false,
        .enable_compare = false,
        .padding1 = 0,
        .padding2 = 0,
        .props = 0
    };

    colourSampler = SDL_CreateGPUSampler(m_gpuDevice.get(), &samplerInfo);
    if (!colourSampler) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create colour sampler: %s", SDL_GetError());
        return false;
    }

    return true;
}

bool App::UploadDirtTexturesToGPU() {
    char *basePath = const_cast<char *>(SDL_GetBasePath());
    if (!basePath) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Getting base path failed: %s", SDL_GetError());
        return false;
    }

    std::string pathToNormal = std::string(basePath) + pathToNormalTexture;
    std::string pathToColour = std::string(basePath) + pathToColourTexture;

    SDL_free(basePath);

    SDL_Log("Loading textures from: %s and %s", pathToNormal.c_str(), pathToColour.c_str());
    fflush(stdout);

    SDL_IOStream *normalSurfaceStream = SDL_IOFromFile(pathToNormal.c_str(), "rb");
    SDL_IOStream *colourSurfaceStream = SDL_IOFromFile(pathToColour.c_str(), "rb");

    if (!normalSurfaceStream || !colourSurfaceStream) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to open texture files: %s", SDL_GetError());
        return false;
    }

    SDL_Surface *loadedNormalSurface = IMG_LoadJPG_IO(normalSurfaceStream);
    SDL_Surface *loadedColourSurface = IMG_LoadJPG_IO(colourSurfaceStream);

    SDL_CloseIO(normalSurfaceStream);
    SDL_CloseIO(colourSurfaceStream);

    if (!loadedNormalSurface || !loadedColourSurface) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to load normal/colour texture: %s", SDL_GetError());
        return false;
    }

    SDL_Log("Loaded surfaces: normal(%dx%d), colour(%dx%d)", loadedNormalSurface->w, loadedNormalSurface->h, loadedColourSurface->w, loadedColourSurface->h);

    SDL_Surface *normalRGBASurface = SDL_ConvertSurface(loadedNormalSurface, SDL_PIXELFORMAT_RGBA32);
    SDL_Surface *colourRGBASurface = SDL_ConvertSurface(loadedColourSurface, SDL_PIXELFORMAT_RGBA32);

    SDL_DestroySurface(loadedNormalSurface);
    SDL_DestroySurface(loadedColourSurface);

    if (!normalRGBASurface || !colourRGBASurface) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to convert normal/co texture to RGBA32: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTextureCreateInfo normalTextureInfo{};
    normalTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    normalTextureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    normalTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    normalTextureInfo.width = normalRGBASurface->w;
    normalTextureInfo.height = normalRGBASurface->h;
    normalTextureInfo.layer_count_or_depth = 1;
    normalTextureInfo.num_levels = 1;
    normalTextureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTextureCreateInfo colourTextureInfo{};
    colourTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    colourTextureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    colourTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    colourTextureInfo.width = colourRGBASurface->w;
    colourTextureInfo.height = colourRGBASurface->h;
    colourTextureInfo.layer_count_or_depth = 1;
    colourTextureInfo.num_levels = 1;
    colourTextureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    normalTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &normalTextureInfo);
    colourTexture = SDL_CreateGPUTexture(m_gpuDevice.get(), &colourTextureInfo);

    if (!normalTexture || !colourTexture) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create normal/colour GPU texture: %s", SDL_GetError());
        SDL_DestroySurface(normalRGBASurface);
        SDL_DestroySurface(colourRGBASurface);
        return false;
    }

    const Uint32 normalBytesPerPixel = 4; //8 red, 8 blue, 8 green, 8 alpha per pixel = 4 * 8 = 32 bits = 4 bytes since size is in bytes
    const Uint32 colourBytesPerPixel = 4; //same as above

    const Uint32 normalUploadSize = normalTextureInfo.width * normalTextureInfo.height * normalBytesPerPixel;
    const Uint32 colourUploadSize = colourTextureInfo.width * colourTextureInfo.height * colourBytesPerPixel;

    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.size = normalUploadSize + colourUploadSize;
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice.get(), &transferInfo);

    if (!transferBuffer) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create normal/colour texture transfer buffer: %s", SDL_GetError());
        SDL_DestroySurface(normalRGBASurface);
        SDL_DestroySurface(colourRGBASurface);
        return false;
    }

    if (SDL_MUSTLOCK(colourRGBASurface) && !SDL_LockSurface(colourRGBASurface)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to lock colour texture surface: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);
        SDL_DestroySurface(normalRGBASurface);
        SDL_DestroySurface(colourRGBASurface);
        return false;
    }
    if (SDL_MUSTLOCK(normalRGBASurface) && !SDL_LockSurface(normalRGBASurface)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to lock normal texture surface: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);
        SDL_DestroySurface(normalRGBASurface);
        SDL_DestroySurface(colourRGBASurface);
        return false;
    }

    auto *mappedData = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(m_gpuDevice.get(), transferBuffer, false));
    if (!mappedData) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to map normal/colour texture transfer buffer: %s", SDL_GetError());

        if (SDL_MUSTLOCK(normalRGBASurface)) {
            SDL_UnlockSurface(normalRGBASurface);
        }

        if (SDL_MUSTLOCK(colourRGBASurface)) {
            SDL_UnlockSurface(colourRGBASurface);
        }

        SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);
        SDL_DestroySurface(normalRGBASurface);
        SDL_DestroySurface(colourRGBASurface);
        return false;
    }

    const auto *sourceNormalPixels = static_cast<const Uint8 *>(normalRGBASurface->pixels);
    const auto *sourceColourPixels = static_cast<const Uint8 *>(colourRGBASurface->pixels);

    // Ttransfer buffer expects rows with no padding, so we need to memcpy each row individually
    const Uint32 normalBytesPerRow = normalTextureInfo.width * normalBytesPerPixel;
    const Uint32 colourBytesPerRow = colourTextureInfo.width * colourBytesPerPixel;

    //Offset for memcpy is 0
    for (Uint32 y = 0; y < normalTextureInfo.height; ++y) {
        SDL_memcpy(mappedData + (y * normalBytesPerRow),
                   sourceNormalPixels + (y * normalRGBASurface->pitch),
                   normalBytesPerRow);
    }

    //Offset for memcpy is normalDataSize MUST be in bytes
    Uint32 normalDataSize = normalTextureInfo.height * normalBytesPerRow;

    for (Uint32 y = 0; y < colourTextureInfo.height; ++y) {
        SDL_memcpy(mappedData + normalDataSize + (y * colourBytesPerRow),
                   sourceColourPixels + (y * colourRGBASurface->pitch),
                   colourBytesPerRow);
    }

    SDL_UnmapGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);

    if (SDL_MUSTLOCK(normalRGBASurface)) {
        SDL_UnlockSurface(normalRGBASurface);
    }
    if (SDL_MUSTLOCK(colourRGBASurface)) {
        SDL_UnlockSurface(colourRGBASurface);
    }

    SDL_DestroySurface(normalRGBASurface);
    SDL_DestroySurface(colourRGBASurface);

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

    SDL_GPUTextureTransferInfo normalSource{};
    normalSource.transfer_buffer = transferBuffer;
    normalSource.offset = 0;
    normalSource.pixels_per_row = normalTextureInfo.width;
    normalSource.rows_per_layer = normalTextureInfo.height;

    SDL_GPUTextureTransferInfo colourSource{};
    colourSource.transfer_buffer = transferBuffer;
    colourSource.offset = normalDataSize; //mentioned in the SDL_memcpy's
    colourSource.pixels_per_row = colourTextureInfo.width;
    colourSource.rows_per_layer = colourTextureInfo.height;

    SDL_GPUTextureRegion normalDestination{};
    normalDestination.texture = normalTexture;
    normalDestination.mip_level = 0;
    normalDestination.layer = 0;
    normalDestination.x = 0;
    normalDestination.y = 0;
    normalDestination.z = 0;
    normalDestination.w = normalTextureInfo.width;
    normalDestination.h = normalTextureInfo.height;
    normalDestination.d = 1;

    SDL_GPUTextureRegion colourDestination{};
    colourDestination.texture = colourTexture;
    colourDestination.mip_level = 0;
    colourDestination.layer = 0;
    colourDestination.x = 0;
    colourDestination.y = 0;
    colourDestination.z = 0;
    colourDestination.w = colourTextureInfo.width;
    colourDestination.h = colourTextureInfo.height;
    colourDestination.d = 1;

    SDL_UploadToGPUTexture(copyPass, &normalSource, &normalDestination, false);
    SDL_UploadToGPUTexture(copyPass, &colourSource, &colourDestination, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_ReleaseGPUTransferBuffer(m_gpuDevice.get(), transferBuffer);

    if (!SDL_SubmitGPUCommandBuffer(cmdBuf)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to submit normal texture upload: %s", SDL_GetError());
        return false;
    }

    SDL_GPUSamplerCreateInfo normalSamplerInfo{};
    normalSamplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    normalSamplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    normalSamplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    normalSamplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    normalSamplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    normalSamplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    normalSamplerInfo.max_lod = 1000.f;

    SDL_GPUSamplerCreateInfo colourSamplerInfo{};
    colourSamplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    colourSamplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    colourSamplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    colourSamplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    colourSamplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    colourSamplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    colourSamplerInfo.max_lod = 1000.f;

    normalSampler = SDL_CreateGPUSampler(m_gpuDevice.get(), &normalSamplerInfo);
    colourSampler = SDL_CreateGPUSampler(m_gpuDevice.get(), &colourSamplerInfo);

    if (!normalSampler || !colourSampler) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create normal texture sampler: %s", SDL_GetError());
        return false;
    }

    return true;
}
*/

SDL_AppResult App::OnUpdate() {
    deltaTimeMS = SDL_GetTicks() - currentMillisecondsSinceStart;
    currentMillisecondsSinceStart = SDL_GetTicks();

    sceneCamera->MoveCameraBasedOnVelocity();

    if (sceneCamera->Position.y >= 0) {
        sceneCamera->AddForceThisTick((GetGravityVector() * deltaTimeMS / 1000 * kGravityMultiplier));
    } else {
        // SDL_Log("%f", sceneCamera->Position.y);
        sceneCamera->ResetVelocityAlongWorldAxis(Vector(0, 1, 0));
        //sceneCamera->Position.y = 0 + kCameraHeightAboveGround;
    }

    auto hit = RaycastRay(sceneCamera->Position, GetGravityVector().Normalized(), kCameraHeightAboveGround);
    if (hit.hit) {
        sceneCamera->ResetVelocityAlongWorldAxis(GetGravityVector().Normalized());
        sceneCamera->Position.y = hit.blockPosition.y + kBlockHalfExtent + kCameraHeightAboveGround;
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
    return Vector(0, -1, 0) * 9.81f;
}

SDL_AppResult App::OnRender() {
    SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(m_gpuDevice.get());
    if (!commandBuffer) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to acquire command buffer: %s", SDL_GetError());
        return FAILURE;
    }

    Camera camera = *sceneCamera;
    Matrix4D viewMatrix = camera.GetViewMatrix();
    Matrix4D projectionMatrix = camera.GetProjectionMatrix();

    // auto view = Matrix4D(
    //     1, 0, 0, 0,
    //     0, 1, 0, -2,
    //     0, 0, -1, -3,
    //     0, 0, 0, 1
    // );
    //
    // auto proj = Matrix4D(
    //     0.67, 0, 0, 0,
    //     0, 1.19, 0, 0,
    //     0, 0, -1.001, -0.1001,
    //     0, 0, -1, 0
    // );

    float float16Array[16];
    Matrix4D viewProjection = projectionMatrix * viewMatrix;
    viewProjection.toOutFloat16Array(float16Array);

    SDL_PushGPUVertexUniformData(
        commandBuffer,
        0,
        float16Array,
        sizeof(float16Array)
    );

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

    //Verticies for both objects and lines
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
    SDL_GPUTexture *swapchainTexture = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, m_Window.get(), &swapchainTexture, nullptr, nullptr)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Could not acquire swapchain texture: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return FAILURE;
    }

    if (!swapchainTexture) {
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
    if (!renderPass) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to begin render pass");
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return FAILURE;
    }

    // SDL_GPUViewport viewport = {0, 0, (float) screenWidth, (float) screenHeight, 0.0f, 1.0f};
    // SDL_SetGPUViewport(renderPass, &viewport);

    SDL_BindGPUGraphicsPipeline(renderPass, graphicsPipeline);

    //Bind our vertex buffer/s
    if (vertexDataSize > 0) {
        SDL_GPUBufferBinding vertexBinding = {sceneVertexBuffer, 0};
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

        // SDL_Log("colourTexture = %p, colourSa
        SDL_GPUTextureSamplerBinding bindings[2] =
        {
            {colourTexture, colourSampler},
            {normalTexture, normalSampler},
        };

        SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 1);

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
