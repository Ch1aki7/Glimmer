#pragma once
#include "Glimmer.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/ShaderPanel.h"
#include "Panels/DebugPanel.h"
#include "Glimmer/Renderer/EditorCamera.h"
#include "Editor/EditorCommand.h"

namespace gl {

	class EditorLayer : public Layer {
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		virtual void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& event) override;


	private:
		void SetEditorScene(const Ref<Scene>& scene);
		bool ActivateTemporaryDebugScene(const Ref<Scene>& scene);
		void ExitTemporaryDebugScene();
		void OnScenePlay();
		void OnSceneStop();
		void FocusSelectedEntity();

	private:
		ShaderLibrary m_ShaderLib;
		ShaderPanel m_ShaderPanel;
		DebugPanel m_DebugPanel;

		EditorCamera m_EditorCamera;
		Ref<Shader> m_SkyboxShader;
		// 3D 模型
		Ref<Framebuffer> m_Framebuffer;
		Ref<Framebuffer> m_DisplayFramebuffer;
		bool m_GrayscaleEnabled = false;
		float m_Exposure = 1.0f;
		bool m_DistanceFogEnabled = false;
		float m_DistanceFogDensity = 0.012f;
		float m_DistanceFogStart = 60.0f;
		float m_DistanceFogEnd = 260.0f;
		glm::vec3 m_DistanceFogColor = { 0.55f, 0.65f, 0.75f };
		uint32_t m_FinalSceneTexture = 0;

		// 场景 & 层级面板
		Ref<Scene> m_EditorScene;
		Ref<Scene> m_RuntimeScene;
		Ref<Scene> m_ActiveScene;
		SceneHierarchyPanel m_HierarchyPanel;
		InspectorPanel m_InspectorPanel;
		EditorCommandHistory m_CommandHistory;
		SelectionContext m_SelectionContext;
		ContentBrowserPanel m_ContentBrowser;

		// 视口
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2];
		bool m_ViewportFocused = false, m_ViewportHovered = false;

		// Gizmos
		int m_GizmoType = 0; // 0=Translate, 1=Rotate, 2=Scale

		// 场景状态
		enum class SceneState { Edit = 0, Play = 1 };
		SceneState m_SceneState = SceneState::Edit;
		bool m_ShadowBenchmarkAutorun = false;
		bool m_TerrainSamplingBenchmarkAutorun = false;


		// 设置灯光参数
		glm::vec4 m_SquareColor = { 0.1f, 0.1f, 0.1f, 1 };
	};

}
