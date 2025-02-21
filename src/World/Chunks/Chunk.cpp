//
// Created by cew05 on 11/07/2024.
//

#include "Chunk.h"

#include <glm/gtc/noise.hpp>
#include <memory>
#include <utility>

#include "../../Blocks/CreateBlock.h"
#include "../../GlobalStates.h"
#include "../World.h"

/*
 * CHUNK
 */

Chunk::Chunk(const glm::vec3& _chunkPosition, ChunkData _chunkData) {
    // Update the chunkPosition and chunkBounds transformations
    displayTransformation.SetPosition(_chunkPosition * (float)chunkSize);
    displayTransformation.UpdateModelMatrix();

    cullingTransformation.SetPosition(_chunkPosition * (float)chunkSize);
    cullingTransformation.UpdateModelMatrix();

    // Set chunk position
    chunkIndex = _chunkPosition;

    // Create bounding box for the chunk (assume max 16x256x16 volume)
    boxBounds = std::move(GenerateBoxBounds({{glm::vec3(0,0,0)},
                                             {glm::vec3(chunkSize,chunkHeight,chunkSize)}}));

    // Set chunkData
    chunkData = _chunkData;
}

Chunk::~Chunk() {
    uniqueBlockMap.clear();
    std::unique_lock meshLock(meshMutex);
    uniqueMeshMap.clear();

//    printf("CHUNK AT %f %f DESTROYED\n", chunkIndex.x, chunkIndex.z);
};



/*
 * Displays only the block meshes which have no transparent elements
 */

