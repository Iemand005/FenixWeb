#include "FenixWebGame.hpp"
#include "Scene.hpp"
#include "Camera.hpp"
#include "Object.hpp"
#include "Mesh.hpp"
#include "Primitives.hpp"
#include "XRGame.hpp"
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
    if (cssW <= 0 || cssH <= 0) return;

    double dpr = emscripten_get_device_pixel_ratio();
    int fbW = (int)(cssW * dpr);
    int fbH = (int)(cssH * dpr);

    int bw = 0, bh = 0;
    emscripten_get_canvas_element_size("#canvas", &bw, &bh);

    if (bw != fbW || bh != fbH) {
        emscripten_set_canvas_element_size("#canvas", fbW, fbH);
        if (g_game) g_game->Resize(fbW, fbH);
        g_lastW = (int)cssW;
        g_lastH = (int)cssH;
        std::cout << "[resize] css=" << g_lastW << "x" << g_lastH
                  << " fb=" << fbW << "x" << fbH << " dpr=" << dpr << std::endl;
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

    {
        static int frameCount = 0;
        if (frameCount < 3) {
            GLenum err = glGetError();
            std::cout << "[frame " << frameCount << "] glError=0x" << std::hex << err << std::dec << std::endl;
            frameCount++;
        }
    }
}

extern "C" {

EMSCRIPTEN_KEEPALIVE void ToggleImGui() {
    if (g_game) g_game->ToggleDebugUI();
}

EMSCRIPTEN_KEEPALIVE int IsImGuiVisible() {
    return g_game ? (g_game->IsDebugUIShown() ? 1 : 0) : 0;
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

    double cssW = 0, cssH = 0;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    int initW = cssW > 0 ? (int)cssW : 800;
    int initH = cssH > 0 ? (int)cssH : 600;
    std::cout << "[init] CSS size: " << initW << "x" << initH << std::endl;

    fe::XRGameOptions options;
    options.loadProc = reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress);
    g_game = new FenixWebGame(options);
    std::cout << "[init] FenixWebGame created" << std::endl;

    g_game->NewWindow(initW, initH, false, false, false);
    std::cout << "[init] Window created. windows=" << g_game->windows.size() << std::endl;

    emscripten_set_canvas_element_size("#canvas", initW, initH);
    g_game->Resize(initW, initH);
    g_lastW = initW;
    g_lastH = initH;

    g_game->InitImGui();
    std::cout << "[init] ImGui initialized" << std::endl;

    g_game->SetClearColor(1.0f, 0.0f, 0.0f, 1.0f);

    g_game->camera = std::make_unique<Camera>(45.0f, 0.1f, 100.0f);
    g_game->camera->SetPos(glm::vec3(0.0f, 0.0f, 3.0f));
    g_game->camera->SetAspect(initW, initH);
    std::cout << "[init] Camera set up" << std::endl;

    std::cout << "[init] Loading shaders..." << std::endl;
    g_game->LoadShaders(
        "resources/shaders/VertexShader.glsl",
        "resources/shaders/FragmentShader.glsl"
    );
    std::cout << "[init] Shaders loaded" << std::endl;

    g_game->scene = std::make_unique<Scene>();
    std::cout << "[init] Scene created" << std::endl;

    auto cube = Primitives::GenerateCube();
    g_cube = g_game->scene->AddObject(std::move(cube)).get();
    g_cube->color = glm::vec3(1.0f, 0.2f, 0.2f);
    std::cout << "[init] Cube added. objects=" << g_game->scene->GetObjects().size() << std::endl;

    std::cout << "FenixWeb: initialized at " << initW << "x" << initH << std::endl;

    emscripten_set_main_loop(main_loop, 0, 1);

    delete g_game;
    SDL_Quit();
    return 0;
}