//
// Created by cew05 on 30/07/2024.
//

#ifndef UNTITLED7_WORLDGENCONSTS_H
#define UNTITLED7_WORLDGENCONSTS_H

#include <SDL.h>
#include <random>

/*
 * WORLD VALUES
 */

// UNIVERSAL CONSTANTS FOR GENERATION
static const int WATERLEVEL = 120;
static const int DENSITYLAYER = 60;
static const int MINBLOCKHEIGHT = 14;
static const int SEAFLOORMINIMUM = 80;
static const int MAXBLOCKHEIGHT = 256;

static const int MINTEMP = -20;
static const int BASETEMP = 15;
static const int MAXTEMP = 40;


// MAX SIZE OF THE WORLD AREA TO BE LOADED
static const int loadRadius = 8; // minimum 2
static const int meshRadius = loadRadius - 1;
static const int renderRadius = meshRadius; // at maximum = meshRadius
static const int worldSize = (1 + loadRadius*2) + 2; // + 2 for border chunks to permit structure generation at world chunk borders
static const int worldArea = worldSize * worldSize;

// WORLD SEEDED GENERATION
static long long int worldSeed = 1738350823;
//static long long int worldSeed = time(nullptr);
static std::mt19937 worldGenerationRandom(worldSeed);
static std::mt19937 worldActionsRandom(worldSeed);

/*
 * CHUNK VALUES
 */

// SIZE OF THE CHUNKS
static const int chunkSize = 16;
static const int chunkHeight = 320;
static const int chunkArea = chunkSize * chunkSize;
static const int chunkProfile = chunkSize * chunkHeight;
static const int chunkVolume = chunkArea * chunkHeight;

/*
 * MOVEMENT VALUES
 */

const inline float JUMPHEIGHT = 1.2f;
const inline float JUMPDURATION = 0.15f;
const inline float GRAVITY = -(JUMPHEIGHT / (2 * std::pow(JUMPDURATION, 2.0f)));
const inline float JUMPSPEED = sqrtf(2 * JUMPHEIGHT * -GRAVITY);




#endif //UNTITLED7_WORLDGENCONSTS_H
