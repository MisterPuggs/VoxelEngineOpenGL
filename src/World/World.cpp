//
// Created by cew05 on 10/07/2024.
//

#include "World.h"


#include "../Blocks/CreateBlock.h"
#include "Biomes/CreateBiome.h"
#include "Chunks/Chunk.h"

#include "Noise/2DSimplexNoise.h"
#include "Noise/3DSimplexBlockDensity.h"

World::World() {
    // Create skybox, sun and moon
    skybox = CreateBlock({BLOCKID::AIR, 1});
    sun = CreateBlock({AIR, 2});
    moon = CreateBlock({AIR, 3});

    sunTransformation = Transformation();
    sunTransformation.SetScale({15.0f, 15.0f, 15.0f});
    sunTransformation.UpdateModelMatrix();

    moonTransformation = Transformation();
    moonTransformation.SetScale({12.0f, 12.0f, 12.0f});
    moonTransformation.UpdateModelMatrix();

    // Update shader values
    GLint uLocation;
    uLocation = glGetUniformLocation(window.GetShader(), "worldAmbients.lightingStrength");
    if (uLocation < 0) printf("location not found [worldAmbients.lightingStrength]\n");
    else glUniform1f(uLocation, 1.0f);

    uLocation = glGetUniformLocation(window.GetShader(), "worldAmbients.minFogDistance");
    if (uLocation < 0) printf("location not found [worldAmbients.minFogDistance]\n");
    else glUniform1f(uLocation, (renderRadius - 1) * chunkSize);

    uLocation = glGetUniformLocation(window.GetShader(), "worldAmbients.maxFogDistance");
    if (uLocation < 0) printf("location not found [worldAmbients.maxFogDistance]\n");
    else glUniform1f(uLocation, renderRadius * chunkSize);

    glEnable(GL_DEPTH_TEST);

    // Set Mesher Retry Check
    chunkMesherThread.SetRetryCheckFunction([&](const glm::ivec2& _chunkIndex, const glm::vec3& _blockPos){
        int diffX = std::abs(_chunkIndex.x - (int)loadingIndex.x);
        int diffZ = std::abs(_chunkIndex.y - (int)loadingIndex.y);

        // chunk within load region returns true, permits retry
        return (diffX + diffZ <= meshRadius);
    });

    // Start threads
    chunkBuilderThread.StartThread();
    chunkMesherThread.StartThread();
    chunkLoaderThread.StartThread();
    chunkLighterThread.StartThread();
}

World::~World() {
    chunkBuilderThread.EndThread();
    chunkMesherThread.EndThread();
    chunkLoaderThread.EndThread();
}

void World::Display() const {
    glEnable(GL_BLEND);

    // First draw in the skybox and decorations
    skybox->Display(skyboxTransformation);
    sun->Display(sunTransformation);
    moon->Display(moonTransformation);

    // Draw solid objects
    glEnable(GL_CULL_FACE);
    for (int x = -renderRadius; x < renderRadius; x++) {
        for (int z = -renderRadius; z < renderRadius; z++) {
            auto chunk = GetChunkAtIndex(loadingIndex + glm::ivec2{x, z});

            if (chunk != nullptr) chunk->DisplaySolid();
        }
    }
    glDisable(GL_CULL_FACE);

    // Draw transparent objects
    for (int x = -renderRadius; x < renderRadius; x++) {
        for (int z = -renderRadius; z < renderRadius; z++) {
            auto chunk = GetChunkAtIndex(loadingIndex + glm::ivec2{x, z});

            if (chunk != nullptr) chunk->DisplayTransparent();
        }
    }

}

void World::CheckCulling(const Camera &_camera) {
    displayingChunks = 0;

    for (int x = -renderRadius; x < renderRadius; x++) {
        for (int z = -renderRadius; z < renderRadius; z++) {
            auto chunk = GetChunkAtIndex(loadingIndex + glm::ivec2{x, z});

            if (chunk != nullptr) {
                chunk->CheckCulling(_camera);
                if (chunk->ChunkVisible()) displayingChunks += 1;
            }
        }
    }

    printf("DISPLAYING %d CHUNKS\n", displayingChunks);
}



