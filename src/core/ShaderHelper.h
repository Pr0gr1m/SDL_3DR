SDL_Window *window;
SDL_GPUDevice *device;
SDL_GPUBuffer *vertexBuffer;
SDL_GPUTransferBuffer *transferBuffer;
SDL_GPUGraphicsPipeline *graphicsPipeline;

constexpr const char *kVertexShaderPath = "shaders/vertex.spv";
constexpr const char *kFragmentShaderPath = "shaders/fragment.spv";
