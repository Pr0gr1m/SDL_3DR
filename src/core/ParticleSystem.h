#ifndef SDL1_PARTICLESYSTEM_H
#define SDL1_PARTICLESYSTEM_H
#include <vector>
#include <SDL3/SDL_gpu.h>

#include "Bounds.h"
#include "Vertex3D.h"

//DEBUG ONLY MODE
#define assertMax(toCheck) assert(toCheck <= maxN);

//6 faces - 2 triangles - 3 verticies
template<size_t maxN, size_t vertsPerParticle = 6 * 2 * 3>
class ParticleSystem {
public:
    virtual ~ParticleSystem() = default; //no need to use destructor, only Dispose()

    std::array<Vector, maxN> particlePosition{};
    size_t nParticles{0}; //visible particles not max particles

    Bounds particleRenderBounds{Vector(1, 1, 1)};

    Vector worldOffset{};
    float scale{0.05f};

    SDL_FColor particleColour{1, 1, 1, 1};

    //One transfer buffer per particle system?
    SDL_GPUTransferBuffer *transferBuffer{nullptr};
    SDL_GPUBuffer *buffer{nullptr};
    SDL_GPUDevice *device{nullptr};

    std::array<Vertex3D, maxN * vertsPerParticle> lastFrameParticleVerticies{};
    bool buffersDirty{true};

    bool toBeDestroyed{false};

    [[nodiscard]] virtual size_t numVerticies() const {
        return nParticles * vertsPerParticle;
    }

    //replace destructor
    virtual void Dispose() = 0;

    void DisposeBuffers() {
        this->toBeDestroyed = true;

        if (this->buffer) SDL_ReleaseGPUBuffer(this->device, this->buffer);
        if (this->transferBuffer) SDL_ReleaseGPUTransferBuffer(this->device, this->transferBuffer);

        this->buffer = nullptr;
        this->transferBuffer = nullptr;
    }

    virtual void UpdateParticleSystem() = 0;

    virtual void InsertParticlesByArray(const Vector *particlesPositions, const Vector *particleUpNormals, const Vector *particleVelocites, const float *ParticleScaleMultipliers, size_t numParticles) = 0;

    virtual void Upload() = 0;

    virtual void TickParticleSystem(Uint64 dT) = 0;
};

#endif //SDL1_PARTICLESYSTEM_H
