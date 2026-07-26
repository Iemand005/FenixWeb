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

static Renderer* g_renderer = nullptr;
static SDLWindow* g_window = nullptr;

void main_loop() {
    if (g_window && g_window->ShouldClose()) {
        emscripten_cancel_main_loop();
        return;
    }
    
    if (g_renderer) {
        g_renderer->Redraw();
    }
}

int main() {
#ifdef __EMSCRIPTEN__
    // Disable buffering so cout/cerr/printf show in browser console
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif

    std::cout << "FenixWeb: starting up..." << std::endl;

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        const char* err = SDL_GetError();
        std::cerr << "SDL_Init failed: " << (err ? err : "(null)") << std::endl;
        return 1;
    }
    
    // Request OpenGL ES 3.0 / WebGL 2 context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    // Create window
    g_window = new SDLWindow("FenixWeb", 800, 600, false, false, WindowOptions{}, false);
    
    if (!g_window->ShouldClose()) {
        // Window created successfully
    }
    
    // Get Emscripten's GL proc address for WebGL via SDL
    auto loadProc = (GLADloadproc)SDL_GL_GetProcAddress;
    
    // Create Renderer with OpenGL (not Vulkan) using GLAD loader
    g_renderer = new Renderer(loadProc);
    g_renderer->CreateRenderDevice(false); // false = OpenGL
    
    // Create window through renderer (proper setup with shared context, etc.)
    g_renderer->NewWindow(800, 600, false, false, false);
    
    // Set up minimal scene
    g_renderer->scene = std::make_unique<Scene>();
    g_renderer->camera = std::make_unique<Camera>(45.0f, 0.1f, 100.0f);
    g_renderer->camera->SetPos(glm::vec3(0.0f, 0.0f, 5.0f));
    
    // Load shaders (will use OpenGL paths)
    g_renderer->LoadShaders(
        "resources/shaders/VertexShader.glsl",
        "resources/shaders/FragmentShader.glsl"
    );
    
    std::cout << "FenixWeb initialized - OpenGL ES 3.0 / WebGL 2 renderer active" << std::endl;
    
    // Run main loop
    emscripten_set_main_loop(main_loop, 0, 1);
    
    // Cleanup (won't reach here in Emscripten)
    delete g_renderer;
    delete g_window;
    SDL_Quit();
    
    return 0;
}