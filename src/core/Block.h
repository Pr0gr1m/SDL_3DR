#ifndef SDL1_BLOCK_H
#define SDL1_BLOCK_H

enum BlockType {
    None = 0,
    Dirt = 1
};

struct Block {
    BlockType type{};
    float volume{};
    float mass{};
    bool fitsInPocket{};

    Block(BlockType type, float volume, float mass, bool fitsInPocket) : type(type), volume(volume), mass(mass), fitsInPocket(fitsInPocket) {
    }
};

inline Block blocks[] = {{Dirt, 1, 1, true}};
#endif //SDL1_BLOCK_H