/*
 * SKYBOX
 */

void World::SetSkyboxProperties(const Player& player) {
    // Determine max distance for skybox
    std::pair<float, float> minMax = player.GetUsingCamera()->GetMinMaxDistance();
    double maxSqrd = std::pow(minMax.second, 2.0);

    // Set skybox scale
    skyboxTransformation.SetScale({float(sqrt(maxSqrd)), float(sqrt(maxSqrd)), float(sqrt(maxSqrd))});

    // Set skybox position centred on the player
    SetSkyboxPosition(player.GetPosition());
}



/*
 * Updates the positions of elements within the skybox when the player moves
 */

void World::SetSkyboxPosition(glm::vec3 _position) {
    glm::vec3 originFromCentre;

    // Skybox
    originFromCentre = glm::vec3{_position - (skyboxTransformation.GetLocalScale() / 2.0f)};
    originFromCentre.y += skyboxTransformation.GetLocalScale().y;
    skyboxTransformation.SetPosition(originFromCentre);
    skyboxTransformation.UpdateModelMatrix();

    // Sun
    originFromCentre = _position - sunTransformation.GetLocalScale() / 2.0f;
    originFromCentre.y += sunTransformation.GetLocalScale().y;

    glm::mat4 sunAngle = glm::rotate(glm::mat4(1.0f), glm::radians(-((float)worldTime-(7*60)) * 0.125f), dirBack);
    glm::vec3 sunPos{originFromCentre.x, originFromCentre.y, originFromCentre.z};
    glm::vec3 sunOffset = sunAngle * glm::vec4(0.0f, 0.0f, 180.0f, 1.0f);

    sunTransformation.SetPosition(sunPos + sunOffset);
    sunTransformation.UpdateModelMatrix();

    // Moon
    originFromCentre = _position - moonTransformation.GetLocalScale() / 2.0f;
    originFromCentre.y += moonTransformation.GetLocalScale().y;

    glm::vec3 moonPos{originFromCentre.x, originFromCentre.y, originFromCentre.z};
    moonPos += -sunAngle * glm::vec4(0.0f, 0.0f, 180.0f, 1.0f);

    moonTransformation.SetPosition(moonPos);
    moonTransformation.UpdateModelMatrix();
}



/*
 * Continuously updates the world time and calculates the ambient light level from the time of day.
 */

void World::UpdateWorldTime(Uint64 _deltaTicks) {
    worldTicks += _deltaTicks;
    if (worldTicks >= 1000) {
        auto timeTicks = std::div((int)worldTicks, 1000);
        worldTime += timeTicks.quot;
        worldTicks = timeTicks.rem;
    }

    if (worldTime >= 24 * 60) {
        auto daysTime = std::div((int)worldTime, 24 * 60);
        worldDays += daysTime.quot;
        worldTime = daysTime.rem;
    }
}



/*
 * WORLD GENERATION
 * Creates chunk objects in the region around the player, adding their generation and
 * meshing functions into the threads
 * DONE FOR IMMEDIATE AREA AROUND PLAYER AS HIGH PRIORITY
 */

void World::GenerateRequiredWorldRegion() {
    using namespace std::placeholders;

    bool loadSquare = false;

    ThreadAction generateChunk{std::bind(&World::GenerateChunk, this, _1, _2), loadingIndex};
    chunkBuilderThread.AddActionRegion(generateChunk, loadRadius, loadSquare);
}



/*
 * WORLD GENERATION
 * Creates chunk objects in the region around the player, adding their generation and
 * meshing functions into the threads
 * DONE FOR ENTIRE SQUARE AREA AS LOW PRIORITY ACTIONS
 */

