#pragma once

#include "core/Ray.h"
#include <SDL3/SDL.h>

// namespace GlobalVariables {
#define CONTINUE SDL_APP_CONTINUE
#define SUCCESS SDL_APP_SUCCESS
#define FAILURE SDL_APP_FAILURE

#define F2STRING(Value) #Value

/**
 *@enum AppLogCategory
 *@brief Describes custom error log categories
*/
enum AppLogCategory {
    APP_LOG_CATEGORY_GENERIC = SDL_LOG_CATEGORY_APPLICATION,
    APP_LOG_CATEGORY_VIDEO = SDL_LOG_CATEGORY_VIDEO
};

inline extern constexpr RaycastHit RaycastHit_NULL = {};

static constexpr float defaultScreenWidth = 1920;
static constexpr float defaultScreenHeight = 1080;
static constexpr float defaultAspectRatio = defaultScreenWidth / defaultScreenHeight;

static constexpr float kMouseLookSensitivity = 0.2f;
static constexpr float kMoveSpeed = 1.75f;
static constexpr float kJumpForceMagnitude = 0.3f;
static constexpr float kBlockHalfExtent = 0.5f;
static constexpr float kCameraHeightAboveGround = 1.5f;
static constexpr float kGravityMultiplier = 1;

static constexpr float DEG_2_RAD = M_PI / 180.0f;
// }
