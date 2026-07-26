#include "Renderer.hpp"
#include "Scene.hpp"
#include "Camera.hpp"
#include "window/SDLWindow.hpp"
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <glad/glad.h>
#include <iostream>
#include <cstdio>

using namespace fe;

static SDLWindow* g_window = nullptr;

void main_loop() {
    if (g_window && g_window->ShouldClose()) {
        emscripten_cancel_main_loop();
        return;
    }

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (g_window) {
        SDL_GL_SwapWindow((SDL_Window*)g_window->GetWindow());
    }
}

int main() {
#ifdef __EMSCRIPTEN__
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif

    std::cout << "FenixWeb: starting up..." << std::endl;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        const char* err = SDL_GetError();
        std::cerr << "SDL_Init failed: " << (err ? err : "(null)") << std::endl;
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    g_window = new SDLWindow("FenixWeb", 800, 600, false, false, WindowOptions{}, false);

    auto loadProc = (GLADloadproc)SDL_GL_GetProcAddress;
    if (!gladLoadGLLoader(loadProc)) {
        std::cerr << "GLAD init failed" << std::endl;
        return 1;
    }

    std::cout << "GL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GL renderer: " << glGetString(GL_RENDERER) << std::endl;

    emscripten_set_main_loop(main_loop, 0, 1);

    delete g_window;
    SDL_Quit();
    return 0;
}