void World::GenerateLoadableWorldRegion() {
    using namespace std::placeholders;

    bool loadSquare = false;

    // Generate the chunks within the loading region (and not border), this will be done after the chunks are created
    ThreadAction generateChunk{std::bind(&World::GenerateChunk, this, _1, _2), loadingIndex};
    chunkBuilderThread.AddActionRegion(generateChunk, loadRadius, loadSquare);
}

/*
 * Thread-Called function to retrieve chunk data for a given chunk index position.
 */

THREAD_ACTION_RESULT World::CreateChunk(const glm::ivec2& _chunkIndex, const glm::vec3& _blockPos) {
    // Chunk object already exists, dont overwrite
    if (GetChunkAtIndex(_chunkIndex) != nullptr) {
        return ThreadAction::OK;
    }

    // Retrieve any existing data


    // Get ChunkData and create the chunk
    ChunkData chunkData = GenerateChunkData(_chunkIndex);
    chunkData.biome = GenerateBiome(GetBiomeIDFromData(chunkData));

    glm::vec3 index{_chunkIndex.x, 0, _chunkIndex.y};
    CreateChunkAtIndex(index, chunkData);

    return ThreadAction::OK;
}



/*
 * Obtains chunk data and generates a chunk (base terrain and decorative)
 */

THREAD_ACTION_RESULT World::GenerateChunk(const glm::ivec2& _chunkIndex, const glm::vec3& _blockPos) {
    using namespace std::placeholders;

    auto chunk = GetChunkAtIndex(_chunkIndex);
    if (chunk == nullptr) {
        CreateChunk(_chunkIndex, {0, 0, 0});
        chunk = GetChunkAtIndex(_chunkIndex);
        if (chunk == nullptr) return ThreadAction::FAIL;
    }

    // Ensure that chunk should be loaded
    int diffX = std::abs(_chunkIndex.x - (int)loadingIndex.x);
    int diffZ = std::abs(_chunkIndex.y - (int)loadingIndex.y);

    // chunk not within load region will not proceed to generate. returns fail.
    if (diffX + diffZ > loadRadius) return ThreadAction::FAIL;

    // Generate the chunk's blocks
    if (!chunk->Generated()) {
        chunk->GenerateChunk();
    }


    // Adds Lighting Task for the chunk
    ThreadAction lightingAction{std::bind(&World::FloodSkyLightingForChunk, this, _1, _2), _chunkIndex};
    chunkLighterThread.AddActionRegion(lightingAction, 0);

    return ThreadAction::OK;
}


THREAD_ACTION_RESULT World::GenerateChunkMesh(const glm::ivec2 &_chunkIndex, const glm::vec3& _blockPos) const {
    auto chunk = GetChunkAtIndex(_chunkIndex);

    if (chunk != nullptr && chunk->RegionGenerated()) {
        chunk->CreateChunkMeshes();
        return ThreadAction::OK;
    }
    else if (chunk != nullptr) {
        return ThreadAction::RETRY;
    }

    return ThreadAction::FAIL;
}


THREAD_ACTION_RESULT World::FloodSkyLightingForChunk(const glm::ivec2 &_chunkIndex, const glm::vec3 &_blockPos) {
    using namespace std::placeholders;

    if (_blockPos.y < 0 || _blockPos.y >= chunkSize) return ThreadAction::FAIL;

    auto chunk = GetChunkAtIndex(_chunkIndex);

    if (chunk != nullptr && chunk->Generated()) {
        chunk->FloodFillSkyLight();

        // Adds meshing task for the chunk
        ThreadAction meshAction{std::bind(&World::GenerateChunkMesh, this, _1, _2), _chunkIndex};
        chunkMesherThread.AddActionRegion(meshAction, 0);

        return ThreadAction::OK;
    }

    return ThreadAction::FAIL;
}

/*
 * Starting at player chunk origin
 * Retrieve lighting from above
 * Set lightValue as greater of (filtered above block skylight || filtered side light)
 * flood to side then below
 */