void Chunk::DisplaySolid() {
    if (!inCamera) return;

    if (USE_WIREFRAME) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Draw only the blocks that are solid and prevent creation of new meshes whilst drawing
    std::unique_lock lock(meshMutex);
    for (const auto& mesh : uniqueMeshMap) {
        auto block = mesh.second->GetBlock();
        if (block != nullptr && block->GetSharedAttribute(BLOCKATTRIBUTE::TRANSPARENT) > 0) continue;
        mesh.second->DrawMesh(displayTransformation);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}



/*
 * Displays only the block meshes which have transparent elements
 */

void Chunk::DisplayTransparent() {
    if (!inCamera) return;

    if (USE_WIREFRAME) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Draw only the blocks that are transparent
    for (const auto& mesh : uniqueMeshMap) {
        if (mesh.second->GetBlock()->GetSharedAttribute(BLOCKATTRIBUTE::TRANSPARENT) == 0) continue;
        mesh.second->DrawMesh(displayTransformation);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}


void Chunk::FloodFillFrom(const glm::vec3 &_blockPos, bool _chunkOnly) {



    glm::vec3 adjDirs[] = {dirLeft, dirRight, dirFront, dirBack};
    glm::vec3 floodDirs[] = {dirLeft, dirRight, dirFront, dirBack, dirBottom};

    ChunkDataTypes::ChunkBlock thisBlockData = GetBlockAtPosition(_blockPos);
    Block thisBlockObj = GetBlockFromData(thisBlockData.type);

    GLbyte filter = thisBlockObj.GetSharedAttribute(BLOCKATTRIBUTE::TRANSPARENT);
    if (filter == 0) {
        // Solid Block. Set skylight to 0. No light to flood to surroundings from this block
        thisBlockData.attributes.skyLight = 0;
        SetBlockAttributesAtPosition(_blockPos, thisBlockData.attributes);

        // Flood to surroundings and end
        for (const auto &dir: floodDirs) {
            // Will only flood to adjacent if they are part of the chunk
            glm::vec3 blockPos = _blockPos + dir;
            if (blockPos.z < 0 || blockPos.z >= chunkSize || blockPos.x < 0 || blockPos.x >= chunkSize)
                continue;
            else if (GetChunkBlockAttributesAtPosition(blockPos).skyLight < thisBlockData.attributes.skyLight) {
                // Will only flood if it can increase light level
                FloodFillFrom(blockPos, _chunkOnly);
            }
        }

        return;
    }


    // Fetch light from above first
    GLbyte lightAbove = 15; // Use WorldTime to get light
    if (_blockPos.y + dirTop.y < chunkHeight) {
        ChunkDataTypes::ChunkBlock blockAbove = GetBlockAtPosition(_blockPos + dirTop);
        lightAbove = GLbyte(blockAbove.attributes.skyLight - filter);
        lightAbove = std::max((GLbyte) 0, lightAbove);
    }

    thisBlockData.attributes.skyLight = std::max(lightAbove, thisBlockData.attributes.skyLight);

    // Pull in surrounding skylight values
    for (const auto &dir: adjDirs) {
        ChunkDataTypes::ChunkBlock blockAdj = GetBlockAtPosition(_blockPos + dir);
        GLbyte lightAdj = GLbyte(blockAdj.attributes.skyLight - filter);
        thisBlockData.attributes.skyLight = std::max(lightAdj, thisBlockData.attributes.skyLight);
    }

    SetBlockAttributesAtPosition(_blockPos, thisBlockData.attributes);

    // Push out new skylight value
    for (const auto &dir: floodDirs) {
        glm::vec3 blockPos = _blockPos + dir;
        if (blockPos.z < 0 || blockPos.z >= chunkSize || blockPos.x < 0 || blockPos.x >= chunkSize)
            continue;
        else if (GetChunkBlockAttributesAtPosition(blockPos).skyLight < thisBlockData.attributes.skyLight) {
            // Will only flood if it can increase light level
            FloodFillFrom(blockPos, _chunkOnly);
        }
    }
}


void Chunk::FloodFillSkyLight() {
//    auto st = std::chrono::high_resolution_clock::now();

    for (int x = 0; x < chunkSize; ++x) {
        for (int z = 0; z < chunkSize; ++z) {

//            int maxY = (int)GetHeightAt(x, z);
            for (int y = chunkHeight - 1; y >= 0; --y) {
                ChunkDataTypes::ChunkBlock thisBlockData = GetChunkBlockAtPosition({x,y,z});
                Block thisBlockObj = GetBlockFromData(thisBlockData.type);

                GLbyte filter = thisBlockObj.GetSharedAttribute(BLOCKATTRIBUTE::TRANSPARENT);
                if (filter == 0) {
                    thisBlockData.attributes.skyLight = 0;
                    SetBlockAttributesAtPosition({x,y,z}, thisBlockData.attributes);
                    continue;
                }

                if (thisBlockData.type == BlockType{AIR, 0}) {
                    filter = 0;
                }

                GLbyte aboveLight = 15; // get world lightlevel
                if (y + 1 < chunkHeight) {
                    aboveLight = GLbyte(GetBlockAttributesAtPosition({x,y+1,z}).skyLight - filter);
                }
                thisBlockData.attributes.skyLight = aboveLight;
                SetBlockAttributesAtPosition({x,y,z}, thisBlockData.attributes);
            }
        }
    }
}




/*
 * Updates the mesh of a given block. This block must belong to the unique blocks of the chunk, otherwise the blockMesh
 * will not be obtained, even if it is the same blockType being updated.
 */

void Chunk::UpdateBlockMesh(Block* _meshBlock) {
    MaterialMesh* blockMesh = GetMeshFromBlock(_meshBlock->GetBlockType());
    if (blockMesh == nullptr || blockMesh->GetBlock()->GetBlockType().blockID == AIR) return;

    blockMesh->ResetVerticies();

    for (int x = 0; x < chunkSize; x++) {
        for (int z = 0; z < chunkSize; z++) {
            for (int y = 0; y < chunkHeight; y++) {
                ChunkDataTypes::ChunkBlock block = GetBlockAtPosition({x,y,z});
                Block blockPtr = GetBlockFromData(block.type);

                if (block.type == _meshBlock->GetBlockType()) {
                    std::vector<UniqueVertex> verticies = blockPtr.GetFaceVerticies(
                            GetShowingFaces({x,y,z}, blockPtr),
                            block.attributes);
                    blockMesh->AddVerticies(verticies, {x,y,z});
                }
            }
        }
    }

    needsMeshUpdates = false;
    unboundMeshChanges = true;
}

/*
 * Goes through all positions within the chunk and adds the visible verticies of blocks to their corresponding meshes.
 * Can affect all meshes except any potential air mesh.
 * Will only act upon meshes where "oldMesh" is true
 */

void Chunk::CreateChunkMeshes() {
    for (auto& mesh : uniqueMeshMap) {
        if (mesh.second->IsOld()) {
            mesh.second->ResetVerticies();
        }
    }

    for (int x = 0; x < chunkSize; ++x) {
        for (int z = 0; z < chunkSize; ++z) {
            for (int y = 0; y < chunkHeight; ++y) {
                ChunkDataTypes::ChunkBlock block = GetChunkBlockAtPosition({x,y,z});
                if (block.type.blockID == AIR) continue;

                MaterialMesh* blockMesh = GetMeshFromBlock(block.type);
                if (!blockMesh->IsOld()) continue;

                // Get Visible Verticies
                Block blockPtr = GetBlockFromData(block.type);
                std::vector<UniqueVertex> verticies = blockPtr.GetFaceVerticies(
                        GetShowingFaces({x,y,z}, blockPtr),
                        block.attributes);

                // Calculate Occlusion
                CalculateOcclusion(verticies, blockPtr, {x,y,z});

                // Retrieve Lighting
                ApplyChunkLighting(verticies, {x,y,z});

                // Add to blockMesh
                blockMesh->AddVerticies(verticies, {x,y,z});
            }
        }
    }

    for (auto& mesh : uniqueMeshMap) {
        if (mesh.second->IsOld()) {
            mesh.second->MarkReadyToBind();
        }
    }

    needsMeshUpdates = false;
    unboundMeshChanges = true;
}



void Chunk::MarkForMeshUpdates() {
    needsMeshUpdates = true;
}


void Chunk::BindChunkMeshes() {
    for (auto& mesh : uniqueMeshMap) {
        if (mesh.second->ReadyToBind()) {
            mesh.second->BindMesh();
        }
    }

    unboundMeshChanges = false;
}

/*
 * Returns the FaceIDs of the obscured faces of a block at a given position. Faces should be fully obscured to be
 * considered hidden.
 */

std::vector<BLOCKFACE> Chunk::GetHiddenFaces(glm::vec3 _blockPos) {
    std::vector<BLOCKFACE> hiddenFaces {};
    std::vector<BLOCKFACE> faces {TOP, BOTTOM, FRONT, BACK, RIGHT, LEFT};
    std::vector<glm::vec3> positionOffsets {
            glm::vec3{0, 1, 0}, glm::vec3{0, -1, 0}, glm::vec3{-1, 0, 0},
            glm::vec3{1,0,0}, glm::vec3{0, 0, 1}, glm::vec3{0, 0, -1}};

    ChunkDataTypes::ChunkBlock checkingBlock = GetBlockAtPosition(_blockPos);
    if (checkingBlock.type == BlockType{AIR, 0}) return faces; // Air block
    Block checkingPtr = GetBlockFromData(checkingBlock.type);

    for (int i = 0; i < faces.size(); i++) {
        ChunkDataTypes::ChunkBlock blockAtFace = GetBlockAtPosition(_blockPos + positionOffsets[i]);
        Block facePtr = GetBlockFromData(blockAtFace.type);

        // transparent blocks only show when there is air
        if (checkingPtr.GetSharedAttribute(BLOCKATTRIBUTE::TRANSPARENT) > 0) {
            // Is air, face is not hidden
            if (checkingBlock.type != BlockType{AIR, 0}) {
                continue;
            }
        }

        // Normal blocks may show if the block on the face is transparent
        else if (facePtr.GetSharedAttribute(BLOCKATTRIBUTE::TRANSPARENT) > 0) {
            continue;
        }

        hiddenFaces.push_back(faces[i]);
    }

    return hiddenFaces;
}



/*
 * Returns the FaceIDs of the visible faces of a block at a given position. Faces are considered visible unless fully
 * obscured.
 */
std::vector<BLOCKFACE> Chunk::GetShowingFaces(glm::vec3 _blockPos, const Block& _checkingBlock) {
    std::vector<BLOCKFACE> showingFaces {}, checkingFaces = {TOP, BOTTOM, FRONT, BACK, RIGHT, LEFT};
    std::vector<glm::vec3> positionOffsets {
            dirTop, dirBottom, dirFront,
            dirBack, dirRight, dirLeft};

    if (_checkingBlock.GetSharedAttribute(BLOCKATTRIBUTE::BLOCKMODEL) == BLOCKMODEL::PLANT)
        return {FRONT, BACK};

    // Check for non-transparent block on each face (or non-same transparent block for a transparent block)
    for (int i = 0; i < checkingFaces.size(); i++) {
        ChunkDataTypes::ChunkBlock faceBlockData = GetBlockAtPosition(_blockPos + positionOffsets[i]);
        Block faceBlock = GetBlockFromData(faceBlockData.type);

        if (Block::BlockFaceVisible(_checkingBlock, faceBlock, checkingFaces[i]))
            showingFaces.push_back(checkingFaces[i]);
    }

    return showingFaces;
}

void Chunk::ApplyChunkLighting(std::vector<UniqueVertex> &_verticies, const glm::vec3& _position) const {
    for (auto& vertex : _verticies) {
        glm::vec3 adjDir;

        switch ((BLOCKFACE)vertex.facing) {
            case FRONT:
                adjDir = dirFront;
                break;

            case BACK:
                adjDir = dirBack;
                break;

            case LEFT:
                adjDir = dirLeft;
                break;

            case RIGHT:
                adjDir = dirRight;
                break;

            case TOP:
                adjDir = dirTop;
                break;

            case BOTTOM:
                adjDir = dirBottom;
                break;

            default:
                vertex.lightLevel = 0;
                continue;
        }

        BlockAttributes blockAtFace = GetBlockAttributesAtPosition(_position + adjDir);
        GLbyte light = std::max(blockAtFace.skyLight, blockAtFace.blockLight);
        vertex.lightLevel = light;
    }
}

void Chunk::CalculateOcclusion(std::vector<UniqueVertex>& _verticies, Block& _block, const glm::vec3& _position) {
    // No changes necessary
    if (_block.GetSharedAttribute(BLOCKATTRIBUTE::CANBEOCCLUDED) == 0) return;

    // Check adjacent blocks for each vertex
    for (auto& vertex : _verticies) {
        // Testing blocks
        ChunkDataTypes::ChunkBlock chunkBlock;
        Block block;
        std::array<glm::vec3, 3> adjacentPositions {};              // sideA, sideB, corner
        std::array<bool, 3> adjacentOccluded {false, false, false}; // sideA, sideB, corner

        // which blocks are checked depends on which model vertex is in use

        // FRONT BACK FACE
        if (vertex.normalAxis.x != 0) {
            float x = vertex.normalAxis.x;
            if (vertex.modelVertex.y == 0 && vertex.modelVertex.z == 0)
                adjacentPositions = {glm::vec3{x, 1, 0}, glm::vec3{x,0,-1}, glm::vec3{x, 1, -1}};
            if (vertex.modelVertex.y == 0 && vertex.modelVertex.z == 1)
                adjacentPositions = {glm::vec3{x, 1, 0}, glm::vec3{x,0,1},  glm::vec3{x, 1, 1}};
            if (vertex.modelVertex.y == -1 && vertex.modelVertex.z == 0)
                adjacentPositions = {glm::vec3{x, -1, 0}, glm::vec3{x, 0, -1}, glm::vec3{x,-1,-1}};
            if (vertex.modelVertex.y == -1 && vertex.modelVertex.z == 1)
                adjacentPositions = {glm::vec3{x, -1, 0}, glm::vec3{x, 0, 1}, glm::vec3{x,-1,1}};
        }

        // TOP BOTTOM FACE
        if (vertex.normalAxis.y != 0) {
            float y = vertex.normalAxis.y;
            if (vertex.modelVertex.x == 0 && vertex.modelVertex.z == 0)
                adjacentPositions = {glm::vec3{-1, y, 0}, glm::vec3{0,y,-1}, glm::vec3{-1, y, -1}};
            if (vertex.modelVertex.x == 0 && vertex.modelVertex.z == 1)
                adjacentPositions = {glm::vec3{-1, y, 0}, glm::vec3{0,y,1},  glm::vec3{-1, y, 1}};
            if (vertex.modelVertex.x == 1 && vertex.modelVertex.z == 0)
                adjacentPositions = {glm::vec3{1,y,0}, glm::vec3{0, y, -1}, glm::vec3{1,y,-1}};
            if (vertex.modelVertex.x == 1 && vertex.modelVertex.z == 1)
                adjacentPositions = {glm::vec3{1, y, 0}, glm::vec3{0, y, 1}, glm::vec3{1,y,1}};
        }

        // LEFT RIGHT FACE
        if (vertex.normalAxis.z != 0) {
            float z = vertex.normalAxis.z;
            if (vertex.modelVertex.x == 0 && vertex.modelVertex.y == 0)
                adjacentPositions = {glm::vec3{-1, 0, z}, glm::vec3{0,1,z}, glm::vec3{-1, 1, z}};
            if (vertex.modelVertex.x == 1 && vertex.modelVertex.y == 0)
                adjacentPositions = {glm::vec3{1, 0, z}, glm::vec3{0,1,z},  glm::vec3{1, 1, z}};
            if (vertex.modelVertex.x == 0 && vertex.modelVertex.y == -1)
                adjacentPositions = {glm::vec3{-1,0,z}, glm::vec3{0, -1, z}, glm::vec3{-1,-1,z}};
            if (vertex.modelVertex.x == 1 && vertex.modelVertex.y == -1)
                adjacentPositions = {glm::vec3{1, 0, z}, glm::vec3{0, -1, z}, glm::vec3{1,-1,z}};
        }


        // For each adjacent block
        for (int a = 0; a < 3; a++) {
            chunkBlock = GetBlockAtPosition(_position + adjacentPositions[a]);
            block = GetBlockFromData(chunkBlock.type);

            // mark block as occluding
            adjacentOccluded[a] = (block.GetSharedAttribute(BLOCKATTRIBUTE::CANOCCLUDE) == 1);

            // if both sides are occluding, exit early as corner does not impact
            if (adjacentOccluded[0] && adjacentOccluded[1])
                break;
        }


        // Calculate final occlusion value
        GLbyte occlusion = 3;
        if (adjacentOccluded[0] && adjacentOccluded[1])
            occlusion = 0;
        else if ((adjacentOccluded[0] || adjacentOccluded[1]) && adjacentOccluded[2])
            occlusion = 1;
        else if (adjacentOccluded[0] ^ adjacentOccluded[1] ^ adjacentOccluded[2])
            occlusion = 2;

        vertex.occlusion = occlusion;
    }
}



MaterialMesh* Chunk::GetMeshFromBlock(const BlockType& _blockType) {
    if (uniqueMeshMap.count(_blockType) == 0) {
        std::unique_lock lock(meshMutex);
        uniqueMeshMap[_blockType] = std::make_unique<MaterialMesh>(&GetBlockFromData(_blockType));
    }

    return uniqueMeshMap[_blockType].get();
}



/*
 * Updates inCamera value with if / not the chunk is within the player camera's view frustrum
 */

void Chunk::CheckCulling(const Camera& _camera) {
//    glm::vec3 minDir = _camera.GetDirection() * _camera.GetMinMaxDistance().first;
//    glm::vec3 maxDir = _camera.GetDirection() * _camera.GetMinMaxDistance().second;
//
//    float minLeft, minRight, maxLeft, maxRight;
//
//
//
//
//
    if (boxBounds == nullptr) return;
    inCamera = boxBounds->InFrustrum(_camera.GetCameraFrustrum(), cullingTransformation);
}


float Chunk::GetHeightAt(int _x, int _z) {
    float hmTopLevel = chunkData.heightMap[_x + _z * chunkSize];
    return hmTopLevel;
}


/*
 * Generates the chunk's blocks into the 3d terrain array using stored data maps
 */

void Chunk::GenerateChunk() {
    // Populate the terrain array solid/nonSolid
    CreateTerrain();

    // Alter the solid blocks to introduce Terrain Blocks
    PaintTerrain();

    // Foliage and Natural Decorations
    SurfaceDecorations();

    // Generate any structures that appear

    //
    MarkForMeshUpdates();
    generated = true;
}



/*
 * Generates the chunk terrain and the meshes of the chunk
 */

void Chunk::CreateTerrain() {
    auto st = std::chrono::high_resolution_clock::now();

    for (int x = 0; x < chunkSize; x++) {
        for (int z = 0; z < chunkSize; z++) {
            glm::vec2 blockMapPos = glm::vec2{x,z} + (GetXZIndex() * (float)chunkSize);

            // Fetch map values
            float hmTopLevel = chunkData.heightMap[x + z * chunkSize];
            float cavernosity = World::GenerateBlockCavernosity(blockMapPos);
            float hollowness = World::GenerateBlockHollowness(blockMapPos); // change to hollowness

            int maxY = std::max((int)hmTopLevel + 1, WATERLEVEL + 1);
            for (int y = 0; y < maxY; y++) {
                glm::vec3 blockPos = glm::vec3(x, y, z) + (chunkIndex * (float)chunkSize);
                BlockType generatingBlockData;
                Block generatingBlock;

                // Utilise BlockDensity to determine Solid / Air
                // Above TopLevel height is always air and below minimum y is always solid
                int blockDensity = World::GenerateCaveChambers(blockPos, hmTopLevel, cavernosity, hollowness);
                generatingBlockData = BlockType{(blockDensity < 0 ? AIR : STONE), 0};

                SetChunkBlockAtPosition({x, y, z}, generatingBlockData);
            }
        }
    }

    auto et = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(et - st).count();
    printf("took %lld ms\n", duration / 1000000);
}

void Chunk::PaintTerrain() {
    for (int x = 0; x < chunkSize; x++) {
        for (int z = 0; z < chunkSize; z++) {

            // Fetch map values
            glm::vec2 blockMapPos = glm::vec2{x, z} + (GetXZIndex() * (float) chunkSize);
            float hmTopLevel = chunkData.heightMap[x + z * chunkSize];

            int maxY = std::max((int)hmTopLevel + 1, WATERLEVEL + 1);
            for (int y = 0; y < maxY; y++) {

                // If the Block is air and below toplevel, then a cave has been generated.
                BlockType generatedSolid = GetChunkBlockAtPosition({x,y,z}).type;
                if (generatedSolid == BlockType{AIR, 0} && (float)y <= hmTopLevel) {
                    continue; // next y
                }

                // Generate Block for position
                glm::vec3 blockPos = glm::vec3(x, y, z) + (chunkIndex * (float)chunkSize);
                BlockType generatingBlockData = chunkData.biome->GetBlockType(hmTopLevel, blockPos.y);
                Block generatingBlock = GetBlockFromData(generatingBlockData);

                // If a block has already been generated for this position and has higher gen priority than the current
                // block attempting to generate, then ignore new gen attempt. Equivalent gen = newest overwrite

                BlockType generatedType = GetChunkBlockAtPosition({x,y,z}).type;
                if (generatedType != BlockType{AIR, 0}) {
                    Block generatedBlock = GetBlockFromData(generatedType);
                    GLbyte generatedPriority = generatedBlock.GetSharedAttribute(BLOCKATTRIBUTE::GENERATIONPRIORITY);
                    GLbyte generatingPriority = generatingBlock.GetSharedAttribute(BLOCKATTRIBUTE::GENERATIONPRIORITY);

                    if (generatedPriority > generatingPriority) continue;
                }

                // Set block and make a new unique blockptr if required
                SetChunkBlockAtPosition({x, y, z}, generatingBlockData);

                // Generate Unique Block Data
                BlockAttributes blockAttributes;
                blockAttributes.halfRightRotations = generatingBlock.GetRandomRotation();
                blockAttributes.topFaceDirection = generatingBlock.GetRandomTopFaceDirection();
                blockAttributes.subBlockOffset = generatingBlock.GetRandomSubOffset(blockPos);
                SetChunkBlockAttributesAtPosition({x,y,z}, blockAttributes);
            }
        }
    }
}




/*
 *
 */

void Chunk::SurfaceDecorations() {
    for (int x = 0; x < chunkSize; x++) {
        for (int z = 0; z < chunkSize; z++) {
            glm::vec3 blockPos = {x, chunkData.heightMap[x + z * chunkSize], z};

            // Only plant on grass/dirt variants
            ChunkDataTypes::ChunkBlock rootBlock = GetBlockAtPosition(blockPos);
            if (rootBlock.type.blockID != GRASS) continue;

            // Plant Density and biome determines foliage type
            float plantDensity = chunkData.plantMap[x + z * chunkSize];
            Biome::FOLIAGE foliageType = chunkData.biome->GetFoliage(plantDensity);

            // Small Plants
            if (foliageType == Biome::FOLIAGE::NONE)
                continue;
            else if ((int)foliageType < (int)Biome::FOLIAGE::STRUCTURE_TYPE) {
                int plantHeight = 0;
                BlockType plantBlockType = chunkData.biome->BuildFoliage(foliageType, plantDensity, &plantHeight);
                Block plantBlock = GetBlockFromData(plantBlockType);

                glm::i8vec3 subOffset = plantBlock.GetRandomSubOffset(blockPos);

                for (int i = 1; i <= plantHeight; i++) {
                    SetBlockAtPosition(blockPos + (dirTop * (float) i), plantBlockType);

                    BlockAttributes blockAttributes;
                    blockAttributes.subBlockOffset = subOffset;
                    SetBlockAttributesAtPosition(blockPos + (dirTop * (float) i), blockAttributes);
                }
            }

            // Large Plant Structure
            else {
                chunkData.biome->LoadStructure((Biome::STRUCTURES)foliageType);
                glm::vec3 plantPos = blockPos + dirTop;

                bool doonce = false;
                bool completed = false;
                while (!completed) {
                    StructBlockData foliageBlock = chunkData.biome->BuildStructure(&completed, 1.0f);

                    if (!doonce && !completed) {
                        int maxB = rand() % 5;
                        for (int b = 0; b < maxB; b++) {
                            SetBlockAtPosition(plantPos, {WOOD, 0});
                            plantPos += dirTop;
                        }

                        doonce = true;
                    }

                    ChunkDataTypes::ChunkBlock loadedBlockType = GetBlockAtPosition(foliageBlock.blockPos + plantPos);
                    Block loadingBlock = GetBlockFromData(foliageBlock.blockType);

                    // Ensure vegetation can overwrite any current blocks in that position before placing
                    if (loadedBlockType.type != BlockType{AIR, 0}) {
                        Block generatedBlock = GetBlockFromData(loadedBlockType.type);
                        GLbyte generatedPriority = generatedBlock.GetSharedAttribute(BLOCKATTRIBUTE::GENERATIONPRIORITY);
                        GLbyte generatingPriority = loadingBlock.GetSharedAttribute(BLOCKATTRIBUTE::GENERATIONPRIORITY);

                        if (generatedPriority > generatingPriority) continue;
                    }

                    SetBlockAtPosition(foliageBlock.blockPos + plantPos, foliageBlock.blockType);

                    BlockAttributes blockAttributes;
                    blockAttributes.subBlockOffset = loadingBlock.GetRandomSubOffset(foliageBlock.blockPos + plantPos);
                    SetBlockAttributesAtPosition(foliageBlock.blockPos + plantPos, blockAttributes);
                }
            }
        }
    }
}

/*
 * Checks if any adjacent chunk, or this chunk is not generated. Returns true if all chunks (and this chunk) have
 * been generated
 */

bool Chunk::RegionGenerated() const {
    auto adjacentDirs = { dirFront, dirLeft, dirBack, dirRight };

    bool adjGenerated = true;
    for (const auto& adjDir : adjacentDirs) {
        auto adjChunk = world->GetChunkAtIndex(chunkIndex + adjDir);
        if (adjChunk == nullptr || !adjChunk->Generated()) {
            adjGenerated = false;
            break;
        }
    }

    return generated && adjGenerated;
}

/*
 * Set the block at the given block position to air, and update the meshes of the broken block, and surrounding blocks
 */

void Chunk::BreakBlockAtPosition(glm::vec3 _blockPos) {
    PlaceBlockAtPosition(_blockPos, {AIR, 0});
}



/*
 * Set the block at the given position to the given block type. Will construct a new unique block instance if the block
 * is new to the chunk. Then updates the meshes of the newly placed block and adjacent blocks.
 */

void Chunk::PlaceBlockAtPosition(glm::vec3 _blockPos, BlockType _blockType) {
    auto blockChunk = GetChunkAtBlockPos(_blockPos);
    if (blockChunk == nullptr) return;


    // directions of adjacent blocks
    std::array<glm::vec3, 7> blockPositions {_blockPos, _blockPos + dirTop, _blockPos + dirBottom, _blockPos + dirLeft,
                                             _blockPos + dirRight, _blockPos + dirFront, _blockPos + dirBack};

    // Mark original block's mesh for recreation and add original block mesh's chunk to list
    ChunkDataTypes::ChunkBlock originalBlock = blockChunk->GetChunkBlockAtPosition(_blockPos);
    MaterialMesh* blockMesh = blockChunk->GetMeshFromBlock(originalBlock.type);
    if (blockMesh != nullptr) {
        blockMesh->MarkOld();
    }

    // Place new block at position
    blockChunk->SetChunkBlockAtPosition(_blockPos, _blockType);
    blockChunk->MarkForMeshUpdates();

    for (auto& blockPosition : blockPositions) {
        auto chunkAtPosition = blockChunk->GetChunkAtBlockPos(blockPosition);
        if (chunkAtPosition != nullptr) {
            chunkAtPosition->MarkForMeshUpdates();

            // mark new block's mesh and meshes of adjacent blocks for updates
            ChunkDataTypes::ChunkBlock chunkBlock = chunkAtPosition->GetBlockAtPosition(blockPosition);
            MaterialMesh* chunkMesh = chunkAtPosition->GetMeshFromBlock(chunkBlock.type);
            if (chunkMesh != nullptr) chunkMesh->MarkOld();
        }
    }
}



/*
 * Sets the block at given position with the given type. blockPos is assumed to have values within 0 - 15
 */

void Chunk::SetChunkBlockAtPosition(const glm::vec3 &_blockPos, const BlockType& _blockType) {
    // Set block and clear attributes
    std::unique_lock lockGuard(terrainLayers[(int)_blockPos.y].layerLock);
    terrainLayers[(int)_blockPos.y].blockLayer[(int)_blockPos.x + (int)_blockPos.z * chunkSize] = {_blockType, {}};

    if (uniqueBlockMap[_blockType] == nullptr) {
        uniqueBlockMap[_blockType] = CreateBlock(_blockType);
    }
}

/*
 * Obtains the chunk that the provided position is within, and then sets the block in that chunk to the specified type.
 */

void Chunk::SetBlockAtPosition(glm::vec3 _blockPos, const BlockType& _blockType) const {
    auto blockChunk = GetChunkAtBlockPos(_blockPos);
    if (blockChunk == nullptr) return;

    blockChunk->SetChunkBlockAtPosition(_blockPos, _blockType);
}



/*
 * Fetches block at position. Assumes provided position values are within 0 - 15.
 */

ChunkDataTypes::ChunkBlock Chunk::GetChunkBlockAtPosition(const glm::vec3& _blockPos) {
    std::unique_lock lockGuard(terrainLayers[(int)_blockPos.y].layerLock);
    return terrainLayers[(int)_blockPos.y].blockLayer[(int)_blockPos.x + (int)_blockPos.z * chunkSize];
}


BlockAttributes Chunk::GetChunkBlockAttributesAtPosition(const glm::vec3 &_blockPos) {
    std::unique_lock lockGuard(terrainLayers[(int)_blockPos.y].layerLock);
    return terrainLayers[(int)_blockPos.y].blockLayer[(int)_blockPos.x + (int)_blockPos.z * chunkSize].attributes;
}

void Chunk::SetChunkBlockAttributesAtPosition(const glm::vec3 &_blockPos, const BlockAttributes &_attributes) {
    std::unique_lock lockGuard(terrainLayers[(int)_blockPos.y].layerLock);
    terrainLayers[(int)_blockPos.y].blockLayer[(int)_blockPos.x + (int)_blockPos.z * chunkSize].attributes = _attributes;
}


/*
 * Obtains the chunk that the provided position is within, and gets the block in that chunk
 */

ChunkDataTypes::ChunkBlock Chunk::GetBlockAtPosition(glm::vec3 _blockPos) const {
    glm::vec3 blockPos = _blockPos;
    auto blockChunk = GetChunkAtBlockPos(_blockPos);
    if (blockChunk == nullptr) return {};

    return blockChunk->GetChunkBlockAtPosition(_blockPos);
}


void Chunk::SetBlockAttributesAtPosition(glm::vec3 _blockPos, const BlockAttributes& _attributes) const {
    auto blockChunk = GetChunkAtBlockPos(_blockPos);
    if (blockChunk == nullptr) return;

    blockChunk->SetChunkBlockAttributesAtPosition(_blockPos, _attributes);
}

BlockAttributes Chunk::GetBlockAttributesAtPosition(glm::vec3 _blockPos) const {
    auto blockChunk = GetChunkAtBlockPos(_blockPos);
    if (blockChunk == nullptr) return {};

    return blockChunk->GetChunkBlockAttributesAtPosition(_blockPos);
}

/*
 * Returns a pointer to a unique block instance in the chunk. If the provided data does not correlate to a unique block
 * then a new block is created and added, and a pointer to that returned.
 */

Block& Chunk::GetBlockFromData(const BlockType& _blockType) {
    if (uniqueBlockMap[_blockType] == nullptr) {
        uniqueBlockMap[_blockType] = CreateBlock(_blockType);
    }

    return *uniqueBlockMap[_blockType];
}



/*
 * Get the value of the topmost y position of the chunks blocks at the given block position
 */

float Chunk::GetTopLevelAtPosition(glm::vec3 _blockPos, float _radius) {
    ChunkDataTypes::ChunkBlock playerBlock = GetBlockAtPosition(_blockPos + dirTop);
    Block playerBlockPtr = GetBlockFromData(playerBlock.type);
    if (playerBlockPtr.GetSharedAttribute(BLOCKATTRIBUTE::ENTITYCOLLISIONSOLID) != 0) {
        return GetTopLevelAtPosition(_blockPos + dirTop, _radius);
    }

    float topLevel = -20;

    // if position y is 0.8 or above, round to ciel
    float y = (_blockPos.y - floorf(_blockPos.y) >= 0.8f) ? roundf(_blockPos.y) : floorf(_blockPos.y);

    // to 0.01 precision, convert to int to *maybe* stop some imprecision issues with looping through with floats
    for (int x = int(100.0 * (_blockPos.x - _radius)); x <= int(100.0 * (_blockPos.x + _radius)); x += int(100.0 * _radius)) {
        for (int z = int(100.0 * (_blockPos.z - _radius)); z <= int(100.0 * (_blockPos.z + _radius)); z += int(100.0 * _radius)) {
            // Convert back to float position of block relative to chunk
            glm::vec3 position{x/100.0, y, z/100.0};
            ChunkDataTypes::ChunkBlock block = GetBlockAtPosition(position);
            Block blockPtr = GetBlockFromData(block.type);

            // If no block found / air, or if it is a liquid (ie: water) / non-solid then do not apply topLevel
            if (block.type.blockID == AIR) continue;
            if (blockPtr.GetSharedAttribute(BLOCKATTRIBUTE::ENTITYCOLLISIONSOLID) == 0) continue;

            // blockHeight + y in chunk + chunkHeight
            float blockTL = 1.0f + y + chunkIndex.y * (float)chunkSize;
            if (blockTL > topLevel) topLevel = blockTL;

        }
    }

    return topLevel;
}



/*
 * Get the distance from the provided position to the next solid surface in a given direction.
 */

float Chunk::GetDistanceToBlockFace(glm::vec3 _blockPos, glm::vec3 _direction, float _radius) {
    if (_direction == glm::vec3{0,0,0}) return 0;

    // get block in direction player is checking, if no block is found, assume no obstructions for the next
    // 2 blocks (to prevent stop-starting player movement if they move faster than 1 block/second)
    // also applies for air or liquid blocks

    ChunkDataTypes::ChunkBlock block = GetBlockAtPosition(_blockPos + _direction);
    Block blockPtr = GetBlockFromData(block.type);

    if (block.type.blockID == AIR ||
            blockPtr.GetSharedAttribute(BLOCKATTRIBUTE::ENTITYCOLLISIONSOLID) == 0) {
        if (_direction.x != 0) return floorf(_blockPos.x) + _direction.x * 2.0f;
        if (_direction.y != 0) return floorf(_blockPos.y) + _direction.y * 2.0f;
        if (_direction.z != 0) return floorf(_blockPos.z) + _direction.z * 2.0f;
    }

    BLOCKFACE face {};
    std::vector<UniqueVertex> faceVerticies {};
    float minZ {0}, maxZ {0};
    float minX {0}, maxX {0};

    if (_direction == dirFront) face = BACK;
    if (_direction == dirBack) face = FRONT;
    if (_direction == dirLeft) face = RIGHT;
    if (_direction == dirRight) face = LEFT;

    // Get min and max face verticies for x and z position
    faceVerticies = blockPtr.GetFaceVerticies({face}, block.attributes);
    for (auto& vertex : faceVerticies) {
        glm::vec3 vertexPosition = vertex.chunkPosition + vertex.modelVertex;
        if (vertexPosition.z + glm::ceil(_blockPos.z) + _direction.z < minZ)
            minZ = vertexPosition.z + glm::ceil(_blockPos.z) + _direction.z;
        if (vertexPosition.z + glm::floor(_blockPos.z) + _direction.z > maxZ)
            maxZ = vertexPosition.z + glm::floor(_blockPos.z) + _direction.z;

        if (vertexPosition.x + glm::ceil(_blockPos.x) + _direction.x < minX)
            minX = vertexPosition.x + glm::ceil(_blockPos.x) + _direction.x;
        if (vertexPosition.x + glm::ceil(_blockPos.x) + _direction.x > maxX)
            maxX = vertexPosition.x + glm::floor(_blockPos.x) + _direction.x;
    }

    // Player is only blocked by the block if their position +- radius is within min and max bounds
    if (_direction.x != 0) {
        if ((_blockPos.z + _radius >= minZ && _blockPos.z + _radius <= maxZ) ||
            (_blockPos.z - _radius >= minZ && _blockPos.z - _radius <= maxZ)) {
            if (_direction == dirFront) return maxX;
            else return minX + floorf(_blockPos.x) + _direction.x;
        }
    }

    if (_direction.z != 0) {
        if ((_blockPos.x + _radius >= minX && _blockPos.x + _radius <= maxX) ||
            (_blockPos.x - _radius >= minX && _blockPos.x - _radius <= maxX)) {
            if (_direction == dirLeft) return maxZ;
            else return minZ + floorf(_blockPos.z) + _direction.z;
        }
    }

    // Player not within min and max bounds, so not blocked
    if (_direction.x != 0) return floorf(_blockPos.x) + _direction.x * 2.0f;
    if (_direction.y != 0) return floorf(_blockPos.y) + _direction.y * 2.0f;
    if (_direction.z != 0) return floorf(_blockPos.z) + _direction.z * 2.0f;

    return 20;
}


/*
 * Position assumed to be relative to the chunk calling the initial command. The calling chunk determines if the
 * position provided is within 0 <= blockPos < ChunkSize, in each .xyz position. If not, it obtains the adjacent chunk
 * in the given direction where the position is outside of the limits.
 *
 * Returns a correct chunk for the _blockPos and updates _blockPos to be relative to the returned chunk
 */

std::shared_ptr<Chunk> Chunk::GetChunkAtBlockPos(glm::vec3& _blockPos) const {
    std::shared_ptr<Chunk> chunk = world->GetChunkAtIndex(chunkIndex);

    if (_blockPos.x < 0 || _blockPos.x >= chunkSize || _blockPos.z < 0 || _blockPos.z >= chunkSize) {
        _blockPos += chunkIndex * (float)chunkSize;
        chunk = world->GetChunkAtBlockPosition(_blockPos);
        if (chunk != nullptr) _blockPos -= chunk->GetIndex() * (float)chunkSize;
    }

    if (_blockPos.y < 0 || _blockPos.y >= chunkHeight)
        chunk = nullptr;

    return chunk;
}