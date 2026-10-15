#ifndef SDL1_SKYSTARSPARTICLESYSTEM_H
#define SDL1_SKYSTARSPARTICLESYSTEM_H
#include <SDL3/SDL_log.h>

#include "GlobalVariables.h"
#include "ParticleSystem.h"

//2 faces - 2 triangles - 3 verticies
template<size_t maxN, size_t vertsPerParticle = 2 * 2 * 3>
class SkyStarsParticleSystem : public ParticleSystem<maxN> {
public:
    std::array<float, maxN> particleScaleMultipliers{};

    std::array<Vertex3D, maxN * vertsPerParticle> lastFrameParticleVerticies{};

    //2 faces - 2 triangles - 3 verticies
    float daytimeFade{0}; //0-100

    void Dispose() override {
        this->toBeDestroyed = true;
        this->DisposeBuffers();
    }

    void UpdateParticleSystem() override;

    void InsertParticlesByArray(const Vector *particlesPositions, const float *ParticleScaleMultipliers, size_t numParticles);

    void TickParticleSystem(Uint64) override;

    void Upload() override;

    [[deprecated]]
    void InsertParticlesByArray(const Vector *particlesPositions, const Vector *particleUpNormals, const Vector *particleVelocites, const float *ParticleScaleMultipliers, size_t numParticles) {
    }

    [[nodiscard]] size_t numVerticies() const override {
        return this->nParticles * vertsPerParticle;
    }
};

template<size_t maxN, size_t vertsPerParticle>
void SkyStarsParticleSystem<maxN, vertsPerParticle>::UpdateParticleSystem() {
    if (this->toBeDestroyed) return;

    constexpr float sphereRadius = 100;
    Vector &playerPosition = this->worldOffset; //alias for world offset (for now)

    const static auto worldUp = Vector(0, 1, 0);

    const float halfBoundsX = this->particleRenderBounds.topLeftForwardCorner.x * 0.5f;
    const float halfBoundsY = this->particleRenderBounds.topLeftForwardCorner.y * 0.5f;

    const SDL_FColor particleColour = this->particleColour;

    std::array<Vertex3D, maxN * vertsPerParticle> particleVerticies{};
    size_t index = 0;
    for (size_t i = 0; i < this->nParticles; i++) {
        float alpha = this->particlePosition[i].x;
        float beta = this->particlePosition[i].z;

        const Vector sphereDir{std::cos(alpha) * std::cos(beta), std::sin(alpha), std::cos(alpha) * std::sin(beta)};
        const Vector particlePosition = playerPosition + (sphereDir * sphereRadius);

        //normal points at playerPositon from particlePosition
        auto normal = (playerPosition - particlePosition).Normalized();
        auto right = worldUp.Cross(normal);
        auto up = normal.Cross(right).Normalized();

        const auto scale = this->scale * 15 * (this->particleScaleMultipliers[i] + 1);

        const Vector rightExtent = right * (halfBoundsX * scale);
        const Vector upExtent = up * (halfBoundsY * scale);

        const Vector tl = particlePosition - rightExtent + upExtent;
        const Vector tr = particlePosition + rightExtent + upExtent;
        const Vector br = particlePosition + rightExtent - upExtent;
        const Vector bl = particlePosition - rightExtent - upExtent;

        // Triangle 1
        particleVerticies[index++] = Vertex3D{tl, particleColour, normal, 0.f, 0.f, right};
        particleVerticies[index++] = Vertex3D{br, particleColour, normal, 1.f, 0.f, right};
        particleVerticies[index++] = Vertex3D{tr, particleColour, normal, 1.f, 1.f, right};

        // Triangle 2
        particleVerticies[index++] = Vertex3D{tl, particleColour, normal, 0.f, 0.f, right};
        particleVerticies[index++] = Vertex3D{bl, particleColour, normal, 1.f, 1.f, right};
        particleVerticies[index++] = Vertex3D{br, particleColour, normal, 0.f, 1.f, right};
    }

    this->lastFrameParticleVerticies = std::move(particleVerticies);
}

template<size_t maxN, size_t vertsPerParticle>
void SkyStarsParticleSystem<maxN, vertsPerParticle>::Upload() {
    if (this->toBeDestroyed) return;

    if (this->buffersDirty) {
        if (this->transferBuffer) SDL_ReleaseGPUTransferBuffer(this->device, this->transferBuffer);
        if (this->buffer) SDL_ReleaseGPUBuffer(this->device, this->buffer);

        const auto sizeBytes = static_cast<Uint32>(std::bit_ceil(sizeof(Vertex3D) * this->numVerticies()));

        const SDL_GPUTransferBufferCreateInfo transferCreateInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, sizeBytes, 0};
        this->transferBuffer = SDL_CreateGPUTransferBuffer(this->device, &transferCreateInfo);

        const SDL_GPUBufferCreateInfo bufferCreateInfo{SDL_GPU_BUFFERUSAGE_VERTEX, sizeBytes};
        this->buffer = SDL_CreateGPUBuffer(this->device, &bufferCreateInfo);

        this->buffersDirty = false;
    }

    if (this->transferBuffer && this->buffer) {
        auto *mappedData = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(this->device, this->transferBuffer, true)); //cycling doesnt change much for smaller words
        if (mappedData == nullptr) {
            SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to map chunk transfer buffer: %s", SDL_GetError());
        }

        SDL_memcpy(mappedData, this->lastFrameParticleVerticies.data(), sizeof(Vertex3D) * this->numVerticies());

        SDL_UnmapGPUTransferBuffer(this->device, this->transferBuffer);
    }
}

template<size_t maxN, size_t vertsPerParticle>
void SkyStarsParticleSystem<maxN, vertsPerParticle>::InsertParticlesByArray(const Vector *particlesPositions, const float *ParticleScaleMultipliers, size_t numParticles) {
    if (this->toBeDestroyed) return;

    //assertMax(this->nParticles + numParticles)
    this->buffersDirty = true;

    for (size_t i = 0; i < numParticles; i++) {
        this->particlePosition[this->nParticles + i] = particlesPositions[i];
        particleScaleMultipliers[this->nParticles + i] = ParticleScaleMultipliers[i];
    }

    this->nParticles += numParticles;
}

template<size_t maxN, size_t vertsPerParticle>
void SkyStarsParticleSystem<maxN, vertsPerParticle>::TickParticleSystem(Uint64) {
}

#endif //SDL1_SKYSTARSPARTICLESYSTEM_H
