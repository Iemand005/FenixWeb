#include "FenixWebGame.hpp"
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

using namespace fe;

static FenixWebGame* g_game = nullptr;
static Object* g_cube = nullptr;
static int g_lastW = 0;
static int g_lastH = 0;

static void SyncCanvasSize() {
    double cssW = 0, cssH = 0;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    int w = (int)cssW;
    int h = (int)cssH;
    if (w <= 0 || h <= 0) return;

    int bw = 0, bh = 0;
    emscripten_get_canvas_element_size("#canvas", &bw, &bh);

    if (bw != w || bh != h) {
        emscripten_set_canvas_element_size("#canvas", w, h);
        if (g_game) g_game->Resize(w, h);
        g_lastW = w;
        g_lastH = h;
        std::cout << "[resize] canvas -> " << w << "x" << h << std::endl;
    }
}

void main_loop() {
    if (!g_game) return;

    auto* window = g_game->GetWindow();
    if (!window) return;

    SyncCanvasSize();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        auto io = ImGui::GetIO();

        switch (event.type) {
            case SDL_EVENT_QUIT:
                emscripten_cancel_main_loop();
                return;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                int w = event.window.data1;
                int h = event.window.data2;
                if (w != g_lastW || h != g_lastH) {
                    emscripten_set_canvas_element_size("#canvas", w, h);
                    g_game->Resize(w, h);
                    g_lastW = w;
                    g_lastH = h;
                }
                break;
            }
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_F3) {
                    g_game->ToggleDebugUI();
                }
                break;
        }
    }

    if (g_cube) {
        float t = (float)window->GetTime();
        g_cube->SetRotation(glm::vec3(t * 50.0f, t * 30.0f, 0.0f));
    }

    g_game->Redraw();
}

extern "C" {

EMSCRIPTEN_KEEPALIVE void ToggleImGui() {
    if (g_game) g_game->ToggleDebugUI();
}

EMSCRIPTEN_KEEPALIVE int IsImGuiVisible() {
    return g_game ? (g_game->IsDebugUIShown() ? 1 : 0) : 0;
}

}

static Mesh<Vertex> CreateCube() {
    std::vector<Vertex> verts;
    auto P = [&](float x, float y, float z, float nx, float ny, float nz, float u, float v) {
        verts.push_back(Vertex(x, y, z, nx, ny, nz, u, v));
    };
    P(-0.5f,-0.5f, 0.5f,  0, 0, 1,  0,0); P( 0.5f,-0.5f, 0.5f,  0, 0, 1,  1,0);
    P( 0.5f, 0.5f, 0.5f,  0, 0, 1,  1,1); P(-0.5f, 0.5f, 0.5f,  0, 0, 1,  0,1);
    P( 0.5f,-0.5f,-0.5f,  0, 0,-1,  0,0); P(-0.5f,-0.5f,-0.5f,  0, 0,-1,  1,0);
    P(-0.5f, 0.5f,-0.5f,  0, 0,-1,  1,1); P( 0.5f, 0.5f,-0.5f,  0, 0,-1,  0,1);
    P(-0.5f, 0.5f, 0.5f,  0, 1, 0,  0,0); P( 0.5f, 0.5f, 0.5f,  0, 1, 0,  1,0);
    P( 0.5f, 0.5f,-0.5f,  0, 1, 0,  1,1); P(-0.5f, 0.5f,-0.5f,  0, 1, 0,  0,1);
    P(-0.5f,-0.5f,-0.5f,  0,-1, 0,  0,0); P( 0.5f,-0.5f,-0.5f,  0,-1, 0,  1,0);
    P( 0.5f,-0.5f, 0.5f,  0,-1, 0,  1,1); P(-0.5f,-0.5f, 0.5f,  0,-1, 0,  0,1);
    P( 0.5f,-0.5f, 0.5f,  1, 0, 0,  0,0); P( 0.5f,-0.5f,-0.5f,  1, 0, 0,  1,0);
    P( 0.5f, 0.5f,-0.5f,  1, 0, 0,  1,1); P( 0.5f, 0.5f, 0.5f,  1, 0, 0,  0,1);
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

    double cssW = 0, cssH = 0;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    int initW = cssW > 0 ? (int)cssW : 800;
    int initH = cssH > 0 ? (int)cssH : 600;
    std::cout << "[init] CSS size: " << initW << "x" << initH << std::endl;

    g_game = new FenixWebGame(SDL_GL_GetProcAddress);

    g_game->NewWindow(initW, initH, false, false, false);

    emscripten_set_canvas_element_size("#canvas", initW, initH);
    g_game->Resize(initW, initH);
    g_lastW = initW;
    g_lastH = initH;

    g_game->InitImGui();

    g_game->SetClearColor(1.0f, 0.0f, 0.0f, 1.0f);

    g_game->camera->SetPos(glm::vec3(0.0f, 0.0f, 3.0f));
    g_game->camera->SetAspect(initW, initH);

    g_game->LoadShaders(
        "resources/shaders/VertexShader.glsl",
        "resources/shaders/FragmentShader.glsl"
    );

    auto cube = CreateCube();
    g_cube = g_game->scene->AddObject(std::move(cube)).get();
    g_cube->color = glm::vec3(1.0f, 0.2f, 0.2f);

    std::cout << "FenixWeb: initialized at " << initW << "x" << initH << std::endl;

    emscripten_set_main_loop(main_loop, 0, 1);

    delete g_game;
    SDL_Quit();
    return 0;
}
