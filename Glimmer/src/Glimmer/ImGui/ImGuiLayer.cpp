#include "glpch.h"
#include "ImGuiLayer.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <ImGuizmo.h>

#include "Glimmer/Core/Application.h"

// 暂时包含 GLFW
#include <GLFW/glfw3.h>

namespace gl {

	ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}
	ImGuiLayer::~ImGuiLayer() {}

	void ImGuiLayer::OnAttach() {
		GL_PROFILE_FUNCTION();

		// 设置 ImGui 上下文
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 允许键盘控制
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // 允许停靠
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // 允许多窗口拖拽

		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

		//io.Fonts->AddFontFromFileTTF("assets/fonts/Montenegrin_Gothic_One/MontenegrinGothicOne-Regular.ttf", 16.0f);
		//io.Fonts->AddFontFromFileTTF("assets/fonts/Josefin_Sans/static/JosefinSans-ExtraLight.ttf", 16.0f);
		//io.Fonts->AddFontFromFileTTF("assets/fonts/Caveat/static/Caveat-Regular.ttf", 20.0f);
		io.Fonts->AddFontFromFileTTF("assets/fonts/Open_Sans/static/OpenSans_SemiCondensed-Italic.ttf", 20.0f);

		// Font Awesome 6 图标字体（合并模式）
		static const ImWchar faRanges[] = { 0xf000, 0xf2ff, 0 }; // FA 图标码点范围
		ImFontConfig faConfig;
		faConfig.MergeMode = true;
		faConfig.GlyphMinAdvanceX = 16.0f;
		faConfig.GlyphOffset = ImVec2(0, 2);
		io.Fonts->AddFontFromFileTTF("assets/fonts/FontAwesome/fa-solid-900.otf", 16.0f, &faConfig, faRanges);

		// 多窗口模式下的样式微调
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			ImGui::StyleColorsLight();
			style.ScaleAllSizes(1.2f);
			style.WindowPadding = { 16.0f, 16.0f };
			style.FramePadding = { 8.0f, 5.0f };

			style.WindowTitleAlign = { 0.5f, 0.5f };

			style.MouseCursorScale = 0.5f;

			style.WindowRounding = 16.0f;
			style.ChildRounding = 12.0f;
			style.PopupRounding = 16.0f;
			style.FrameRounding = 16.0f;
			style.GrabRounding = 12.0f;

			style.FrameBorderSize = 1;
			style.PopupBorderSize = 1;
		}

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

		// 初始化后端
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 410"); // 指定 OpenGL 核心版本
	}

	void ImGuiLayer::OnDetach() {
		GL_PROFILE_FUNCTION();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::Begin() {
		GL_PROFILE_FUNCTION();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	ImGuizmo::Enable(true);
	}

	void ImGuiLayer::End() {
		GL_PROFILE_FUNCTION();

		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

		// 渲染
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// 多窗口模式的特殊处理：将脱离主窗口的 UI 渲染到桌面
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	void ImGuiLayer::OnUpdate(Timestep ts)
	{
	}

	void ImGuiLayer::OnEvent(Event& event)
	{
		if (!m_BlockEvents)
			return;

		// GLFW backend callbacks already feed mouse, keyboard and text input to ImGui.
		// This layer only prevents captured events from reaching editor/game layers.
		ImGuiIO& io = ImGui::GetIO();
		event.Handled |= event.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
		event.Handled |= event.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
	}
}
