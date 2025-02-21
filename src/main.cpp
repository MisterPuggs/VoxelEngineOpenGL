#define SDL_MAIN_HANDLED
#include <random>
#include <memory>

#include <SDL.h>
#include <glew.h>

#include "Window.h"
#include "Player/Player.h"
#include "World/World.h"
#include "World/Structures/LoadStructure.h"
#include "Textures/TextureManager.h"

#include "GlobalStates.h"


int main(int argc, char** argv){
    // Init SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        LogError("Failed to init", SDL_GetError(), true);
        return 0;
    }

    // Construct Window
    if (window.WindowPtr() == nullptr) return 0;        // Create SDL_Window
    if (!window.CreateGLContext()) return 0;            // Create openGL context in SDL_Window

    // Init glew : requires openGL context to have been made
    if (glewInit() != GLEW_OK) {
        LogError("Failed to initialise glew", gluErrorString(glGetError()), true);
        return 0;
    }

    // Create shaders
    if (window.CreateShaders() == 0) return 0;
    window.SetShader(Window::Shader::BASEMESH);

    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /*
     * OBJECT MODELS AND TEXTURE LOADING
     */

    // LOAD BLOCK VAOs
    blockVAOmanager = std::make_unique<BlockVAOs>();
    blockVAOmanager->BindBlockModels();

    // LOAD TEXTURES

    // Create the texture manager
    textureManager = std::make_unique<TextureManager>();

    /*
     *  WORLD CREATION
     */

    // Create StructureLoader

    // Create world and enable chunk builder threads
    world = std::make_unique<World>();
    world->SetLoadingOrigin({0,0,0});
    world->GenerateRequiredWorldRegion();

    printf("WORLD SEED %llu\n", worldSeed);

    /*
     * PLAYER CREATION
     */

    printf("SPAWN POS %f %f %f\n", 0.5f, world->GenerateBlockHeight({0,0}), 0.5f);

    glm::vec3 startPos = {0.5f, world->GenerateBlockHeight({0,0}) + 1, 0.5f};
    Player player {startPos, {0,0.01,1.0f}};

    // Set skybox dimensions with player camera
    world->SetSkyboxProperties(player);

    /*
     * LOOP SETUP
     */

    // Trap mouse to screen and hide it
    SDL_bool grabMouse = SDL_TRUE;
    SDL_SetWindowGrab(window.WindowPtr(), grabMouse);
    SDL_ShowCursor(SDL_DISABLE);

    bool running = true;
    bool escToggled = false;

    // Whilst the world is still loading the minimum required area, hold the user here
    auto build = world->GetThread(THREAD::CHUNKBUILDING);
    auto mesh = world->GetThread(THREAD::CHUNKMESHING);

    while (build->HasActions() || mesh->HasActions()) {
        // SDL EVENTS
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
                build->EndThread();
                mesh->EndThread();
            }
        }

        // KEYBOARD STATES
        const std::uint8_t* state = SDL_GetKeyboardState(nullptr);

        // LOCK / UNLOCK MOUSE TO SCREEN
        if (state[SDL_SCANCODE_ESCAPE]) {
            // Guard against escape being set to false after 2 frame press
            if (!escToggled) {
                // Set mouse pos to centre to prevent change in looking target on resume
                int maxx, maxy;
                window.GetWindowSize(maxx, maxy);
                SDL_WarpMouseInWindow(window.WindowPtr(), maxx/2, maxy/2);

                // lock mouse to window
                grabMouse = (grabMouse == SDL_TRUE) ? SDL_FALSE : SDL_TRUE;
                SDL_SetWindowGrab(window.WindowPtr(), grabMouse);
                SDL_ShowCursor((grabMouse == SDL_TRUE) ? SDL_DISABLE : SDL_ENABLE);
            }

            escToggled = true;
        }
        if (!state[SDL_SCANCODE_ESCAPE]) escToggled = false;
    }

    player.UpdatePlayerChunk();

    // Render Loop
    Uint64 deltaTicks, endTick = SDL_GetTicks64();
    glm::mat4 lastViewMatrix {};

    // FPS check
    int target = 120; // target fps
    int frames = 0;
    Uint64 ticks = 0;

    while (running) {
        /*
         * START OF FRAME
         */

        Uint64 startTick = SDL_GetTicks64();
        deltaTicks = startTick - endTick;
        if (deltaTicks < 1000 / target) {
            SDL_Delay(Uint32((1000 / target) - deltaTicks));
            deltaTicks = SDL_GetTicks64() - endTick;
        }
        endTick = startTick;

        frames += 1;
        ticks += deltaTicks;

        /*
         *  CLEAR SCREEN
         */

        float lightLevel;
        window.SetShader(Window::BASEMESH);
        GLint uLocation = glGetUniformLocation(window.GetShader(), "worldAmbients.lightingStrength");
        glGetUniformfv(window.GetShader(), uLocation, &lightLevel);

        glm::vec3 background = glm::vec3(179/255.0f, 199/255.0f, 242/255.0f) * lightLevel;
        glClearColor(background.r, background.g, background.b, 1.0f); //background colour
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /*
         * CHECK OBJECT CULLING
         */

        // If the player has changed the direction they are looking in since last check
        if (player.GetUsingCamera()->GetViewMatrix() != lastViewMatrix) {
            player.GetUsingCamera()->UpdateViewFrustrum();
//            world->CheckCulling(*player.GetUsingCamera());

            lastViewMatrix = player.GetUsingCamera()->GetViewMatrix();
        }


        /*
         * DRAW TO SCREEN
         */

        // WORLD DISPLAY
        world->Display();

        // ENTITY DISPLAY
        player.Display();

        // 2D OVERLAY



        /*
         * INPUT MANAGEMENT
         */

        // SDL EVENTS
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (grabMouse == SDL_TRUE) {
                player.HandlePlayerInputs(event);
            }
        }

        // KEYBOARD STATES
        const std::uint8_t* state = SDL_GetKeyboardState(nullptr);

        // LOCK / UNLOCK MOUSE TO SCREEN
        if (state[SDL_SCANCODE_ESCAPE]) {
            // Guard against escape being set to false after 2 frame press
            if (!escToggled) {
                // Set mouse pos to centre to prevent change in looking target on resume
                int maxx, maxy;
                window.GetWindowSize(maxx, maxy);
                SDL_WarpMouseInWindow(window.WindowPtr(), maxx/2, maxy/2);

                // lock mouse to window
                grabMouse = (grabMouse == SDL_TRUE) ? SDL_FALSE : SDL_TRUE;
                SDL_SetWindowGrab(window.WindowPtr(), grabMouse);
                SDL_ShowCursor((grabMouse == SDL_TRUE) ? SDL_DISABLE : SDL_ENABLE);
            }

            escToggled = true;
        }
        if (!state[SDL_SCANCODE_ESCAPE]) escToggled = false;

        // PLAYER MOVEMENT UPDATE
        if (grabMouse == SDL_TRUE) {
            player.HandleMovement(deltaTicks);
            player.MouseLook(grabMouse);
            player.GetUsingCamera()->UpdateLookatUniform();
        }

        // FPS CHECK
        if (state[SDL_SCANCODE_F]) {
            auto seconds = (double(ticks) / 1000.0);
            if (seconds == 0) seconds = 1;
            double fps = frames / seconds;
            printf("FPS: %f | TARGET FPS: %d | %%MATCH: %f%%\n", fps, target, (double(fps)/target) * 100.0);

            // reset fps counter every second as it would become too unrepresentative of any sudden dips / changes
            // to fps

            if (ticks >= 1000 * 1) {
                ticks = 0;
                frames = 0;
            }
        }

        // player coords
        if (state[SDL_SCANCODE_C]) {
            auto pos = player.GetPosition();
            printf("x %f y %f z %f\n", pos.x, pos.y, pos.z);
        }

        /*
         *  UDPATE OBJECTS
         */

        // WORLD
        world->UpdateWorldTime(deltaTicks);
        world->SetSkyboxPosition(player.GetPosition());
        world->BindChunks();

        // ...

        /*
         * UPDATE DISPLAY
         */

        SDL_GL_SwapWindow(window.WindowPtr());

        /*
         *  END OF FRAME
         */

    }

    /*
     * HANDLE END OF PROGRAM
     */

    SDL_Quit();

    return 0;
}