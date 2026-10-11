#ifndef SDL1_PLAYER_H
#define SDL1_PLAYER_H

#include <array>

#include "Block.h"
#include "GlobalVariables.h"

constexpr float handMaxVolume = 2;
constexpr int numPocketSlots = 4;

class Player {
public:
    Player() = default;

    std::pair<BlockType, int> hand{};

    std::array<std::pair<BlockType, int>, numPocketSlots> pocketSlots{};

    void CollectBlock(BlockType blockType, bool toHand);

private:
    bool hasInPocketSlots(BlockType);
};
#endif //SDL1_PLAYER_H
