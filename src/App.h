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

    //LOD 0: 1 block because n^0 = 1
    //LOD 1: smallesChunkSizeLogNumber because n^1 = n
    //LOD 2: smallesChunkSizeLogNumber^2
    //In conclusion, min LOD is 0 and max LOD is log chunkSizeXYZ with base of smallesChunkSizeLogNumber
    //F.e with chunk size of 16, which is not prime, smallest log number is 2 so max lod is 4 (2^4 = 16)
    //But if chunk size is prime number like 7, smallest log number will be 1 and no LOD effect can be applied, so this can be standalone scenario

    [[nodiscard]] static int GetLevelOfDetailFromDistance(float distance) {
        if (ChunkManager::smallestChunkSizeLogNumber == 1) return 0;
        
        auto LOD = static_cast<int>(std::ceil(1.2f * distance / ChunkManager::chunkSizeXYZ)) - 1; //[0, infinity)
        SDL_Log("For distance %f lod is %i", distance, LOD);
        // if (LOD <= 0) return 1;
        if (std::pow(ChunkManager::smallestChunkSizeLogNumber, LOD) >= ChunkManager::chunkSizeXYZ) //This could be a (f.e hash) table
            return static_cast<int>(log(ChunkManager::chunkSizeXYZ) / log(ChunkManager::smallestChunkSizeLogNumber));
        return LOD;
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