THREAD_ACTION_RESULT World::FloodSkyLightingFromPosition(const glm::ivec2 &_chunkIndex, const glm::vec3 &_blockPos) {
    using namespace std::placeholders;

    // Outside chunk bounds
    if (_blockPos.y < 0 || _blockPos.y >= chunkSize) return ThreadAction::OK;

    auto chunk = GetChunkAtIndex(_chunkIndex);
    if (chunk != nullptr && chunk->RegionGenerated()) {

        ChunkDataTypes::ChunkBlock thisBlock = chunk->GetBlockAtPosition(_blockPos);
        Block thisBlockObj = chunk->GetBlockFromData(thisBlock.type);

        GLbyte filter = thisBlockObj.GetSharedAttribute(BLOCKATTRIBUTE::TRANSPARENT);
        if (filter == 0) {
            // Solid Block. Set skylight to 0 and exit early
            thisBlock.attributes.skyLight = 0;
            return ThreadAction::OK;
        }

        GLbyte lightAbove = 15; // Use WorldTime to get light
        if (_blockPos.y + dirTop.y < chunkHeight) {
            ChunkDataTypes::ChunkBlock blockAbove = chunk->GetBlockAtPosition(_blockPos + dirTop);
            lightAbove = GLbyte(blockAbove.attributes.skyLight - filter);
        }

        thisBlock.attributes.skyLight = std::max(lightAbove, thisBlock.attributes.skyLight);

        glm::vec3 adjDirs[] {dirFront, dirBack, dirLeft, dirRight};
        for (const auto& dir : adjDirs) {
            ChunkDataTypes::ChunkBlock blockAdj = chunk->GetBlockAtPosition(_blockPos + dir);
            GLbyte lightAdj = GLbyte(blockAdj.attributes.skyLight - filter);
            thisBlock.attributes.skyLight = std::max(lightAdj, thisBlock.attributes.skyLight);

            ThreadAction floodAdj{std::bind(&World::FloodSkyLightingFromPosition, this, _1, _2), _chunkIndex};
            floodAdj.chunkBlock = _blockPos + dir;
            chunkLighterThread.AddActionRegion(floodAdj, 0);
        }

        ThreadAction floodBelow{std::bind(&World::FloodSkyLightingFromPosition, this, _1, _2), _chunkIndex};
        floodBelow.chunkBlock = _blockPos + dirBottom;
        chunkLighterThread.AddActionRegion(floodBelow, 0);

        return ThreadAction::OK;
    }

    return ThreadAction::FAIL;
}

THREAD_ACTION_RESULT World::FloodBlockLightingFrom(const glm::ivec2 &_chunkIndex, const glm::vec3 &_blockPos) {
    auto chunk = GetChunkAtIndex(_chunkIndex);

    // Outside chunk bounds
    if (_blockPos.y < 0 || _blockPos.y >= chunkSize) return ThreadAction::OK;

    if (chunk != nullptr && chunk->Generated()) {




        return ThreadAction::OK;
    }

    return ThreadAction::FAIL;
}



void World::ManageLoadedChunks(const std::shared_ptr<Chunk>& _currentChunk, const std::shared_ptr<Chunk>& _newChunk) {
    using namespace std::placeholders;

    glm::vec2 oldChunkPos = {_currentChunk->GetIndex().x, _currentChunk->GetIndex().z};

    // hijack the blockPos intended for precision to store a second chunkPos instead
    ThreadAction markUnloaded{std::bind(&World::CheckChunkLoaded, this, _1, _2),
                              oldChunkPos, _newChunk->GetIndex()};
    chunkLoaderThread.AddPriorityActionRegion(markUnloaded, loadRadius + 1, true);
}



/*
 * Enacts on all chunks in the current loaded region and checks if they continue to
 * be loaded in the new loaded region
 */

