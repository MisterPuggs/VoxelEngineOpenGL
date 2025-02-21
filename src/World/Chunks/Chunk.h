//
// Created by cew05 on 11/07/2024.
//

#ifndef UNTITLED7_CHUNK_H
#define UNTITLED7_CHUNK_H

#include <vector>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

#include "../../BlockModels/MaterialMesh.h"
#include "../../Player/Camera.h"
#include "../WorldGenConsts.h"
#include "../Biomes/Biome.h"

// CHUNK TYPEDEFS
namespace ChunkDataTypes {
    struct ChunkBlock {
        BlockType type {AIR, 0};
        BlockAttributes attributes;
    };

    struct LockableTerrainLayer {
        std::array<ChunkBlock, chunkArea> blockLayer;
        std::mutex layerLock;
    };

    typedef std::array<LockableTerrainLayer, chunkHeight> TerrainArray;
    typedef std::array<float, chunkArea> DataMap;
    typedef std::array<GLbyte, chunkArea> ByteMap;
}

/*
 * Simple data struct used to hold various maps of the chunk's blocks and biome information.
 */

struct ChunkData {
    // Biome Information
    Biome* biome {};

    // Initial terrain maps
    ChunkDataTypes::DataMap heightMap {};
    ChunkDataTypes::DataMap plantMap {};
};

/*
 * Houses a 3D array of blocks, of cubic size 16x16x16 (chunkSize^3). Resonsible for generating blocks from a given set
 * of maps (see ChunkData) and then generating biome-specific structures. Chunk also incorporates pointers to the
 * surrounding 10 adjacent chunks (diagonal top/bottoms not included) which can be used for mesh creation and various
 * block updates / interactions.
 */

class Chunk {
    private:
        // Chunk Culling and Display
        std::unique_ptr<BoxBounds> boxBounds {};
        Transformation cullingTransformation {};
        Transformation displayTransformation {};
        bool inCamera = true;
        bool needsMeshUpdates = false;
        bool unboundMeshChanges = false;

        // Chunk Terrain and Block Data
        std::unordered_map<BlockType, std::unique_ptr<Block>> uniqueBlockMap {};
        std::unordered_map<BlockType, std::unique_ptr<MaterialMesh>> uniqueMeshMap {};
        std::mutex meshMutex;
        ChunkDataTypes::TerrainArray terrainLayers {};
        bool generated = false;

        // Unique ChunkData and the adjacent Chunk pointers
        ChunkData chunkData;
        glm::vec3 chunkIndex {0, 0, 0};

        // Private functions for getting/setting blocks which non-chunks shouldn't access
        [[nodiscard]] ChunkDataTypes::ChunkBlock GetChunkBlockAtPosition(const glm::vec3& _blockPos);
        void SetChunkBlockAtPosition(const glm::vec3& _blockPos, const BlockType& _blockType);
        [[nodiscard]] BlockAttributes GetChunkBlockAttributesAtPosition(const glm::vec3& _blockPos);
        void SetChunkBlockAttributesAtPosition(const glm::vec3& _blockPos, const BlockAttributes& _attributes);

    public:
        Chunk(const glm::vec3& _chunkPosition, ChunkData _chunkData);
        ~Chunk();

        // Chunk Display
        void DisplaySolid();
        void DisplayTransparent();

        // Chunk Terrain and Structures Generation
        float GetHeightAt(int _x, int _z);
        void GenerateChunk();
        void CreateTerrain();
        void PaintTerrain();
        void SurfaceDecorations();
        [[nodiscard]] bool Generated() const { return generated; }
        [[nodiscard]] bool RegionGenerated() const;

        // Block Lighting
        void FloodFillFrom(const glm::vec3& _blockPos, bool _chunkOnly);
        void FloodFillSkyLight();

        // Chunk Block Meshes Creation / Updating
        void UpdateBlockMesh(Block* _meshBlock);
        void CreateChunkMeshes();
        void ApplyChunkLighting(std::vector<UniqueVertex>& _verticies, const glm::vec3& _position) const;
        void CalculateOcclusion(std::vector<UniqueVertex>& _verticies, Block& _block, const glm::vec3& _position);
        [[nodiscard]] std::vector<BLOCKFACE> GetHiddenFaces(glm::vec3 _blockPos);
        [[nodiscard]] std::vector<BLOCKFACE> GetShowingFaces(glm::vec3 _blockPos, const Block& _checkingBlock);
        [[nodiscard]] MaterialMesh* GetMeshFromBlock(const BlockType& _blockType);

        // Mesh Processing Signals + Mesh Binding
        void MarkForMeshUpdates();
        void BindChunkMeshes();
        [[nodiscard]] bool NeedsMeshUpdates() const { return needsMeshUpdates; }
        [[nodiscard]] bool UnboundMeshChanges() const { return unboundMeshChanges; }

        // Chunk Culling
        void CheckCulling(const Camera& _camera);
        [[nodiscard]] bool ChunkVisible() const { return inCamera; };

        // Chunk Block Interaction
        void BreakBlockAtPosition(glm::vec3 _blockPos);
        void PlaceBlockAtPosition(glm::vec3 _blockPos, BlockType _blockType);
        void SetBlockAtPosition(glm::vec3 _blockPos, const BlockType& _blockType) const;
        [[nodiscard]] ChunkDataTypes::ChunkBlock GetBlockAtPosition(glm::vec3 _blockPos) const;
        void SetBlockAttributesAtPosition(glm::vec3 _blockPos, const BlockAttributes& _attributes) const;
        [[nodiscard]] BlockAttributes GetBlockAttributesAtPosition(glm::vec3 _blockPos) const;

        // Chunk-Entity Collision
        [[nodiscard]] float GetTopLevelAtPosition(glm::vec3 _blockPos, float _radius) ;
        [[nodiscard]] float GetDistanceToBlockFace(glm::vec3 _blockPos, glm::vec3 _direction, float _radius) ;

        //
        [[nodiscard]] Block& GetBlockFromData(const BlockType& _blockType);
        [[nodiscard]] glm::vec3 GetIndex() const { return chunkIndex; }
        [[nodiscard]] glm::vec2 GetXZIndex() const { return {chunkIndex.x, chunkIndex.z}; }
        [[nodiscard]] std::shared_ptr<Chunk> GetChunkAtBlockPos(glm::vec3& _blockPos) const;
        [[nodiscard]] Biome* GetBiome() const { return chunkData.biome; }
};


#endif //UNTITLED7_CHUNK_H
