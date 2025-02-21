//
// Created by cew05 on 18/04/2024.
//

#ifndef UNTITLED7_WINDOW_H
#define UNTITLED7_WINDOW_H

#include <SDL.h>
#include <vector>
#include <glew.h>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ErrorLogging.h"
#include "../src_shader/Shaders.h"

class Window {
    private:
        // Window info vars
        SDL_Rect winRect {};
        float aspectRatio {};

        // Window object and contex
        SDL_Window* window {};
        SDL_GLContext glContext {};

        // Shader programs
        unsigned int baseMeshShader = 0;
        unsigned int shadowShader = 0;
        unsigned int activeShader = 0;

    public:
        // Setup
        Window();
        ~Window();
        bool CreateGLContext();
        unsigned int CreateShaders();

        // Setters
        void SetWindowSize(int _w, int _h);

        // Shaders
        enum Shader { BASEMESH};
        void SetShader(const Window::Shader &_shader);

        // Getters
        SDL_Window* WindowPtr();
        void GetWindowSize(int& _w, int& _h) const;
        [[nodiscard]] unsigned int GetShader() const { return activeShader; };
        [[nodiscard]] float GetAspectRatio() const { return aspectRatio; };
};

inline Window window;

#endif //UNTITLED7_WINDOW_H
