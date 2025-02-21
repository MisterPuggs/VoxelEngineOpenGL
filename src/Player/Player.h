//
// Created by cew05 on 01/08/2024.
//

#ifndef UNTITLED7_PLAYER_H
#define UNTITLED7_PLAYER_H

#include <cmath>
#include <glm/matrix.hpp>
#include "Camera.h"

#include "../World/Chunks/Chunk.h"

enum class MOVEMENTMODE {
        WALKING, FLYING, UNRESTRICTED, // ...
};

class Player {
    private:
        // Positioning info
        glm::vec3 position {};
        glm::vec3 lastPosition {};
        glm::vec3 lastStaticPosition {};

        float timeSinceOnGround {};
        float timeSinceStill {};
        bool canJump = true;
        bool inLiquid = false;

        float radius = 0.4f;
        float maxX {}, minX {};
        float maxZ {}, minZ {};
        float maxY {}, minY {};

        std::shared_ptr<Chunk> playerChunk {};

        // Movement Info
        MOVEMENTMODE movementMode = MOVEMENTMODE::UNRESTRICTED;
        bool canStartFly = true;
        bool jumpStarted = false;
        float timeSinceLastJump = 0;
        glm::vec3 vectorSpeed {};
        glm::vec3 vectorAcceleration {};
        glm::vec3 moveDirection {0, 0, 0};

        // Player normals
        glm::vec3 normalUp {};
        glm::vec3 normalRight {};
        glm::vec3 normalFront {};

        // Camera management
        glm::vec3 facingDirection {};
        float cameraSensitivity {};
        bool cameraMoved = true;

        std::unique_ptr<Camera> firstPerson = std::make_unique<Camera>();
        std::unique_ptr<Camera> thirdPerson = std::make_unique<Camera>();
        Camera* usingCamera = firstPerson.get();
        int currentCamera = 1;
        bool camSwitchToggle = false;

        // Player-World interaction
        float range = 7.5f;
        glm::vec3 unobstructedRayPosition {};
        bool lookingAtInteractable = false;

        std::unique_ptr<Block> blockInHand {};
        BlockType blockInHandType {STONE, 0};
        float mouseScroll = 0;

    public:
        Player(glm::vec3 _position, glm::vec3 _facingDirection);

        // Display
        void Display();

        // Movement
        void HandleMovement(Uint64 _deltaTicks);
        void FlyingMovement(const std::uint8_t* _keyInputs, float _seconds);
        void WalkingMovement(const std::uint8_t* _keyInputs, float _seconds);

        // Movement Parameters
        void UpdatePlayerChunk();
        void UpdateMaxPositions();
        void EnforcePositionBoundaries(float _seconds);
        void GetMovementFriction();

        // Player Camera
        void MouseLook(SDL_bool _mouseGrabbed);
        void SwitchCamera(const std::uint8_t* _keyInputs);

        // Block interactions
        void HandlePlayerInputs(const SDL_Event& _event);
        void GetUnobstructedRayPosition();
        void BreakBlock(glm::vec3 _rayPosition);
        void PlaceBlock(glm::vec3 _rayPosition);

        // Inventory
        void SelectHotbarItem();

        // Getters
        [[nodiscard]] Camera* GetUsingCamera() const { return usingCamera; }
        [[nodiscard]] glm::vec3 GetPosition() const { return position; }
        [[nodiscard]] bool CameraMoved() const { return cameraMoved; }
};


#endif //UNTITLED7_PLAYER_H
