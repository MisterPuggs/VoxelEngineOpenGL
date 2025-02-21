//
// Created by cew05 on 08/07/2024.
//

#include "Camera.h"

#include "../Window.h"
#include "../Textures/TextureManager.h"

Camera::Camera() {
    // Set the perspective of the camera and update the uniform matrix in the shader
    perspective = glm::perspective(glm::radians(fovAngleY), window.GetAspectRatio(),
                                   minDistance, maxDistance);
    GLint uLocation = glGetUniformLocation(window.GetShader(), "matricies.uProjectionMatrix");
    if (uLocation < 0) printf("location not found [matricies.uProjectionMatrix]");
    else {
        glUniformMatrix4fv(uLocation, 1, GL_FALSE, &perspective[0][0]);
    }

    // Set camera info
    position = glm::vec3(0.0f, 40.0f, 0.0f);
    direction = glm::vec3(0.0f, 0.0f, 1.0f);
    normalUp = glm::vec3(0.0f, 1.0f, 0.0f);
    normalRight = glm::vec3(1.0f, 0.0f, 0.0f);

    // Set the view frustrum
    UpdateViewFrustrum();
}







void Camera::MoveTo(const glm::vec3 &_position) {
    position = _position;
}

void Camera::SetDirection(const glm::vec3 &_direction) {
    direction = glm::normalize(_direction);

    glm::vec3 horizDirection(direction.x, 0.0f, direction.z);
    normalRight = glm::normalize(glm::cross(horizDirection, normalUp));
}

void Camera::SetAngle(double _angleVert, double _angleHoriz) {
    angleVert = _angleVert;
    angleHoriz = _angleHoriz;
}

void Camera::UpdateLookatUniform() const {
    GLint uLocation;

    window.SetShader(Window::BASEMESH);
    uLocation = glGetUniformLocation(window.GetShader(), "matricies.uViewMatrix");
    if (uLocation < 0) printf("base location not found [matricies.uViewMatrix]");
    else glUniformMatrix4fv(uLocation, 1, GL_FALSE, &GetViewMatrix()[0][0]);

    window.SetShader(Window::BASEMESH);
}


void Camera::UpdateViewFrustrum() {
    // Get rows from projection matrix
    const float halfVertFarSide = maxDistance * tanf(fovAngleY * 0.5f);
    const float halfHoriFarSide = halfVertFarSide * window.GetAspectRatio() * 0.5f;
    const glm::vec3 farDistance = maxDistance * direction;

    // INDEX SIDE : |0 LEFT | 1 RIGHT | 2 BOTTOM | 3 TOP | 4 NEAR | 5 FAR
    vf.planes[0].SetPlane(position, glm::cross(normalUp, farDistance + normalRight*halfHoriFarSide));
    vf.planes[1].SetPlane(position, glm::cross(farDistance - normalRight * halfHoriFarSide, normalUp));
    vf.planes[2].SetPlane(position, glm::cross(farDistance + normalUp * halfVertFarSide, normalRight));
    vf.planes[3].SetPlane(position, glm::cross(normalRight, farDistance - normalUp * halfVertFarSide));
    vf.planes[4].SetPlane(position+minDistance*direction, direction);
    vf.planes[5].SetPlane(position+farDistance, -direction);
}