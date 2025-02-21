//
// Created by cew05 on 11/07/2024.
//

#ifndef UNTITLED7_MODELSTRUCTS_H
#define UNTITLED7_MODELSTRUCTS_H

#include <iostream>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ModelTransformations.h"

struct ModelVertex {
    glm::vec3 position {};
    glm::vec2 textureCoord {};
};

struct UniqueVertex {
    // Positioning
    glm::vec3 chunkPosition {};
    glm::i8vec3 blockOffset {0,0,0};

    glm::vec3 modelVertex {};
    glm::vec3 normalAxis {};

    glm::vec2 textureCoord {};
    glm::bvec2 blockRotation {};
    GLbyte occlusion {3};
    GLbyte facing {};
    GLbyte lightLevel {15};
};

struct UniqueVertexOptimised {
    // Offset from chunk 0,0,0 max scale of 1/16 of a block
    // Original Model Position in 1/16 subblock measurements (from 0 to 1)
    glm::vec3 chunkPosOffset {};
    glm::i8vec3 modelVertex {};

    // Normal to vertex for block rotations
    glm::i8vec3 normalAxis {};
    glm::i8vec3 blockRotation {};

    // Block Mesh Texture info
    glm::i8vec3 textureCoord {};
    GLbyte occlusion {3};
};




struct Plane {
    glm::vec3 normal {};
    float distance {};

    void SetPlane(const glm::vec3& _pos, const glm::vec3& _normal) {
        normal = glm::normalize(_normal);
        distance = glm::dot(normal, _pos);
    }

    [[nodiscard]] float DistToPlane(const glm::vec3& _vertex) const {
        // Signed Distance from a given point (_vertex) to the plane
        return glm::dot(normal, _vertex) - distance;
    }
};

struct Frustrum {
    // INDEX SIDE : |0 LEFT | 1 RIGHT | 2 BOTTOM | 3 TOP | 4 NEAR | 5 FAR
    Plane planes[6] {};
};

enum FRUSTRUM : int{
    OUTSIDE, INTERSECTING, INSIDE,
};

struct BoundingVolume {

    [[nodiscard]] virtual int InFrustrum(const Frustrum& _frustrum, const Transformation& _transformation) const {
        return INSIDE;
    }
};

struct SphereBounds : public BoundingVolume {
    glm::vec3 centre {0.0f, 0.0f, 0.0f};
    float radius {0.0f};

    SphereBounds(const glm::vec3& _centre, float _radius) : BoundingVolume() {
        centre = _centre;
        radius = _radius;
    }

    [[nodiscard]] SphereBounds GetGlobalSphere(const Transformation& _transformation) const {
        // move sphere centre to model centre by using the model's transformation matrix
        glm::vec3 globalCentre = { _transformation.GetModelMatrix() * glm::vec4(centre, 1.0f)};

        // Get the largest scale of the object to use for radius
        glm::vec3 globalScale = _transformation.GetGlobalScale();
        float maxScale = std::max(std::max(globalScale.x, globalScale.y), globalScale.z);

        // Create a temp sphere struct using this global positioning info
        return {globalCentre, radius * maxScale * 0.5f};
    }

    [[nodiscard]] int InFrustrum(const Frustrum& _camFrustrum, const Transformation& _transformation) const override {
        SphereBounds globalSphere = GetGlobalSphere(_transformation);

        // test temp sphere in each frustrum plane
        bool inOrInFrontOfPlane;
        for (const auto& plane : _camFrustrum.planes) {
            float dist = plane.DistToPlane(globalSphere.centre);
            inOrInFrontOfPlane = (dist > -globalSphere.radius);

            // If sphere is behind plane, sphere is not in view
            if (!inOrInFrontOfPlane) {
                return OUTSIDE;
            }
        }

        // no checks failed
        return INSIDE;
    }
};

struct BoxBounds : public BoundingVolume {
    glm::vec3 centre {0.0f, 0.0f, 0.0f};
    float extent {0.0f};

