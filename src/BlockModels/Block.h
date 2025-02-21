//
// Created by cew05 on 07/07/2024.
//

#ifndef UNTITLED7_BLOCK_H
#define UNTITLED7_BLOCK_H

#include <memory>
#include <vector>
#include <string>

#include <glew.h>
#include <glm/matrix.hpp>


#include "../Textures/TextureManager.h"
#include "ModelStructs.h"

/*
 * For obtaining a particular block model
 */

enum BLOCKMODEL {
    EMPTY, FULL, PLANT, nModels
};

/*
 * For referencing a particular face of a block
 */

enum BLOCKFACE : GLbyte {
    FRONT, BACK, LEFT, RIGHT, TOP, BOTTOM, ALL
};

/*
 * A struct that binds the block models into memory, and permits the fetching / retrieving of a particular model's
 * VAO for display, or the base verticies / indexes of a model
 */

struct BlockVAOs {
    // Buffer objects
    std::array<GLuint, nModels> vertexArrayObject {};
    std::array<GLuint, nModels> vertexBufferObject {};
    std::array<GLuint, nModels> indexBufferObject {};

    BlockVAOs();
    ~BlockVAOs();

    // VAO Creation for FullBlock
    static std::vector<ModelVertex> FullblockVA();
    static std::vector<GLuint> FullblockIA();

    // VAO Creation for PlantBlock
    static std::vector<ModelVertex> PlantblockVA();
    static std::vector<GLuint> PlantblockIA();

    // ...

    // Bind the block vertex data
    void BindBlockModels() const;

    // Getters for unchanged IndexArrays / VertexArrays
    [[nodiscard]] static std::vector<GLuint> GetBaseIndexArray(BLOCKMODEL _model) ;
    [[nodiscard]] static std::vector<ModelVertex> GetBaseVertexArray(BLOCKMODEL _model) ;
};

inline std::unique_ptr<BlockVAOs> blockVAOmanager {};



/*
 * Base-level identification of block types. Shared between variants
 */

enum BLOCKID : GLubyte {
    // Hard Terrain
    STONE, TOUGHSTONE, UNBREAKABLEBLOCK,

    // Soft Terrain
    GRASS, DIRT, WATER, SAND,

    // Foliage
    LEAVES, WOOD, GRASSPLANT,

    // Crafted

    // Other
    AIR,

    // Testing
    TEST,

};



/*
 * Used to identify the type of block through the combination of BLOCKID and a variant value. Blocks should only be
 * considered as the same type if they share both blockID and variantID
 */

struct BlockType {
    BLOCKID blockID {BLOCKID::AIR};
    GLbyte variantID {0};

    friend bool operator==(const BlockType& A, const BlockType& B) {
        return A.blockID == B.blockID && A.variantID == B.variantID;
    }

    friend bool operator!=(const BlockType& A, const BlockType& B) {
        return !(A == B);
    }

    friend bool operator<(const BlockType& A, const BlockType& B) {
        return A.variantID < B.variantID && A.blockID <= B.blockID;
    }
};

// Required for map to hash the BlockType
template <>
struct std::hash<BlockType> {
    size_t operator()(const BlockType& A) const {
        return (std::hash<BLOCKID>()(A.blockID)) ^ (std::hash<GLbyte>()(A.variantID));
    }
};

/*
 * Enums for each attribute that a block uses. Attributes when applied to a block should refer to an int value which
 * can be obtained and utilised. For example, transparency = 0 indicates a solid, non-transparent block. Wheras a
 * transparency = 1 indicates a block with transparent elements. Values are not necessarily limited to 0 or 1 but must
 * be int
 */

enum class BLOCKATTRIBUTE {
    // Shared (same across all block objects)
    TRANSPARENT, OBSCURES_SELF, LIQUID, BREAKABLE, CANACCESSTHROUGHBLOCK, GENERATIONPRIORITY, ENTITYCOLLISIONSOLID, BLOCKMODEL,
    CANOCCLUDE, CANBEOCCLUDED, CANHAVESUBBLOCKPOSITION,