THREAD_ACTION_RESULT World::CheckChunkLoaded(const glm::ivec2 &_currentChunkPos, const glm::vec3 &_newChunkPos) {
    glm::vec3 chunkIndex = {_currentChunkPos.x, 0, _currentChunkPos.y};

    int diffX = std::abs(_currentChunkPos.x - (int)_newChunkPos.x);
    int diffZ = std::abs(_currentChunkPos.y - (int)_newChunkPos.z);

    // chunk within load region, ignore
    if (diffX + diffZ <= loadRadius) return ThreadAction::OK;

    // chunk is outside of the load radius so unload
    return DestroyChunkAtIndex(chunkIndex);
}


float World::GenerateBlockCavernosity(glm::vec2 _blockPos) {
    float cavernosity;

    cavernosity = ComplexNoiseLimited(_blockPos, 128, 8, 0.5, 2, 0, 1);

    return cavernosity;
}

float World::GenerateBlockHollowness(glm::vec2 _blockPos) {
    float weirdness;

    weirdness = ComplexNoiseLimited(_blockPos, 256, 4, 0.5, 2, 0, 1);

    return weirdness;
}

float World::GenerateBlockHeight(glm::vec2 _blockPos) {
    float height;

    /*
     * PRIMARY TERRAIN LEVELS
     * Continentiality 0 - 2:
     *      controlls ocean-landmass generation
     *      < 1 = Oceans
     *      1 - 2 = Landmasses, with greater values resulting in higher landmasses
     *      scale 256 = islands,
     *      scale 1024 = big islands
     *
     * Erosion 0 - 1:
     *      low values results in flat landscape
     *      high values results in bumpier landscape
     *
     *
     */

    // Constructs the Base of the Terrain via continental landmass generation from seabed to landbed
    float continentiality = ComplexNoise(_blockPos, 1024, 2, 0.5, 4);
    continentiality += 1;
    continentiality = std::max(0.0f, continentiality);

    // Constructs the base level of the terrain ontop of the SeaFloor
    height = ((WATERLEVEL - SEAFLOORMINIMUM) * continentiality) + SEAFLOORMINIMUM;

    // Erosion (flatness) of terrain
    float erosion = ComplexNoiseLimited(_blockPos, 1024, 1, 0.5, 4,
                                        0, 1);
    erosion = std::pow(erosion, 5.0f);

    // Primary Noise above the continentiality height.
    float surfaceHeightVariation = ComplexNoise(_blockPos, 128.0f, 4, 0.5, 2);
    surfaceHeightVariation *= 5;

    height += erosion * surfaceHeightVariation;

    // Secondary base level noise applied
//    float secondHeight = glm::simplex(glm::vec2( _blockPos.x / 64.0, _blockPos.y / 64.0));
//    secondHeight *= 1;
//    height += secondHeight;

    /*
     *  MOUNTAIN GENERATION
     */

    // Produce noise values for mountain
    float peakHeight = ComplexNoiseLimited(_blockPos, 128, 4, 0.5, 2, 0, 1);
    peakHeight *= (MAXBLOCKHEIGHT - WATERLEVEL);

    // Determine if mountain should generate
    float mountainRegion = ComplexNoiseLimited(_blockPos, 500, 4, 0.5, 2, 0, 1);
    mountainRegion = std::pow(mountainRegion, 5.0f); // increase to reduce number of mountains
    height += mountainRegion * peakHeight;

    return std::round(height);
}

int World::GenerateCaveChambers(glm::vec3 _blockPos, float _hmTopLevel, float _cavernosity, float _hollowness) {
    float y = _blockPos.y;
    float minCavernosity = 0.5f;

    int solid = 1, air = -1;

    // Predetermine if Block Density need not be calculated
    if (y > _hmTopLevel) return air;
    if (y > MAXBLOCKHEIGHT) return air;
    if (y <= MINBLOCKHEIGHT) return solid;

    // Caves can only generate in specific regions
    if (_cavernosity < minCavernosity) return solid;

    // Ceiling where any greater Y is Solid
    float solidCeiling = (_hmTopLevel * _hollowness) / _cavernosity;
    solidCeiling = std::min(solidCeiling, _hmTopLevel + 1);
    if (y >= solidCeiling) return solid;

    /*
     * Generate Cave Chambers
     */

    float density = BlockDensity(_blockPos, 64, 8, 0.8, 2);
    density *= _cavernosity;

    return (density < -0.3) ? air : solid;
}

