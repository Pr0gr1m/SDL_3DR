// #include "GenericParticleSystem.h"
//
// #include "GlobalVariables.h"
//
// template<size_t nMax>
// void GenericParticleSystem<nMax>::GetParticleVerticies() {
//     //render directly onto screen? so use SDL_Vertex
//     std::array<Vertex3D, nMax * 6> particleVerticies{};
//     for (size_t i = 0; i < this->nParticles; i++) {
//         auto particlePosition = this->particlePosition[i];
//         auto halfBoundsX = this->particleRenderBounds.x / 2;
//         auto halfBoundsY = this->particleRenderBounds.x / 2;
//         auto halfBoundsZ = this->particleRenderBounds.x / 2;
//
//         particleVerticies.push_back(Vertex3D{particlePosition + Vector(halfBoundsX, halfBoundsY, halfBoundsZ), 0.5f});
//         particleVerticies.push_back(Vertex3D{particlePosition + Vector(-halfBoundsX, halfBoundsY, halfBoundsZ), 0.5f});
//
//         particleVerticies.push_back(Vertex3D{particlePosition + Vector(halfBoundsX, -halfBoundsY, halfBoundsZ), 0.5f});
//         particleVerticies.push_back(Vertex3D{particlePosition + Vector(-halfBoundsX, -halfBoundsY, halfBoundsZ), 0.5f});
//
//         particleVerticies.push_back(Vertex3D{particlePosition + Vector(halfBoundsX, halfBoundsY, -halfBoundsZ), 0.5f});
//         particleVerticies.push_back(Vertex3D{particlePosition + Vector(-halfBoundsX, halfBoundsY, -halfBoundsZ), 0.5f});
//
//         particleVerticies.push_back(Vertex3D{particlePosition + Vector(halfBoundsX, -halfBoundsY, -halfBoundsZ), 0.5f});
//         particleVerticies.push_back(Vertex3D{particlePosition + Vector(-halfBoundsX, -halfBoundsY, -halfBoundsZ), 0.5f});
//     }
//
//     this->lastFrameParticleVerticies = std::move(particleVerticies);
// }
//
// template<size_t nMax>
// void GenericParticleSystem<nMax>::InsertParticles(const std::vector<Vector> &positions) {
//     for (const auto &position: positions) {
//         this->particlePosition[this->nParticles++] = position;
//     }
//     assert(this->nParticles <= nMax);
// }
//
// template<size_t nMax>
// void GenericParticleSystem<nMax>::Upload() {
//     auto *mappedData = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(this->device, this->transferBuffer, true)); //cycling doesnt change much for smaller words
//     if (mappedData == nullptr) {
//         SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to map chunk transfer buffer: %s", SDL_GetError());
//     }
//
//     SDL_memcpy(mappedData, this->lastFrameParticleVerticies.data(), sizeof(Vertex3D) * this->lastFrameParticleVerticies.size());
//
//     SDL_UnmapGPUTransferBuffer(this->device, this->transferBuffer);
// }