    // Unique (to each block object)
    BLOCKOFFSET, BLOCKOFFSET_X, BLOCKOFFSET_Y, BLOCKOFFSET_Z,
    FACINGDIRECTION, ROTATION, BLOCKLIGHT, SKYLIGHT

    // ... other block attributes
};



/*
 * Data struct to contain block attributes which are not equivalent within all instances of a block type. Whilst all
 * grass blocks are solid (hence the attribute is stored in the block object), they may face in a different direction,
 * and thus this is stored in BlockAttributes.
 */

struct BlockAttributes {
    GLbyte halfRightRotations = 0;
    GLbyte topFaceDirection = DIRECTION::UP;
    GLbyte blockLight = 0;
    GLbyte skyLight = 0;
    glm::i8vec3 subBlockOffset {0,0,0};

    [[nodiscard]] GLbyte GetIndividualAttribute(BLOCKATTRIBUTE _attribute) const;
};



/*
 * A block!
 */

class Block {
    protected:
        // Block Display
        TEXTURESHEET sheet {TEXTURESHEET::WORLD};
        glm::vec2 origin {1,1};

        // Block Data Attributes
        GLbyte transparent = 0;
        GLbyte obscuresSelf = 0;
        GLbyte liquid = 0;
        GLbyte breakable = 1;
        GLbyte canInteractThroughBlock = 0;
        GLbyte generationPriority = 2;
        GLbyte entityCollisionSolid = 1;
        GLbyte canOcclude = 1;
        GLbyte canBeOccluded = 1;
        GLbyte canHaveSubblockPosition = 0;
        glm::vec3 maxSubpixels {8, 8, 8};

        // Visual Rotations
        bool topFaceLocked = true; // can only face up
        bool rotationLocked = false; // cannot be rotated

        // BlockType info
        BlockType blockData {AIR, 0};
        BLOCKMODEL blockModel {FULL};

    public:
        Block();
        ~Block();

        // Model Display
        void Display(const Transformation& _t) const;
        void DisplayWireframe(const Transformation& _transformation) const;
        [[nodiscard]] glm::vec2 GetTextureOrigin() const { return origin; }
        [[nodiscard]] TEXTURESHEET GetTextureSheet() const { return sheet; }

        // Shared Block Attributes
        [[nodiscard]] BlockType GetBlockType() const { return blockData; }
        [[nodiscard]] GLbyte GetSharedAttribute(BLOCKATTRIBUTE _attribute) const;

        // Unique Block Attributes
        [[nodiscard]] DIRECTION GetRandomTopFaceDirection() const;
        [[nodiscard]] GLbyte GetRandomRotation() const;
        [[nodiscard]] glm::i8vec3 GetRandomSubOffset(const glm::vec3& _blockPosition) const;

        // Block Face Culling
        [[nodiscard]] std::vector<UniqueVertex> GetFaceVerticies(const std::vector<BLOCKFACE>& _faces, const BlockAttributes& _blockAttributes) const;
        [[nodiscard]] static bool BlockFaceVisible(const Block& _checkingBlock, const Block& _faceBlock, BLOCKFACE _face = TOP);
};






/*
 * Testing block, used for testing. Should not naturally generate. Could also be used if a request for a block with
 * faulty data is made. TestBlock can be the default return.
 */

class TestBlock : public Block {
    private:

    public:
        explicit TestBlock(GLbyte _variant) {
            blockData = {BLOCKID::TEST, _variant};
            sheet = TEXTURESHEET::TEST16;

            // Set texture origin
            switch (_variant) {
                case 1: origin = {4,2}; break;
                case 2: origin = {5,4}; break;
                case 3: origin = {7,1}; break;
                case 4: origin = {10,2}; break;
                case 5: origin = {13,1}; break;
                default: origin = {1,1};
            }
        };
};



#endif //UNTITLED7_BLOCK_H
