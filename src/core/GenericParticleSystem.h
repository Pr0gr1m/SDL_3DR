#ifndef SDL1_GENERICPARTICLESYSTEM_H
#define SDL1_GENERICPARTICLESYSTEM_H
#include "ParticleSystem.h"

#include "GlobalVariables.h"
#include <cassert>

template<size_t maxN>
//public inheritance, now genericparticlesystem is convertible to particlesystem
class GenericParticleSystem : public ParticleSystem<maxN> {
public:
    std::array<Vector, maxN> particleVelocities{};
    std::array<float, maxN> particleScaleMultipliers{};
    std::array<Vector, maxN> particleUpNormals{};

    int64_t particleLifetimeMSeconds{};

private:
    int64_t maxParticleLifetimeMSeconds{};

public:
    bool destroyOnLifetimeEnd{true};
    bool affectedByGravity{true};

    bool enableCollisions{true};

    enum CollisionType {
        Simple, Advanced
    };

    CollisionType collisionType{Advanced};

    std::optional<ChunkManager *> chunkManager;

    GenericParticleSystem(SDL_GPUDevice *device, int64_t lifetime) {
        this->device = device;
        maxParticleLifetimeMSeconds = particleLifetimeMSeconds = lifetime;
    }

    void Dispose() override {
        this->toBeDestroyed = true;

        if (this->buffer) SDL_ReleaseGPUBuffer(this->device, this->buffer);
        if (this->transferBuffer) SDL_ReleaseGPUTransferBuffer(this->device, this->transferBuffer);

        this->buffer = nullptr;
        this->transferBuffer = nullptr;
    }

    float GetParticleTransparency(float);

    void UpdateParticleSystem() override;

    void InsertParticlesByArray(const Vector *, const Vector *, const Vector *, const float *particleScaleMultipliers, size_t) override;

    void Upload() override;

    void TickParticleSystem(Uint64) override;
};

//template definitions have to be in .h

template<size_t maxN>
float GenericParticleSystem<maxN>::GetParticleTransparency(float data) {
    return data;
}

template<size_t nMax>
void GenericParticleSystem<nMax>::UpdateParticleSystem() {
    if (this->toBeDestroyed) return;

    //render directly onto screen? so use SDL_Vertex
    std::array<Vertex3D, nMax * 6 * 3 * 2> particleVerticies{};
    size_t index = 0;
    for (size_t i = 0; i < this->nParticles; i++) {
        auto particlePosition = this->particlePosition[i] + this->worldOffset;

        auto halfBoundsX = this->particleRenderBounds.topLeftForwardCorner.x / 2;
        auto halfBoundsY = this->particleRenderBounds.topLeftForwardCorner.y / 2;
        auto halfBoundsZ = this->particleRenderBounds.topLeftForwardCorner.z / 2;

        auto scale = this->scale * this->particleScaleMultipliers[i];

        auto x = halfBoundsX * scale;
        auto y = halfBoundsY * scale;
        auto z = halfBoundsZ * scale;

        SDL_FColor particleColour = this->particleColour;
        particleColour.a = GetParticleTransparency(this->particleLifetimeMSeconds / static_cast<float>(maxParticleLifetimeMSeconds)); //1000 is max lifetime

        Vector particleUpNormal = this->particleUpNormals[i];
        Vector worldForwardVector{0, 0, 1};
        Vector particleRightNormal = worldForwardVector.Cross(particleUpNormal).Normalized();
        Vector particleForwardNormal = particleRightNormal.Cross(particleUpNormal).Normalized();

        Vector particleTangentVector = worldForwardVector.Cross(particleRightNormal).Normalized();

        //front face
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, y, z), particleColour, particleForwardNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, y, z), particleColour, particleForwardNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, -y, z), particleColour, particleForwardNormal, 0, 0, particleTangentVector};

        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, y, z), particleColour, particleForwardNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, -y, z), particleColour, particleForwardNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, -y, z), particleColour, particleForwardNormal, 0, 0, particleTangentVector};

        //left face
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, y, z), particleColour, particleRightNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, y, -z), particleColour, particleRightNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, -y, -z), particleColour, particleRightNormal.Negated(), 0, 0, particleTangentVector};

        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, y, z), particleColour, particleRightNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, -y, z), particleColour, particleRightNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, -y, -z), particleColour, particleRightNormal.Negated(), 0, 0, particleTangentVector};

        //back face
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, y, -z), particleColour, particleForwardNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, -y, -z), particleColour, particleForwardNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, -y, -z), particleColour, particleForwardNormal.Negated(), 0, 0, particleTangentVector};

        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, y, -z), particleColour, particleForwardNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, y, -z), particleColour, particleForwardNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, -y, -z), particleColour, particleForwardNormal.Negated(), 0, 0, particleTangentVector};

        //right face
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, y, z), particleColour, particleRightNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, y, -z), particleColour, particleRightNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, -y, -z), particleColour, particleRightNormal, 0, 0, particleTangentVector};

        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, y, z), particleColour, particleRightNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, -y, z), particleColour, particleRightNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, -y, -z), particleColour, particleRightNormal, 0, 0, particleTangentVector};

        //top face
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, y, z), particleColour, particleUpNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, y, z), particleColour, particleUpNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, y, -z), particleColour, particleUpNormal, 0, 0, particleTangentVector};

        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, y, z), particleColour, particleUpNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, y, z), particleColour, particleUpNormal, 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, y, -z), particleColour, particleUpNormal, 0, 0, particleTangentVector};

        //bottom face
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, -y, z), particleColour, particleUpNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, -y, z), particleColour, particleUpNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, -y, -z), particleColour, particleUpNormal.Negated(), 0, 0, particleTangentVector};

        particleVerticies[index++] = Vertex3D{particlePosition + Vector(x, -y, z), particleColour, particleUpNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, -y, z), particleColour, particleUpNormal.Negated(), 0, 0, particleTangentVector};
        particleVerticies[index++] = Vertex3D{particlePosition + Vector(-x, -y, -z), particleColour, particleUpNormal.Negated(), 0, 0, particleTangentVector};
    }

    this->lastFrameParticleVerticies = std::move(particleVerticies);
}