float World::GenerateBlockHeat(glm::vec3 _blockPos) {
    float heat = glm::simplex(glm::vec2(_blockPos.x / 64.0, _blockPos.z / 64.0));
    heat = (heat + 1) / 2;
    heat *= 20;
    heat += BASETEMP;

    // Relate heat to height (higher = colder)
    heat -= (_blockPos.y / MAXBLOCKHEIGHT) * 10;

    return heat;
}

float World::GenerateBlockVegetation(glm::vec3 _blockPos, float _heat) {
    float grassDensity = glm::simplex(glm::vec2( _blockPos.x / 8.0, _blockPos.z / 8.0));
    grassDensity = (grassDensity + 1) / 2;

    float treeDensity = glm::simplex(glm::vec2( _blockPos.x / 1.0, _blockPos.z / 1.0));
    treeDensity = (treeDensity + 1) / 2;
    treeDensity = std::pow(treeDensity, 10.0f);

    return grassDensity + treeDensity;
}

BlockType World::GenerateBlockAtPosition(const glm::vec3 &_blockPos) const {
    auto chunk = GetChunkAtBlockPosition(_blockPos);

    if (chunk == nullptr) return {AIR, 0};

    glm::vec3 blockMapPos = _blockPos - (chunk->GetIndex() * (float)chunkSize);

    // Fetch map values
    float hmTopLevel = chunk->GetHeightAt((int)blockMapPos.x, (int)blockMapPos.z);
    float cavernosity = World::GenerateBlockCavernosity(blockMapPos);
    float hollowness = World::GenerateBlockHollowness(blockMapPos); // change to hollowness

    int blockDensity = World::GenerateCaveChambers(_blockPos, hmTopLevel, cavernosity, hollowness);
    if (blockDensity < 0) return {AIR, 0};

    else return chunk->GetBiome()->GetBlockType(hmTopLevel, _blockPos.y);

}

// Generate the height and temp maps for the given chunk starting pos
ChunkData World::GenerateChunkData(glm::vec2 _chunkPosition) {
    int chunkX = (int)_chunkPosition.x * chunkSize;
    int chunkZ = (int)_chunkPosition.y * chunkSize;
    ChunkData chunkData {};

    for (int x = 0; x < chunkSize; x++) {
        for (int z = 0; z < chunkSize; z++) {
            int bx = chunkX + x, bz = chunkZ + z;

            // Get the toplevel (highest y) of the given x z position
            float height = GenerateBlockHeight({bx, bz});
            chunkData.heightMap[x + z * chunkSize] = height;

            // Create block vegetation value (relate to heat, height)
            float heat = GenerateBlockHeat({bx, height, bz});
            float vegetation = GenerateBlockVegetation({bx, height, bz}, heat);

            chunkData.plantMap[x + z * chunkSize] = vegetation;
        }
    }

    return chunkData;
}



/*
 * Ensure that a biome of type BIOMEID has been generated for the world
 */

Biome* World::GenerateBiome(Biome::ID _biomeID) {
    // If the biome has been generated before then exit
    for (auto& uniqueBiome : uniqueBiomes) {
        if (uniqueBiome->GetBiomeID() == _biomeID) return uniqueBiome.get();
    }

    // Else required to create a new unique biome
    uniqueBiomes.emplace_back(CreateBiome(_biomeID));
    return uniqueBiomes.back().get();
}


void World::SetLoadingOrigin(const glm::vec3 &_origin) {
    loadingIndex = {_origin.x, _origin.z};  // index of player's centre chunk
}

void World::BindChunks() const {
    for (int x = -meshRadius; x < meshRadius; x++) {
        for (int z = -meshRadius; z < meshRadius; z++) {
            auto chunk = GetChunkAtIndex(loadingIndex + glm::ivec2{x, z});

            if (chunk != nullptr && chunk->UnboundMeshChanges()) {
                chunk->BindChunkMeshes();
            }
        }
    }
}






