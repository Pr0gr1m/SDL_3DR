#ifndef SDL1_CAMERA_H
#define SDL1_CAMERA_H

#include <array>

#include "Vector.h"
#include <cmath>
#include <map>

#include "Matrix4D.h"
#include "Object.h"

// constexpr float DEG_2_RAD = M_PI / 180.0f;

/**
 *@class Camera
 *@brief A class representing a player camera
 */
class Camera {
public:
    /**
     *@enum MoveStates
     *@brief Enum representing all possible player move states
     */
    enum MoveStates {
        Forward,
        Backward,
        Left,
        Right,
        Up,
        Down,
    };

    Camera() {
        UpdateDirectionVectors();
    }

    ~Camera() = default;

    ///Camera's current position
    Vector Position = Vector(0, 0, 0);

    ///Camera's pitch, yaw and roll
    float pitch{}, yaw{}, roll{};

    ///World up
    Vector worldUp = Vector(0, 1, 0);
    ///Camera's local forward in global space
    Vector forward;
    ///Camera's local right in global space
    Vector right;
    ///Camera's local up in global space
    Vector up;

    ///Map of all move states
    std::map<MoveStates, bool> currentMoveStates;

    ///Camera's FOV
    float fovY = 80;
    ///Camera's near plane distance
    float near = 0.1f;
    ///Camera's far plane distance
    float far = 100.f;

    ///Aspect ratio
    float aspect = 1;

    ///Last frame's delta time
    Uint64 deltaTimeMS{};

    ///Current camera velocity
    Vector cameraVelocity = Vector(0, 0, 0);

    ///Camera's mass, only divides force when adding it to velocity
    double cameraMass = 10.00;

    //or plane normal * any point + distance/offset from origin = 0

    /**
     *@struct FrustrumPlane
     *@brief Trivial struct representing camera's frustrum plane as equation: Ax + By + Cz + D = 0
    */
    struct FrustrumPlane {
        float A;
        float B;
        float C;
        float D;
    };

    ///Camera's frustrum planes
    std::array<FrustrumPlane, 6> frustrumPlanes{};
    ///Camera's frustrum corners
    std::array<Vector, 8> frustrumCorners;

    Camera(float p, float y, float r) : pitch(p), yaw(y), roll(r) {
        UpdateDirectionVectors();
    }

    Camera(Vector positon, float pitch, float yaw, float roll) : Position(positon), pitch(pitch), yaw(yaw), roll(roll) {
        currentMoveStates = std::map<MoveStates, bool>();
        UpdateDirectionVectors();
    };

    Camera(Vector positon, float pitch, float yaw, float roll, float aspect) : Position(positon), pitch(pitch), yaw(yaw), roll(roll), aspect(aspect) {
        currentMoveStates = std::map<MoveStates, bool>();
        UpdateDirectionVectors();
    };

    /**
     *Sets a selected move state to a flag
     *@param state Selected move state
     *@param flag Flag
     */
    void SetMoveState(MoveStates state, bool flag);

    /**
     *Returns whether a move state is active or not
     *@param state Selected move state
     *@returns State of the selected state
     */
    bool GetMoveState(MoveStates state);

    ///Adds a normalized force with the force

    /**
     *Adds a normalized force with a magnitude to a camera velocity
     *@param force Normalized force vector
     *@param magnitude Force vector magnitude
     */
    void AddForceThisTick(Vector force, float magnitude);

    ///Adds a force

    /**
     *Adds a force to a camera velocity
     *@param force Force vector
     */
    void AddForceThisTick(Vector force);

    /**
     *Updates camera's poisition based on its current velocity
     */
    void MoveCameraBasedOnVelocity();

    ///Returns 8 vector points representing camera's frustrum's corners

    /**
     *Updates camera's frustrum planes and their corners
     */
    void UpdateCameraFrustrumCorners();

    ///Returns whether a point is inside (true) or outside (false) every camera's frustrum plane
    /**
     *Checks if point is on the same side of every frustrum plane
     *@param point Global point to check
     *@returns If the point is inside every frustrum plane it returns true, otherwise false
     */
    [[nodiscard]] bool IsPointInFrustum(const Vector &point) const;

    /**
     *Returns camera's view 4x4 matrix
     *@returns Camera's view matrix
     */
    [[nodiscard]] Matrix4D GetViewMatrix() const {
        return Matrix4D(
            right.x, right.y, right.z, -Position.Dot(right),
            up.x, up.y, up.z, -Position.Dot(up),
            -forward.x, -forward.y, -forward.z, Position.Dot(forward),
            0, 0, 0, 1
        );
    }

    /**
      *Returns camera's projection 4x4 matrix
      *@returns Camera's view matrix
      */
    [[nodiscard]] Matrix4D GetProjectionMatrix() const {
        float fovRad = fovY * DEG_2_RAD;
        float f = 1.0f / tan(fovRad / 2.0f);

        float safeAspect = (aspect == 0.f) ? 1.f : aspect;
        float zRange = near - far;
        if (zRange == 0.f) zRange = -0.001f;

        // Vulkan style: z in [0,1]
        return Matrix4D(
            f / safeAspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, far / zRange, -(far * near) / (far - near == 0.f ? 0.001f : far - near),
            0, 0, -1, 0
        );
    }

    /**
     *Updates cameras direction vectors
     */
    void UpdateDirectionVectors();

    /**
     *Resets camera velocity with 0 values in a specific axis if same parameters axis is greater than 0
     *@param axis Axis
     */
    void ResetVelocityAlongWorldAxis(Vector axis);
};


#endif //SDL1_CAMERA_H