    BoxBounds(const glm::vec3& _centre, float _extent) : BoundingVolume() {
        centre = _centre;
        extent = _extent;
    }

    [[nodiscard]] BoxBounds GetGlobalBoxBounds(const Transformation& _transformation) const {
        glm::vec3 globalCentre = { _transformation.GetModelMatrix() * glm::vec4(centre, 1.0f)};

        // Scaled orientation
        const glm::vec3 right = _transformation.GetRight() * extent;
        const glm::vec3 up = _transformation.GetUp() * extent;
        const glm::vec3 forward = _transformation.GetForward() * extent;

        const float newIi = std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, right)) +
                            std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, up)) +
                            std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, forward));

        const float newIj = std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, right)) +
                            std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, up)) +
                            std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, forward));

        const float newIk = std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, right)) +
                            std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, up)) +
                            std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, forward));

        return {globalCentre, std::max(std::max(newIi, newIj), newIk)};
    }

    [[nodiscard]] std::pair<glm::vec3, glm::vec3> GetMinMaxVertex() const {
        return {centre - extent, centre + extent};
    }

    [[nodiscard]] std::pair<glm::vec3, glm::vec3> GetMinMaxGlobalVertex(const Transformation& _transformation) const {
        BoxBounds globalBoxBounds = GetGlobalBoxBounds(_transformation);
        return {globalBoxBounds.centre - globalBoxBounds.extent, globalBoxBounds.centre + globalBoxBounds.extent};
    }

    [[nodiscard]] int InFrustrum(const Frustrum& _camFrustrum, const Transformation& _transformation) const override {
        BoxBounds globalBoxBounds = GetGlobalBoxBounds(_transformation);

        // test global box bounds in each frustrum plane
        bool inOrInFrontOfPlane;
        for (const auto& plane : _camFrustrum.planes) {
            float maxDist = globalBoxBounds.extent * (std::abs(plane.normal.x) + std::abs(plane.normal.y) + std::abs(plane.normal.z));
            inOrInFrontOfPlane = (-maxDist <= plane.DistToPlane(globalBoxBounds.centre));

            // If box is behind plane, sphere is not in view
            if (!inOrInFrontOfPlane) return OUTSIDE;
        }

        // no checks failed
        return INSIDE;
    }
};


/*
 * GENERATING BOUNDING OBJECTS
 */

// Obtain the minimum and maximum verticies from a vertex array
inline std::pair<glm::vec3, glm::vec3> minMaxVertex(const std::vector<ModelVertex>& _verticies) {
    auto minVertex = glm::vec3(std::numeric_limits<float>::max());
    auto maxVertex = glm::vec3(std::numeric_limits<float>::min());
    for (auto& vertex : _verticies) {
        minVertex.x = std::min(minVertex.x, vertex.position.x);
        minVertex.y = std::min(minVertex.y, vertex.position.y);
        minVertex.z = std::min(minVertex.z, vertex.position.z);

        maxVertex.x = std::max(maxVertex.x, vertex.position.x);
        maxVertex.y = std::max(maxVertex.y, vertex.position.y);
        maxVertex.z = std::max(maxVertex.z, vertex.position.z);
    }

    return {minVertex, maxVertex};
}

inline SphereBounds GenerateSphere(const std::vector<ModelVertex>& _verticies) {
    auto [minVertex, maxVertex] = minMaxVertex(_verticies);
    return {(maxVertex + minVertex) * 0.5f, glm::length(minVertex - maxVertex)};
}



inline std::unique_ptr<BoxBounds> GenerateBoxBounds(const std::vector<ModelVertex>& _verticies) {
    auto [minVertex, maxVertex] = minMaxVertex(_verticies);

    auto centre = (maxVertex + minVertex) * 0.5f;
    return std::make_unique<BoxBounds>(centre, glm::length(maxVertex - centre));
}

#endif //UNTITLED7_MODELSTRUCTS_H
