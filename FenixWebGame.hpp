#pragma once
#include "XRGame.hpp"
#define FE_EXCLUDE_GLFW
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "EditableGame.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

namespace fe {

	class FenixWebGame : public EditableGame {
		bool showDebugUI_ = true;

	public:
		FenixWebGame(fe::XRGameOptions options) : EditableGame(options) {}

		void ToggleDebugUI() { showDebugUI_ = !showDebugUI_; }
		bool IsDebugUIShown() const { return showDebugUI_; }

		void InitImGui() {
			auto* window = GetWindow<SDLWindow>();
			if (!window) return;

			const char* glsl_version = "#version 300 es";
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

			ImGui::StyleColorsDark();

			ImGui_ImplSDL3_InitForOpenGL(window->GetWindow(), window->GetSDLGLContext());
			ImGui_ImplOpenGL3_Init(glsl_version);
		}

		void BeginFrame() {
			auto* primaryWindow = GetWindow<SDLWindow>();
			if (primaryWindow && primaryWindow->GetSDLGLContext())
				SDL_GL_MakeCurrent(primaryWindow->GetWindow(), primaryWindow->GetSDLGLContext());
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();
		}

		void EndFrame() {
			ImGui::Render();
			auto* primaryWindow = GetWindow<SDLWindow>();
			if (primaryWindow && primaryWindow->GetSDLGLContext())
				SDL_GL_MakeCurrent(primaryWindow->GetWindow(), primaryWindow->GetSDLGLContext());
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		void DrawUI() override {
			if (!showDebugUI_) return;
			BeginFrame();
			EditableGame::DrawDebugUI();
			EndFrame();
		}
	};

}
