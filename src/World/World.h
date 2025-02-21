//
// Created by cew05 on 10/07/2024.
//

#ifndef UNTITLED7_WORLD_H
#define UNTITLED7_WORLD_H

#include <memory>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <random>

#include "../Player/Player.h"

#include "Biomes/Biome.h"
#include "Chunks/Chunk.h"
#include "Chunks/ChunkThreads.h"

enum class THREAD {
        CHUNKBUILDING, CHUNKMESHING, CHUNKLOADING, CHUNKLIGHTING // ...
};

struct LockableChunkPtr {
    std::shared_ptr<Chunk> chunkPtr {};
    mutable std::shared_mutex chunkLock;

    LockableChunkPtr& operator=(const std::shared_ptr<Chunk>& _chunkPtr) {
        if (_chunkPtr != nullptr) chunkPtr = _chunkPtr;
        return *this;
    }

    std::shared_ptr<Chunk> operator->() const {
        return chunkPtr;
    }
};

namespace WorldDataTypes {
    typedef std::array<std::array<LockableChunkPtr, 2000>, 2000> chunkArray;
}

class World {
    private:
        Transformation skyboxTransformation, sunTransformation, moonTransformation;
        std::unique_ptr<Block> skybox;

        // Sky decos: clouds, sun, moon, stars, etc
        std::unique_ptr<Block> sun, moon;
        unsigned int worldTicks = 0;
        unsigned int worldTime = 7*60; // minutes
        unsigned int worldDays = 0;

        // World Generation
        WorldDataTypes::chunkArray worldChunks {};
        std::vector<std::unique_ptr<Biome>> uniqueBiomes {};

        int displayingChunks {};

        // Threads
        ChunkThreads chunkBuilderThread = ChunkThreads("BUILDER_THREAD");
        ChunkThreads chunkMesherThread = ChunkThreads("MESHER_THREAD");
        ChunkThreads chunkLoaderThread = ChunkThreads("LOADER_THREAD");
        ChunkThreads chunkLighterThread = ChunkThreads("LIGHTING_THREAD");

        glm::ivec2 loadingIndex {0, 0}; // centre

    public:
        World();
        ~World();

        // Display
        void Display() const;
        void CheckCulling(const Camera& _camera);

        // Skybox and Decoratives
        void SetSkyboxProperties(const Player& player);
        void SetSkyboxPosition(glm::vec3 _position);
        void UpdateWorldTime(Uint64 _deltaTicks);

        // Thread Functions
        THREAD_ACTION_RESULT CreateChunk(const glm::ivec2& _chunkIndex, const glm::vec3& _blockPos);
        THREAD_ACTION_RESULT GenerateChunk(const glm::ivec2& _chunkIndex, const glm::vec3& _blockPos);
        THREAD_ACTION_RESULT GenerateChunkMesh(const glm::ivec2& _chunkIndex, const glm::vec3& _blockPos) const;
        THREAD_ACTION_RESULT FloodSkyLightingForChunk(const glm::ivec2& _chunkIndex, const glm::vec3& _blockPos);
        THREAD_ACTION_RESULT FloodSkyLightingFromPosition(const glm::ivec2& _chunkIndex, const glm::vec3& _blockPos);
        THREAD_ACTION_RESULT FloodBlockLightingFrom(const glm::ivec2& _chunkIndex, const glm::vec3& _blockPos);

        void ManageLoadedChunks(const std::shared_ptr<Chunk>& _currentChunk, const std::shared_ptr<Chunk>& _newChunk);
        THREAD_ACTION_RESULT CheckChunkLoaded(const glm::ivec2& _currentChunkPos, const glm::vec3& _newChunkPos);

        // ChunkData Generation functions
        static float GenerateBlockCavernosity(glm::vec2 _blockPos);
        static float GenerateBlockHollowness(glm::vec2 _blockPos);
        static float GenerateBlockHeight(glm::vec2 _blockPos);
        static int GenerateCaveChambers(glm::vec3 _blockPos, float _hmTopLevel, float _cavernosity, float _hollowness);
        static float GenerateBlockHeat(glm::vec3 _blockPos);
        static float GenerateBlockVegetation(glm::vec3 _blockPos, float _heat);
        BlockType GenerateBlockAtPosition(const glm::vec3& _blockPos) const;
        static ChunkData GenerateChunkData(glm::vec2 _chunkPosition);
        Biome* GenerateBiome(Biome::ID _biomeID);

        //
        void SetLoadingOrigin(const glm::vec3& _origin);
        void GenerateRequiredWorldRegion();
        void GenerateLoadableWorldRegion();

        void BindChunks() const;

        // Chunk Retrieval and Destruction
        [[nodiscard]] std::shared_ptr<Chunk> GetChunkAtBlockPosition(glm::vec3 _blockPos) const;
        [[nodiscard]] std::shared_ptr<Chunk> GetChunkAtIndex(glm::vec2 _chunkIndex) const;
        [[nodiscard]] std::shared_ptr<Chunk> GetChunkAtIndex(glm::vec3 _chunkIndex) const;
        THREAD_ACTION_RESULT DestroyChunkAtIndex(glm::vec3 _chunkIndex);
        THREAD_ACTION_RESULT CreateChunkAtIndex(glm::vec3 _chunkIndex, ChunkData _chunkData);


        [[nodiscard]] Biome* GetBiome(Biome::ID _biomeID);
        [[nodiscard]] ChunkThreads* GetThread(THREAD _thread);
};

inline std::unique_ptr<World> world {};

#endif //UNTITLED7_WORLD_H
