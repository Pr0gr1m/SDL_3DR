#include "Camera.h"

#include <iostream>

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

// void Camera::MoveCameraThisTick() {
//     this->Position += cameraVelocity;
// }

void Camera::ResetVelocityAlongAxis(Vector axis) {
    this->cameraVelocity *= Vector(axis.x > 0 ? 0 : 1, axis.y > 0 ? 0 : 1, axis.z > 0 ? 0 : 1);
}
