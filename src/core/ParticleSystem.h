#ifndef SDL1_PARTICLESYSTEM_H
#define SDL1_PARTICLESYSTEM_H
#include <vector>
#include <SDL3/SDL_gpu.h>

#include "Bounds.h"
#include "Vertex3D.h"

//DEBUG ONLY MODE
#define assertMax(toCheck) assert(toCheck <= maxN);

template<size_t maxN>
class ParticleSystem {
public:
    virtual ~ParticleSystem() = default;

    std::array<Vector, maxN> particlePosition{};
    std::array<Vector, maxN> particleUpNormals{};
    size_t nParticles{0}; //visible particles not max particles

    Bounds particleRenderBounds{Vector(1, 1, 1)};

    Vector worldOffset{};
    float scale{0.05f};

    SDL_FColor particleColour{1, 1, 1, 1};

    //One transfer buffer per particle system?
    SDL_GPUTransferBuffer *transferBuffer{nullptr};
    SDL_GPUBuffer *buffer{nullptr};
    SDL_GPUDevice *device{nullptr};

    std::array<Vertex3D, maxN * 6 * 2 * 3> lastFrameParticleVerticies{};
    bool buffersDirty{true};

    bool toBeDestroyed{false};

    [[nodiscard]] size_t numVerticies() const {
        return nParticles * 6 * 2 * 3; //6 face 2 triangle 3 verticie
    }

    //replace destructor
    virtual void Dispose() = 0;

    virtual void UpdateParticleSystem() = 0;

    virtual void InsertParticles(const std::vector<Vector> &, const std::vector<Vector> &, const std::vector<Vector> &) = 0;

    virtual void InsertParticlesByArray(const Vector *, const Vector *, const Vector *, const float *, size_t) = 0;

    virtual void Upload() = 0;

    virtual void TickParticleSystem(Uint64 dT) = 0;
};

#endif //SDL1_PARTICLESYSTEM_H
