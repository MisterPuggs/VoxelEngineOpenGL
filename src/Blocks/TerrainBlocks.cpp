
//
// Created by cew05 on 23/07/2024.
//

#include "TerrainBlocks.h"
#include "../Textures/TextureManager.h"

/*
 * STONE
 */

Stone::Stone(GLbyte _variant) {
    blockData = {BLOCKID::STONE, _variant};

    // Set the texture and texture origin
    sheet = TEXTURESHEET::TERRAIN;
    origin = {1, 1};
}

ToughStone::ToughStone(GLbyte _variant) {
    blockData = {BLOCKID::TOUGHSTONE, _variant};

    // Set the texture and texture origin
    sheet = TEXTURESHEET::TERRAIN;
    origin = {1, 5};
}

Dirt::Dirt(GLbyte _variant) {
    blockData = {BLOCKID::DIRT, _variant};

    // Set the texture and texture origin
    sheet = TEXTURESHEET::TERRAIN;
    origin = {4,2};
}

Grass::Grass(GLbyte _variant) {
    blockData = {BLOCKID::GRASS, _variant};

    // Set the texture and texture origin
    sheet = TEXTURESHEET::TERRAIN;
    origin = {7,1};
}

Sand::Sand(GLbyte _variant) {
    blockData = {BLOCKID::SAND, _variant};

    // Set texture and texture origin
    sheet = TEXTURESHEET::TERRAIN;
    origin = {13, 1};
}

Water::Water(GLbyte _variant) {
    blockData = {BLOCKID::WATER, _variant};

    // Set the texture and texture origin
    sheet = TEXTURESHEET::TERRAIN;
    origin = {10,2};

    // Set attributes
    transparent = 1;
    obscuresSelf = 1;
    liquid = 1;
    breakable = 0;
    canInteractThroughBlock = 1;
    entityCollisionSolid = 0;
    canOcclude = 0;
}

Air::Air(GLbyte _variant) {
    blockData = {BLOCKID::AIR, _variant};
    blockModel = EMPTY;

    sheet = TEXTURESHEET::WORLD;

    // Variants used for the skybox decorations
    if (_variant == 1) { // skybox
        origin = {10,2};
    }

    if (_variant == 2) { // sun
        origin = {4,2};
    }

    if (_variant == 3) { // moon
        origin = {7,1};
    }

    transparent = 1;
    breakable = 0;
    canInteractThroughBlock = 1;
    generationPriority = 0;
    entityCollisionSolid = 0;
    canBeOccluded = 0;
    canOcclude = 0;
}

UnbreakableBlock::UnbreakableBlock(GLbyte _variant)
{
    blockData = {BLOCKID::UNBREAKABLEBLOCK, _variant};

    sheet = TEXTURESHEET::TERRAIN;
    origin = {1,5};

    breakable = 0;

}