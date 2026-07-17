#include "Camera.h"

#include <iostream>
#include <SDL3/SDL_log.h>

void Camera::SetMoveState(MoveStates state, bool flag) {
    currentMoveStates[state] = flag;
}

bool Camera::GetMoveState(MoveStates state) {
    return currentMoveStates[state];
}

void Camera::AddForceThisTick(Vector dirNorm, float mag) {
    this->AddForceThisTick(dirNorm * mag);
}

void Camera::AddForceThisTick(Vector dir) {
    cameraVelocity += dir / cameraMass;
}

void Camera::MoveCameraBasedOnVelocity() {
    this->Position += cameraVelocity;
}

void Camera::UpdateCameraFrustrumCorners() {
    auto scalePointFromCenter = [](const Vector &point, const Vector &center, const float &scale) {
        return center + ((point - center) * scale);
    };

    auto convertPointsToFrustrumPlane = [&](
        const Vector &a,
        const Vector &b,
        const Vector &c,
        const Vector &point) {
        FrustrumPlane p{};

        auto v1 = b - a;
        auto v2 = c - a;

        auto perpendicular = v1.Cross(v2).Normalized();

        p.A = perpendicular.x;
        p.B = perpendicular.y;
        p.C = perpendicular.z;
        p.D = -(p.A * a.x + p.B * a.y + p.C * a.z);

        // Check if pointInside is on the positive side of the plane
        if (p.A * point.x + p.B * point.y + p.C * point.z + p.D < 0.0f) {
            p.A = -p.A;
            p.B = -p.B;
            p.C = -p.C;
            p.D = -p.D;
        }

        return p;
    };

    std::array<Vector, 8> fCorners{};
    std::array<FrustrumPlane, 6> fPlanes{};

    float halfFovY = fovY * 0.5f * DEG_2_RAD;
    float halfFovX = atanf(tanf(halfFovY) * aspect); // horizontal FOV

    float nearPlaneYDist = near * tanf(halfFovY);
    float nearPlaneXDist = near * tanf(halfFovX);

    float farPlaneYDist = far * tanf(halfFovY);
    float farPlaneXDist = far * tanf(halfFovX);

    Vector forwardNear = forward * near;
    Vector forwardFar = forward * far;

    //Near plane
    fCorners[0] = Position + forwardNear + right * nearPlaneXDist + up * nearPlaneYDist; //top right
    fCorners[1] = Position + forwardNear + right * nearPlaneXDist - up * nearPlaneYDist; //bottom right
    fCorners[2] = Position + forwardNear - right * nearPlaneXDist - up * nearPlaneYDist; //bottom left
    fCorners[3] = Position + forwardNear - right * nearPlaneXDist + up * nearPlaneYDist; //topleft

    //Far plane
    fCorners[4] = Position + forwardFar + right * farPlaneXDist + up * farPlaneYDist; //top right
    fCorners[5] = Position + forwardFar + right * farPlaneXDist - up * farPlaneYDist; //bottom right
    fCorners[6] = Position + forwardFar - right * farPlaneXDist - up * farPlaneYDist; //bottom left
    fCorners[7] = Position + forwardFar - right * farPlaneXDist + up * farPlaneYDist; //top left

    // Vector frustrumCenter = Vector(0.f, 0.f, 0.f);
    // for (auto fCorner: fCorners) {
    //     frustrumCenter += fCorner;
    // }
    // frustrumCenter *= (1 / 8.0f);
    // for (auto &fCorner: fCorners) {
    //     fCorner = scalePointFromCenter(fCorner, frustrumCenter, 1);
    // }

    //assume a right-handed system and looking down on -Z corners are clockwise from top-right looking from camera
    //Near plane: 0, 1, 2, 3 CCW order: 3, 2, 1 (top left, bottom left, bottom right
    fPlanes[0] = convertPointsToFrustrumPlane(fCorners[3], fCorners[2], fCorners[1], Position + forward * (near + 0.1f));

    fPlanes[1] = convertPointsToFrustrumPlane(fCorners[4], fCorners[5], fCorners[6], Position + forward * (near + 0.1f));

    fPlanes[2] = convertPointsToFrustrumPlane(fCorners[7], fCorners[6], fCorners[2], Position + forward * (near + 0.1f));

    fPlanes[3] = convertPointsToFrustrumPlane(fCorners[0], fCorners[1], fCorners[5], Position + forward * (near + 0.1f));

    fPlanes[4] = convertPointsToFrustrumPlane(fCorners[3], fCorners[0], fCorners[4], Position + forward * (near + 0.1f));

    fPlanes[5] = convertPointsToFrustrumPlane(fCorners[1], fCorners[2], fCorners[6], Position + forward * (near + 0.1f));

    this->frustrumCorners = fCorners;
    this->frustrumPlanes = fPlanes;
}

[[deprecated]]
bool Camera::IsPointInFrustum(const Vector &point) const {
    //If point lies on side of planes normal vector it is inside

    for (int i = 0; i < 6; i++) {
        auto currentPlane = frustrumPlanes[i];
        auto planePointEq = currentPlane.A * point.x + currentPlane.B * point.y + currentPlane.C * point.z + currentPlane.D;
        if (planePointEq < 0) return false;
    }

    return true;
}


void Camera::ResetVelocityAlongWorldAxis(Vector axis) {
    this->cameraVelocity *= Vector(axis.x > 0 ? 0 : 1, axis.y > 0 ? 0 : 1, axis.z > 0 ? 0 : 1);
}
