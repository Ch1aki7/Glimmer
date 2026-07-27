#pragma once
#include "Glimmer.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/ShaderPanel.h"
#include "Panels/TerrainPanel.h"
#include "Glimmer/Renderer/EditorCamera.h"
#include "Glimmer/Renderer/TerrainMesh.h"

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
		void OnScenePlay();
		void OnSceneStop();

	private:
		ShaderLibrary m_ShaderLib;
		ShaderPanel m_ShaderPanel;
		TerrainPanel m_TerrainPanel;

		Ref<Texture2D> m_Texture;
		Ref<Texture2D> m_STSTexture;
		Ref<Texture2D> m_HenryTexture;
		Ref<Texture2D> m_WhiteTexture;
		EditorCamera m_EditorCamera;

		// 3D 模型
		Ref<Framebuffer> m_Framebuffer;
		Ref<Framebuffer> m_PostProcessFB;
		bool m_PostProcessEnabled = false;
		uint32_t m_FinalSceneTexture = 0;

		// 场景 & 层级面板
		Ref<Scene> m_EditorScene;
		Ref<Scene> m_RuntimeScene;
		Ref<Scene> m_ActiveScene;
		SceneHierarchyPanel m_HierarchyPanel;
		ContentBrowserPanel m_ContentBrowser;

		// 视口
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2];
		bool m_ViewportFocused = false, m_ViewportHovered = false;

		// Terrain
		Ref<TerrainMesh> m_TerrainMesh;
		Ref<Texture2D> m_HeightMapTexture;
		Scope<TerrainGenerator> m_TerrainGenerator;
		Ref<Shader> m_TerrainShader;
		bool m_UseProceduralTerrain = true;
		float m_TerrainMaxHeight = 24.0f;

		// Gizmos
		int m_GizmoType = 0; // 0=Translate, 1=Rotate, 2=Scale

		// 场景状态
		enum class SceneState { Edit = 0, Play = 1 };
		SceneState m_SceneState = SceneState::Edit;


		// 设置灯光参数
		glm::vec4 m_SquareColor = { 0.1f, 0.1f, 0.1f, 1 };
	};

}
