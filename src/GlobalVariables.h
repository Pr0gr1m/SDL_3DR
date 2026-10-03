#pragma once

#include "core/Ray.h"
#include <SDL3/SDL.h>

// namespace GlobalVariables {
#define CONTINUE SDL_APP_CONTINUE
#define SUCCESS SDL_APP_SUCCESS
#define FAILURE SDL_APP_FAILURE

#define F2STRING(Value) #Value

// #define index(x,y,width) x*width + y

inline constexpr int debugLevel = 0; //switch to 0/-1 for Release, adds around 7 FPS for small scene

#define SLog1(...) do { if (debugLevel >= 1) SDL_Log(__VA_ARGS__); } while (0) //__VA_ARGS__ forwards every argument
#define SLog2(...) do { if (debugLevel >= 2) SDL_Log(__VA_ARGS__); } while (0) //__VA_ARGS__ forwards every argument

/**
 *@file GlobalVariables.h
 *@brief File holding global variables, functions and macros
 */

/**
 * Returns a sign of the integer
 * @tparam N Must be compatible with concept std::integral
 * @param value Value to check sign of
 * @return 1 if value > 0, -1 if value < 0, and 0 if value == 0
 */
template<std::integral N>
[[nodiscard]] int signOf(N value) noexcept {
    return (value > 0) - (value < 0); //b - b
}

/**
 * Returns a sign of the floating number
 * @tparam N Must be compatible with concept std::floating_point
 * @param value Value to check sign of
 * @return 1 if value > 0, -1 if value < 0, and 0 if value == 0
 */
template<std::floating_point N>
[[nodiscard]] int signOf(N value) noexcept {
    if (std::isnan(value)) {
        return 0;
    }
    return (value > 0) - (value < 0);
}

[[nodiscard]] static constexpr int toInt(float f) { return static_cast<int>(std::lround(f)); }

template<typename N>
[[nodiscard]] static constexpr int toInt(N f) { return static_cast<int>(std::lround(f)); }

/**
 *@enum AppLogCategory
 *@brief Describes custom error log categories
*/
enum AppLogCategory {
    APP_LOG_CATEGORY_GENERIC = SDL_LOG_CATEGORY_APPLICATION,
    APP_LOG_CATEGORY_VIDEO = SDL_LOG_CATEGORY_VIDEO
};

inline extern constexpr RaycastHit RaycastHit_NULL = {};

inline constexpr int numPlayerSlots = 2;

inline constexpr float noiseFrequency = 0.05f;
inline constexpr float noiseAmplitude = 2.f;

inline constexpr float defaultScreenWidth = 1920;
inline constexpr float defaultScreenHeight = 1080;
inline constexpr float defaultAspectRatio = defaultScreenWidth / defaultScreenHeight;

inline constexpr float kMouseLookSensitivity = 0.2f;
inline constexpr float kMoveSpeed = 1.75f;
inline constexpr float kJumpForceMagnitude = 0.3f;
inline constexpr float kBlockHalfExtent = 0.5f;
inline constexpr float kCameraHeightAboveGround = 1.5f;
inline constexpr float kGravityMultiplier = 1;

inline constexpr float DEG_2_RAD = M_PI / 180.0f;