template<size_t maxN>
void GenericParticleSystem<maxN>::InsertParticlesByArray(const Vector *particlesPositions, const Vector *particleUpNormals, const Vector *particleVelocites, const float *ParticleScaleMultipliers, size_t numParticles) {
    if (this->toBeDestroyed) return;

    assertMax(this->nParticles + numParticles)
    this->buffersDirty = true;

    for (size_t i = 0; i < numParticles; i++) {
        this->particlePosition[this->nParticles + i] = particlesPositions[i];
        this->particleUpNormals[this->nParticles + i] = particleUpNormals[i];
        this->particleVelocities[this->nParticles + i] = particleVelocites[i];
        particleScaleMultipliers[this->nParticles + i] = ParticleScaleMultipliers[i];
    }

    this->nParticles += numParticles;
}

template<size_t nMax>
void GenericParticleSystem<nMax>::Upload() {
    if (this->toBeDestroyed) return;

    if (this->buffersDirty) {
        if (this->transferBuffer) SDL_ReleaseGPUTransferBuffer(this->device, this->transferBuffer);
        if (this->buffer) SDL_ReleaseGPUBuffer(this->device, this->buffer);

        const auto sizeBytes = static_cast<Uint32>(std::bit_ceil(sizeof(Vertex3D) * this->lastFrameParticleVerticies.size()));

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

        SDL_memcpy(mappedData, this->lastFrameParticleVerticies.data(), sizeof(Vertex3D) * this->lastFrameParticleVerticies.size());

        SDL_UnmapGPUTransferBuffer(this->device, this->transferBuffer);
    }
}

template<size_t maxN>
void GenericParticleSystem<maxN>::TickParticleSystem(Uint64 deltaTIme) {
    if (this->toBeDestroyed) return;

    particleLifetimeMSeconds -= deltaTIme;

    for (size_t i = 0; i < this->nParticles; i++) {
        this->particleVelocities[i] += GetGravityVector() * 0.95f * toInt(deltaTIme) / 1000;

        if (enableCollisions) {
            const float particleGroundPosition = (this->scale * this->particleScaleMultipliers[i] * 0.5f);
            const Vector realParticlePosition = (this->particlePosition[i] + this->worldOffset); //only use for checks
            if (collisionType == Simple) {
                if (realParticlePosition.y < 0 + particleGroundPosition) {
                    this->particlePosition[i].y = (0.0f + particleGroundPosition) - this->worldOffset.y;
                    this->particleVelocities[i] = Vector(0, 0, 0);
                }
            } else if (collisionType == Advanced) {
                if (this->chunkManager.has_value()) {
                    auto chunk = chunkManager.value()->findChunk(ChunkManager::chunkLookupFromBlockPosition(realParticlePosition));

                    if (chunk) {
                        const Vector particleBottomPosition = realParticlePosition - Vector(0, particleGroundPosition + 0.01f, 0);
                        const std::optional<Object> *objectAt = chunk->tryGetWorld((particleBottomPosition));
                        if (objectAt != nullptr && objectAt->has_value()) {
                            this->particlePosition[i].y = particleGroundPosition + 0.5f + objectAt->value().Position.y + chunk->atPosition.y - this->worldOffset.y;
                            this->particleVelocities[i] = Vector(0, 0, 0);
                        }
                    }
                }
            }

            this->particlePosition[i] += (this->particleVelocities[i] * toInt(deltaTIme)) / 1000;
        }
    }

    if (particleLifetimeMSeconds <= 0) {
        if (destroyOnLifetimeEnd) {
            this->Dispose();
        }
    }
}

#endif //SDL1_GENERICPARTICLESYSTEM_H