std::shared_ptr<Chunk> World::GetChunkAtBlockPosition(glm::vec3 _blockPos) const {
    glm::vec3 index = _blockPos / (float)chunkSize;
    return GetChunkAtIndex(index);
}

std::shared_ptr<Chunk> World::GetChunkAtIndex(glm::vec2 _chunkIndex) const {
    return GetChunkAtIndex({_chunkIndex.x, 0, _chunkIndex.y});
}

std::shared_ptr<Chunk> World::GetChunkAtIndex(glm::vec3 _chunkIndex) const {
    glm::vec3 index = _chunkIndex + glm::vec3{1000, 0, 1000}; // centre of the worlds chunks

    if (index.x < 0 || index.x >= 2000) return nullptr;
    if (index.z < 0 || index.z >= 2000) return nullptr;

    // permits multiple fetch requests for chunk
    std::shared_lock lock(worldChunks[(int)index.x][(int)index.z].chunkLock);
    std::shared_ptr<Chunk> chunkPtr = worldChunks[(int)index.x][(int)index.z].chunkPtr;

    return chunkPtr;
}


THREAD_ACTION_RESULT World::DestroyChunkAtIndex(glm::vec3 _chunkIndex) {
    glm::vec3 index = _chunkIndex + glm::vec3{1000, 0, 1000};

    if (index.x < 0 || index.x >= 2000) return ThreadAction::FAIL;
    if (index.y < 0 || index.y >= 2000) return ThreadAction::FAIL;

    // only one thread may destroy the chunk, and only when no fetch requests are active
    std::unique_lock lock(worldChunks[(int)index.x][(int)index.z].chunkLock, std::try_to_lock);
    if (!lock.owns_lock()) {
        // Chunk is busy right now so lock failed. Return it into the list
        return ThreadAction::RETRY;
    }

    // owns lock, destroy chunk
    worldChunks[(int)index.x][(int)index.z].chunkPtr.reset();
    return ThreadAction::OK;
}



THREAD_ACTION_RESULT World::CreateChunkAtIndex(glm::vec3 _chunkIndex, ChunkData _chunkData) {
    glm::vec2 index = {_chunkIndex.x + 1000, _chunkIndex.z + 1000};

    if (index.x < 0 || index.x >= 2000) return ThreadAction::FAIL;
    if (index.y < 0 || index.y >= 2000) return ThreadAction::FAIL;

    // only one thread may create the chunk, and only when no fetch requests are active
    std::unique_lock lock(worldChunks[(int)index.x][(int)index.y].chunkLock, std::try_to_lock);
    if (!lock.owns_lock()) {
        // Chunk is busy right now so lock failed. Return it into the list
        return ThreadAction::RETRY;
    }

    // owns lock, create chunk
    worldChunks[(int)index.x][(int)index.y] = std::make_unique<Chunk>(_chunkIndex, _chunkData);
    return ThreadAction::OK;
}




Biome* World::GetBiome(Biome::ID _biomeID) {
    // Fetch biome
    for (auto& uniqueBiome : uniqueBiomes) {
        if (uniqueBiome->GetBiomeID() == _biomeID) return uniqueBiome.get();
    }

    // Biome did not exist?
    uniqueBiomes.emplace_back(CreateBiome(_biomeID));
    return uniqueBiomes.back().get();
}


ChunkThreads* World::GetThread(THREAD _thread) {
    switch (_thread) {
        case THREAD::CHUNKBUILDING:
            return &chunkBuilderThread;

        case THREAD::CHUNKMESHING:
            return &chunkMesherThread;

        case THREAD::CHUNKLOADING:
            return &chunkLoaderThread;

        case THREAD::CHUNKLIGHTING:
            return &chunkLighterThread;

        default:
            return nullptr;
    }
}