#pragma once
#include <memory>
#include <vector>
#include <SDL3/SDL.h>

#include "Ray.h"
#include "core/Chunk.h"
#include "core/ChunkManager.h"
#include "core/TextureManager.h"

class App {
public:
    App(int argc, char **argv);

    SDL_AppResult Init();

    SDL_AppResult Iterate();

    SDL_AppResult Event(const SDL_Event *event);

    void Quit(SDL_AppResult result) const;

    ~App();

    void LoadOrCompileShaders() const;

    enum AppLogCategory {
        APP_LOG_CATEGORY_GENERIC = SDL_LOG_CATEGORY_APPLICATION,
        APP_LOG_CATEGORY_VIDEO = SDL_LOG_CATEGORY_VIDEO
    };

    enum class ShaderBinaryFormat {
        Spirv,
        Dxil, //unused
    };

    struct LoadedShaderBinary {
        std::vector<std::byte> bytes;
        SDL_GPUShaderFormat sdlFormat;
    };

    static Vector GetGravityVector();

    Uint64 deltaTimeMS; //KEEP IN MIND - NEED TO DIVIDE BY 1000 TO GET SECONDS
    Uint64 currentMillisecondsSinceStart;

private:
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> m_Window;
    std::unique_ptr<SDL_GPUDevice, decltype(&SDL_DestroyGPUDevice)> m_gpuDevice;

    std::unique_ptr<ChunkManager> chunkManager;
    std::unique_ptr<TextureManager> textureManager;

    Vector cubeVerticies[8] =
    {
        {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f},
        {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}
    };

    Int3 cubeTriangles[12] = {
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

    std::unique_ptr<Mesh> cubeMesh{};

    static SDL_AppResult OnQuit();

    bool UploadDirtTexturesToGPU();

    SDL_AppResult OnRender();

    SDL_AppResult OnUpdate();

    void ConstructChunkAt(Vector, bool flat = false);

    bool isPointInsideCameraFrustrumView(Vector);

    int GetLevelOfDetailFromDistance(float distance) {
        auto LOD = static_cast<int>(1.2f * std::ceil(distance / ChunkManager::chunkSizeXYZ));
        if (LOD <= 0) return 1;
        if (LOD > ChunkManager::chunkSizeXYZ) return ChunkManager::chunkSizeXYZ;
        return LOD - 1;
    }

    RaycastHit CheckIsPointInsideAny(Vector) const;

    RaycastHit RaycastRay(Vector, Vector, float maxDistance = 1) const;

    SDL_GPUBuffer *sceneVertexBuffer = nullptr;
    size_t lastSceneVertexBufferDataSize = 0;

    Uint32 sceneVertexBufferSize = 0;
    SDL_GPUGraphicsPipeline *graphicsPipeline = nullptr;
    SDL_GPUGraphicsPipeline *lineGraphicsPipeline = nullptr;
    SDL_GPUBuffer *uniformBuffer = nullptr;

    SDL_GPUTexture *depthTexture = nullptr;

    SDL_GPUTexture *normalTexture = nullptr;
    SDL_GPUSampler *normalSampler = nullptr;
    SDL_GPUTexture *colourTexture = nullptr;
    SDL_GPUSampler *colourSampler = nullptr;

    char *basePath;

    // std::unique_ptr<Simulation> simulation;

    const char *kVertexShaderPath = "shaders/vertex.spv";
    const char *kFragmentShaderPath = "shaders/fragment.spv";
};
