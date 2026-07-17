#include "TextureManager.h"

#include "../App.h"

SDL_Surface *TextureManager::lodSurfaceFromTexture(const char *localPathTo) {
    std::string fullPath = std::string(basePath) + "..\\" + localPathTo;
    SDL_IOStream *textureStream = SDL_IOFromFile(fullPath.c_str(), "rb");

    if (textureStream == nullptr) {
        SDL_LogError(App::APP_LOG_CATEGORY_GENERIC, "Could not open texture: %s", fullPath.c_str());
    }

    SDL_Surface *textureSurface = IMG_LoadJPG_IO(textureStream);
    SDL_CloseIO(textureStream);

    if (textureSurface == nullptr) {
        SDL_LogError(App::APP_LOG_CATEGORY_GENERIC, "IMG_LoadJPG_IO failed: %s", SDL_GetError());
    }

    //Convert surface's format as allegedly IMG_LoadJPG_IO may return surfaces with "weird" pixel formats
    SDL_Surface *convertedTextureSurface = SDL_ConvertSurface(textureSurface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(textureSurface);

    if (convertedTextureSurface == nullptr) {
        SDL_LogError(App::APP_LOG_CATEGORY_GENERIC, "SDL_ConvertSurface failed: %s", SDL_GetError());
    }

    return convertedTextureSurface;
}

SDL_Surface *TextureManager::lodSurfaceFromTexture(BlockType blockType, TextureType textureType) {
    std::string selectBlockPath = this->blockTypeToPath[blockType];
    std::string selectTexturePath = this->textureTypeToPath.at(textureType); //with const maps only at can be used

    if (selectBlockPath.empty()) {
        SDL_LogError(App::APP_LOG_CATEGORY_GENERIC, "No path to texture for block type %d", blockType);
    }

    if (selectTexturePath.empty()) {
        SDL_LogError(App::APP_LOG_CATEGORY_GENERIC, "No path to texture for texture type %d", textureType);
    }

    std::string fullLocalPath = selectBlockPath + selectTexturePath;

    return this->lodSurfaceFromTexture(fullLocalPath.c_str());
}

void TextureManager::AddEntryForBlockType(BlockType blockType, const std::string &localPathTo) {
    if (this->blockTypeToPath.contains(blockType)) {
        SDL_LogError(App::APP_LOG_CATEGORY_GENERIC, "Block type %d already has a path", blockType);
    }

    this->blockTypeToPath[blockType] = localPathTo;
}

