#ifndef SDL1_CHUNKMESHSTORE_H
#define SDL1_CHUNKMESHSTORE_H
#include <cstddef>
#include <map>
#include <vector>
#include <SDL3/SDL.h>

#include "core/Int3.h"
#include "core/Vertex3D.h"

/**
 *@class ChunkMeshStore
 *@brief Owns one persistent GPU vertex buffer per chunk, keyed by the chunk's atPosition
**/
class ChunkMeshStore {
public:
    struct GpuMesh {
        SDL_GPUBuffer *buffer = nullptr;
        Uint32 capacityBytes = 0;
        Uint32 vertexCount = 0;
    };

    ///Freshly built chunk-local mesh that replaces the stored mesh of a chunk
    struct MeshUpdate {
        Int3 chunkLookup;
        std::vector<Vertex3D> vertices;
    };

    explicit ChunkMeshStore(SDL_GPUDevice *device);

    ///Uploads every update in one copy pass, growing chunk buffers when needed. Does nothing when there are no bytes to upload
    [[nodiscard]] bool Upload(SDL_GPUCommandBuffer *commandBuffer, const std::vector<MeshUpdate> &updates);

    [[nodiscard]] const std::map<Int3, GpuMesh> &Meshes() const { return meshes; }

    ///Releases every GPU resource, the GPU must be idle
    void ReleaseAll();

private:
    [[nodiscard]] bool EnsureChunkCapacity(GpuMesh &mesh, size_t requiredBytes) const;

    [[nodiscard]] bool EnsureTransferCapacity(size_t requiredBytes);

    SDL_GPUDevice *device;
    std::map<Int3, GpuMesh> meshes;
    SDL_GPUTransferBuffer *transferBuffer = nullptr;
    Uint32 transferCapacityBytes = 0;
};

#endif //SDL1_CHUNKMESHSTORE_H
