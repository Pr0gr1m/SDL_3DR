#pragma once
#include <memory>
#include <vector>
#include <SDL3/SDL.h>

#include "core/Ray.h"
#include "core/Chunk.h"
#include "core/ChunkManager.h"
#include "core/TextureManager.h"

/**
 * @class App
 * @brief Main application class
 */
class App {
public:
    App(int argc, char **argv);

    ///Initializes application
    SDL_AppResult Init();

    ///Iterates over application
    SDL_AppResult Iterate();

    ///Observer on event
    SDL_AppResult Event(const SDL_Event *event);

    ///Quits application
    void Quit(SDL_AppResult result) const;

    ~App();

    /**
     * @enum ShaderBinaryFormat
     * @brief Describes different compiled shader binaries format
     */
    enum class ShaderBinaryFormat {
        Spirv,
        Dxil, //unused
    };

    /**
     * @struct LoadedShaderBinary
     * @brief Describes a compiled and loaded shader binary
     */
    struct LoadedShaderBinary {
        std::vector<std::byte> bytes;
        SDL_GPUShaderFormat sdlFormat;
    };

    ///Returns a downwards vector / opposite to world up
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

    // static SDL_AppResult OnQuit();

    bool UploadDirtTexturesToGPU();

    SDL_AppResult OnRender();

    SDL_AppResult OnUpdate();

    // void ConstructChunkAt(int, int, bool flat = false) const;
    void ConstructChunkAt(Vector, bool flat = false) const;

    // bool isPointInsideCameraFrustrumView(Vector);

    [[nodiscard]] static int GetLevelOfDetailFromDistance(float distance) {
        auto LOD = static_cast<int>(1.2f * std::ceil(distance / ChunkManager::chunkSizeXYZ));
        if (LOD <= 0) return 1;
        if (LOD > ChunkManager::chunkSizeXYZ) return ChunkManager::chunkSizeXYZ;
        return LOD - 1;
    }

    [[nodiscard]] RaycastHit CheckIsPointInsideAny(Vector) const;

    /**
     * Raycasts a ray using DDA algorythm and returns a result
     * @param origin Ray's orign
     * @param dir Ray's normalized direction
     * @param maxDistance Maximum distance in blocks / roughly
     * @param fullDebug Full debug?
     * @return If success, returns a raycasthit with collision information else returns RaycastHit_NULL
     */
    [[nodiscard]] RaycastHit RaycastRay(Vector origin, Vector dir, float maxDistance = 1, bool fullDebug = false) const;

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
