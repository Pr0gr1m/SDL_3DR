#ifndef SDL1_TEXTUREMANAGER_H
#define SDL1_TEXTUREMANAGER_H
#include <map>
#include <SDL3/SDL_surface.h>
#include <string>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_log.h>
#include <SDL3_image/SDL_image.h>

//lAYOUT OF BLOCK FILES:
/*
../basePath
    textures
        dirt
            normal
            colour
        grass
            normal
            colour



*/
class TextureManager {
public:
    TextureManager(char *basePath) : basePath(basePath) {
    }

    enum BlockType {
        Dirt,
        OakLog,
    };

    enum TextureType {
        Colour,
        Normal
    };

    /**
     * Loads the file and returns the surface in pixel format RGBA32
     * @param localPathTo local (from base path) path to the file
     * @return Converted surface
     */
    SDL_Surface *lodSurfaceFromTexture(const char *localPathTo);

    /**
     * Loads the file and returns the surface in pixel format RGBA32
     * @return
     */

    SDL_Surface *lodSurfaceFromTexture(BlockType, TextureType);

    void AddEntryForBlockType(BlockType, const std::string &);

private:
    char *basePath{};

    std::map<BlockType, std::string> blockTypeToPath;

    const std::map<TextureType, std::string> textureTypeToPath{
        {
            TextureType::Normal, "normal"
        },
        {
            TextureType::Colour, "colour"
        }
    };
};
#endif //SDL1_TEXTUREMANAGER_H
