#ifndef SDL1_TEXTUREMANAGER_H
#define SDL1_TEXTUREMANAGER_H
#include <map>
#include <SDL3/SDL_surface.h>
#include <string>

//Example layout of the block files:
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

/**
 *@class TextureManager
 *@brief Class storing block and texture types as well as paths to all textures
 */
class TextureManager {
public:
    TextureManager(char *basePath) : basePath(basePath) {
    }

    /**
     * @enum BlockType
     * @brief All block types
     */
    enum BlockType {
        Dirt,
        OakLog,
    };

    /**
    * @enum TextureType
    * @brief All texture types
    */
    enum TextureType {
        Colour,
        Normal
    };

    /**
     * Loads the file and returns the surface in pixel format RGBA32
     * @param localPathTo local (from base path) path to the file
     * @return Converted surface
     */
    SDL_Surface *loadSurfaceFromTexture(const char *localPathTo);

    /**
     * Loads the file based on block and texture type and returns the surface in pixel format RGBA32
     * @param blockType block type (e.g dirt)
     * @param textureType texture type (e.g colour, normal)
     * @return Converted surface
     */
    SDL_Surface *loadSurfaceFromTexture(BlockType blockType, TextureType textureType);

    /**
     *Adds a path entry for a blockType
     *@param blockType Block type
     *@param path Local path to folder containing that block's textures
     */
    void AddEntryForBlockType(BlockType blockType, const std::string &path);

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
