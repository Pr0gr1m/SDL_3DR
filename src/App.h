#pragma once
#include <memory>
#include <vector>
#include <SDL3/SDL.h>

#include "Ray.h"
#include "core/Chunk.h"
#include "core/Simulation.h"

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
        Dxil,
    };

    struct LoadedShaderBinary {
        std::vector<std::byte> bytes;
        SDL_GPUShaderFormat sdlFormat;
    };

    static constexpr int chunkSizeXYZ = 16; //starting from 0,0,0 it goes to -8,-8,-8 and to 8,8,8

    std::vector<Chunk<chunkSizeXYZ> > worldChunks;
    SDL_GPUBuffer *uniformBuffer = nullptr;

    static Vector GetGravityVector();

private:
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> m_Window;
    std::unique_ptr<SDL_GPUDevice, decltype(&SDL_DestroyGPUDevice)> m_gpuDevice;

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

    Mesh *cubeMesh{};

    SDL_AppResult OnQuit() const;

    SDL_AppResult OnRender();

    SDL_AppResult OnUpdate();

    void ReupdateVertexBuffers();

    void ConstructChunkAt(Vector, bool flat = false);

    RaycastHit CheckIsPointInsideAny(Vector) const;

    RaycastHit RaycastRay(Vector, Vector, float maxDistance = 1) const;

    bool LoadNormalTexture();

    SDL_GPUBuffer *sceneVertexBuffer = nullptr;
    Uint32 sceneVertexBufferSize = 0;
    SDL_GPUGraphicsPipeline *graphicsPipeline = nullptr;
    SDL_GPUGraphicsPipeline *lineGraphicsPipeline = nullptr;

    SDL_GPUTexture *depthTexture = nullptr;

    //TODO: dont have one default normal and colour texture or sampler
    SDL_GPUTexture *normalTexture = nullptr;
    SDL_GPUSampler *normalSampler = nullptr;
    SDL_GPUTexture *colourTexture = nullptr;
    SDL_GPUSampler *colourSampler = nullptr;

    Uint64 deltaTimeMS = 0;
    Uint64 currentMillisecondsSinceStart;

    std::unique_ptr<Simulation> simulation;

    const char *kVertexShaderPath = "shaders/vertex.spv";
    const char *kFragmentShaderPath = "shaders/fragment.spv";
};
