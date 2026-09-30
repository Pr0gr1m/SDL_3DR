#include "ChunkMeshStore.h"

#include <bit>
#include <cstring>

#include "GlobalVariables.h"

namespace {
    ///Largest byte count whose power-of-two capacity still fits SDL's 32-bit buffer sizes
    constexpr size_t kMaxBufferBytes = size_t{1} << 31;

    size_t ByteSize(const std::vector<Vertex3D> &vertices) {
        return vertices.size() * sizeof(Vertex3D);
    }

    bool FitsBufferLimit(const size_t bytes, const char *what) {
        if (bytes <= kMaxBufferBytes) return true;

        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Chunk %s of %zu bytes exceeds the %zu byte buffer limit", what, bytes, kMaxBufferBytes);
        return false;
    }

    Uint32 CapacityBytesFor(const size_t requiredBytes) {
        return std::bit_ceil(static_cast<Uint32>(requiredBytes));
    }
}

ChunkMeshStore::ChunkMeshStore(SDL_GPUDevice *device)
    : device(device) {
}

bool ChunkMeshStore::Upload(SDL_GPUCommandBuffer *commandBuffer, const std::vector<MeshUpdate> &updates) {
    size_t totalBytes = 0;
    for (const auto &update: updates) {
        const size_t bytes = ByteSize(update.vertices);
        if (bytes == 0) {
            meshes[update.chunkLookup].vertexCount = 0;
            continue;
        }

        if (!FitsBufferLimit(bytes, "vertex buffer")) return false;
        if (!EnsureChunkCapacity(meshes[update.chunkLookup], bytes)) return false;
        totalBytes += bytes;
    }

    if (totalBytes == 0) return true;

    if (!FitsBufferLimit(totalBytes, "transfer buffer")) return false;
    if (!EnsureTransferCapacity(totalBytes)) return false;

    auto *mappedData = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(device, transferBuffer, true));
    if (mappedData == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to map chunk transfer buffer: %s", SDL_GetError());
        return false;
    }

    size_t offset = 0;
    for (const auto &update: updates) {
        const size_t bytes = ByteSize(update.vertices);
        if (bytes == 0) continue;

        SDL_memcpy(mappedData + offset, update.vertices.data(), bytes);
        offset += bytes;
    }
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    if (copyPass == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to begin chunk copy pass: %s", SDL_GetError());
        return false;
    }

    offset = 0;
    for (const auto &update: updates) {
        const size_t bytes = ByteSize(update.vertices);
        if (bytes == 0) continue;

        GpuMesh &mesh = meshes[update.chunkLookup];
        const SDL_GPUTransferBufferLocation source = {transferBuffer, static_cast<Uint32>(offset)};
        const SDL_GPUBufferRegion destination = {mesh.buffer, 0, static_cast<Uint32>(bytes)};
        SDL_UploadToGPUBuffer(copyPass, &source, &destination, true);
        mesh.vertexCount = static_cast<Uint32>(update.vertices.size());
        offset += bytes;
    }
    SDL_EndGPUCopyPass(copyPass);

    return true;
}

void ChunkMeshStore::ReleaseAll() {
    for (auto &[chunkLookup, mesh]: meshes) {
        if (mesh.buffer) SDL_ReleaseGPUBuffer(device, mesh.buffer);
    }
    meshes.clear();

    if (transferBuffer) SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    transferBuffer = nullptr;
    transferCapacityBytes = 0;
}

bool ChunkMeshStore::EnsureChunkCapacity(GpuMesh &mesh, const size_t requiredBytes) const {
    if (mesh.buffer != nullptr && mesh.capacityBytes >= requiredBytes) return true;

    if (mesh.buffer) SDL_ReleaseGPUBuffer(device, mesh.buffer);
    mesh.buffer = nullptr;
    mesh.capacityBytes = 0;
    mesh.vertexCount = 0;

    SDL_GPUBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferInfo.size = CapacityBytesFor(requiredBytes);
    mesh.buffer = SDL_CreateGPUBuffer(device, &bufferInfo);
    if (mesh.buffer == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create chunk vertex buffer: %s", SDL_GetError());
        return false;
    }

    mesh.capacityBytes = bufferInfo.size;
    return true;
}

bool ChunkMeshStore::EnsureTransferCapacity(const size_t requiredBytes) {
    if (transferBuffer != nullptr && transferCapacityBytes >= requiredBytes) return true;

    if (transferBuffer) SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    transferBuffer = nullptr;
    transferCapacityBytes = 0;

    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = CapacityBytesFor(requiredBytes);
    transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    if (transferBuffer == nullptr) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create chunk transfer buffer: %s", SDL_GetError());
        return false;
    }

    transferCapacityBytes = transferInfo.size;
    return true;
}
