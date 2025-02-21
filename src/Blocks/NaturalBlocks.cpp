//
// Created by cew05 on 19/08/2024.
//

#include "NaturalBlocks.h"

Leaves::Leaves(GLbyte _variant) {
    blockData = {BLOCKID::LEAVES, _variant};
    sheet = TEXTURESHEET::NATURAL;

    if (_variant == 0) {
        origin = {4,2};
        transparent = 1;
    }
    if (_variant == 1) {
        origin = {1, 1};
    }

    generationPriority = 1;
}

Wood::Wood(GLbyte _variant) {
    blockData = {BLOCKID::WOOD, _variant};
    sheet = TEXTURESHEET::NATURAL;

    if (_variant == 0) {
        origin = {7,1};
    }
}

GrassPlant::GrassPlant(GLbyte _variant) {
    blockModel = PLANT;

    blockData = {BLOCKID::GRASSPLANT, _variant};
    sheet = TEXTURESHEET::NATURAL;

    origin = {2, 0};

    transparent = 1;
    generationPriority = 1;
    entityCollisionSolid = 0;
    canBeOccluded = 0;
    canOcclude = 0;
    canHaveSubblockPosition = 1;
}