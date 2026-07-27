#pragma once
#define FE_EXCLUDE_GLFW
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "Renderer.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

namespace fe {

	class FenixWebGame : public Renderer {
		bool showDebugUI_ = true;

	public:
		FenixWebGame(GLADloadproc loadProc) : Renderer(loadProc) {}

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
			DrawDebugUI();
			EndFrame();
		}

		void DrawDebugUI() {
			ImGui::Begin("FenixWeb Debug");
			{
				ImGui::Text("Graphics: %s", ImGui::GetIO().BackendRendererName ? ImGui::GetIO().BackendRendererName : "Unknown");
				if (renderDevice) ImGui::Text("Device: %s", renderDevice->GetDeviceName());

				ImGui::Text("FPS: %.1f", fpsCounter.deltaTime > 0.0 ? 1.0 / fpsCounter.deltaTime : 0.0);
				ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

				if (scene) {
					ImGui::Text("Objects: %zu", scene->GetObjects().size());
					size_t totalVertices = 0;
					for (auto& obj : scene->GetObjects()) totalVertices += obj->GetTotalVertexCount();
					ImGui::Text("Vertices: %zu", totalVertices);
				}

				ImGui::Checkbox("Frustum Culling", &frustumCullingEnabled);

				if (camera) {
					glm::vec3 cp = camera->GetPos();
					float p[3] = {cp.x, cp.y, cp.z};
					if (ImGui::DragFloat3("Camera Pos", p)) camera->SetPos(glm::vec3(p[0], p[1], p[2]));

					float fov = camera->GetFOV();
					if (ImGui::SliderFloat("FOV", &fov, -10.0f, 179.0f, "%.1f deg")) {
						camera->SetFOV(fov);
					}
				}

				if (ImGui::Checkbox("VSync", &vsyncEnabled)) {
					SetVSync(vsyncEnabled);
				}

				static bool wireframe = false;
				if (ImGui::Checkbox("Wireframe", &wireframe)) {
					if (wireframe) EnableWireframe();
					else DisableWireframe();
				}
			}
			ImGui::End();

			if (scene) {
				ImGui::Begin("Objects");
				{
					for (auto& object : scene->GetObjects()) {
						ImGui::PushID(object.get());
						ImGui::Text("%s", object->name.c_str());
						ImGui::DragFloat3("Position", &object->state.position.x, 0.1f);
						ImGui::DragFloat3("Rotation", &object->state.rotation.x, 1.0f);
						ImGui::DragFloat3("Scale", &object->state.scale.x, 0.01f);
						ImGui::Separator();
						ImGui::PopID();
					}

					auto lights = scene->GetLights();
					for (int i = 0; i < scene->GetLightCount(); ++i) {
						ImGui::Text("Light %i", i);
						float step = 0.1f;
						ImGui::DragFloat3(("Position##light" + std::to_string(i)).c_str(), &lights[i].position.x, step);
						ImGui::DragFloat3(("Color##light" + std::to_string(i)).c_str(), &lights[i].color.x, step);
						ImGui::DragFloat(("Radius##light" + std::to_string(i)).c_str(), &lights[i].radius, step);
						ImGui::DragFloat(("Intensity##light" + std::to_string(i)).c_str(), &lights[i].intensity, step);
					}
				}
				ImGui::End();
			}
		}
	};

}
