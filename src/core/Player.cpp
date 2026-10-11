#include "Player.h"

#include <algorithm>

void Player::CollectBlock(BlockType blockType, bool toHand) {
    if (toHand) {
        if (hand.first == blockType) {
            if (blocks[hand.first].volume * hand.second < handMaxVolume) {
                if ((blocks[hand.first].volume * hand.second) + blocks[blockType].volume < handMaxVolume) {
                    hand.second += 1;

                    SDL_Log("Now in hand: %i", hand.second);

                    return;
                }
            }
        }
    }

    //Try to find slot with this block type
    for (auto &slot: pocketSlots) {
        if (slot.first == blockType && slot.second > 0) {
            slot.second += 1;
            return;
        }
    }

    //Find an empty slot and start a new stack.
    for (auto &slot: pocketSlots) {
        if (slot.second <= 0) {
            slot = std::make_pair(blockType, 1);
            return;
        }
    }

    SDL_Log("Pockets are full");
}

bool Player::hasInPocketSlots(BlockType blockType) {
    auto it = std::ranges::find_if(pocketSlots, [blockType](const auto &pair) { return pair.first == blockType; });
    return it != pocketSlots.end();
}