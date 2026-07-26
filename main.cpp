#include "Renderer.hpp"
#include "Scene.hpp"
#include "Camera.hpp"
#include "Object.hpp"
#include "Mesh.hpp"
#include "window/SDLWindow.hpp"
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <glad/glad.h>
#include <iostream>
#include <cstdio>
#include <cmath>

using namespace fe;

static Renderer* g_renderer = nullptr;
static Object* g_cube = nullptr;

void main_loop() {
    if (!g_renderer) return;

    auto* window = g_renderer->GetWindow();
    if (!window) return;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            emscripten_cancel_main_loop();
            return;
        }
        if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            int w = event.window.data1;
            int h = event.window.data2;
            emscripten_set_canvas_element_size("#canvas", w, h);
            if (g_renderer) g_renderer->Resize(w, h);
        }
    }

    if (g_cube) {
        float t = (float)window->GetTime();
        g_cube->SetRotation(glm::vec3(t * 50.0f, t * 30.0f, 0.0f));
    }

    g_renderer->Redraw();
}

static Mesh<Vertex> CreateCube() {
    std::vector<Vertex> verts;
    auto P = [&](float x, float y, float z, float nx, float ny, float nz, float u, float v) {
        verts.push_back(Vertex(x, y, z, nx, ny, nz, u, v));
    };
    // Front
    P(-0.5f,-0.5f, 0.5f,  0, 0, 1,  0,0); P( 0.5f,-0.5f, 0.5f,  0, 0, 1,  1,0);
    P( 0.5f, 0.5f, 0.5f,  0, 0, 1,  1,1); P(-0.5f, 0.5f, 0.5f,  0, 0, 1,  0,1);
    // Back
    P( 0.5f,-0.5f,-0.5f,  0, 0,-1,  0,0); P(-0.5f,-0.5f,-0.5f,  0, 0,-1,  1,0);
    P(-0.5f, 0.5f,-0.5f,  0, 0,-1,  1,1); P( 0.5f, 0.5f,-0.5f,  0, 0,-1,  0,1);
    // Top
    P(-0.5f, 0.5f, 0.5f,  0, 1, 0,  0,0); P( 0.5f, 0.5f, 0.5f,  0, 1, 0,  1,0);
    P( 0.5f, 0.5f,-0.5f,  0, 1, 0,  1,1); P(-0.5f, 0.5f,-0.5f,  0, 1, 0,  0,1);
    // Bottom
    P(-0.5f,-0.5f,-0.5f,  0,-1, 0,  0,0); P( 0.5f,-0.5f,-0.5f,  0,-1, 0,  1,0);
    P( 0.5f,-0.5f, 0.5f,  0,-1, 0,  1,1); P(-0.5f,-0.5f, 0.5f,  0,-1, 0,  0,1);
    // Right
    P( 0.5f,-0.5f, 0.5f,  1, 0, 0,  0,0); P( 0.5f,-0.5f,-0.5f,  1, 0, 0,  1,0);
    P( 0.5f, 0.5f,-0.5f,  1, 0, 0,  1,1); P( 0.5f, 0.5f, 0.5f,  1, 0, 0,  0,1);
    // Left
    P(-0.5f,-0.5f,-0.5f, -1, 0, 0,  0,0); P(-0.5f,-0.5f, 0.5f, -1, 0, 0,  1,0);
    P(-0.5f, 0.5f, 0.5f, -1, 0, 0,  1,1); P(-0.5f, 0.5f,-0.5f, -1, 0, 0,  0,1);

    std::vector<unsigned int> indices;
    for (unsigned int i = 0; i < 6; ++i) {
        unsigned int off = i * 4;
        indices.push_back(off + 0); indices.push_back(off + 1); indices.push_back(off + 2);
        indices.push_back(off + 2); indices.push_back(off + 3); indices.push_back(off + 0);
    }

    return Mesh<Vertex>(verts, indices);
}

int main() {
#ifdef __EMSCRIPTEN__
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

    // Request WebGL 2 / OpenGL ES 3.0
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Read actual canvas pixel size — CSS size reflects the fullscreen layout
    double cssW = 0, cssH = 0;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    int canvasW = cssW > 0 ? (int)cssW : 800;
    int canvasH = cssH > 0 ? (int)cssH : 600;

    // Create renderer (loads GLAD + creates OpenGLRenderDevice)
    g_renderer = new Renderer(SDL_GL_GetProcAddress);

    // Create window at actual canvas size
    g_renderer->NewWindow(canvasW, canvasH, false, false, false);

    // SDL_CreateWindow may have overridden the canvas size — restore fullscreen
    emscripten_set_canvas_element_size("#canvas", canvasW, canvasH);
    g_renderer->Resize(canvasW, canvasH);

    // Red clear color
    g_renderer->SetClearColor(1.0f, 0.0f, 0.0f, 1.0f);

    // Create camera with correct initial aspect ratio
    g_renderer->camera = std::make_unique<Camera>(45.0f, 0.1f, 100.0f);
    g_renderer->camera->SetPos(glm::vec3(0.0f, 0.0f, 3.0f));
    g_renderer->camera->SetAspect(canvasW, canvasH);

    // Load shaders
    g_renderer->LoadShaders(
        "resources/shaders/VertexShader.glsl",
        "resources/shaders/FragmentShader.glsl"
    );

    // Create scene with a cube
    g_renderer->scene = std::make_unique<Scene>();
    auto cube = CreateCube();
    g_cube = g_renderer->scene->AddObject(std::move(cube)).get();
    g_cube->color = glm::vec3(1.0f, 0.2f, 0.2f);

    std::cout << "FenixWeb: initialized at " << canvasW << "x" << canvasH << std::endl;

    // Run main loop
    emscripten_set_main_loop(main_loop, 0, 1);

    delete g_renderer;
    SDL_Quit();
    return 0;
}
