#pragma once
#include "Glimmer.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/ShaderPanel.h"
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
		ShaderLibrary m_ShaderLib;
		ShaderPanel m_ShaderPanel;

		Ref<Texture2D> m_Texture;
		Ref<Texture2D> m_STSTexture;
		Ref<Texture2D> m_HenryTexture;
		Ref<Texture2D> m_WhiteTexture;
		EditorCamera m_EditorCamera;

		Ref<Shader> m_3DShader;
		int m_SelectedShaderIndex = 0;
		const char* m_ShaderNames[4] = { "Phong", "Toon", "Blinn-Phong", "Hologram" };

		// 3D 模型
		std::vector<Ref<Model>> m_Models;
		std::vector<std::string> m_ModelNames;
		int m_SelectedModelIndex = 0;

		Ref<Framebuffer> m_Framebuffer;
		Ref<Framebuffer> m_PostProcessFB;
		bool m_PostProcessEnabled = false;
		uint32_t m_FinalSceneTexture = 0;

		// 场景 & 层级面板
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
		Ref<Texture2D> m_ProceduralHeightMapTexture;
		Ref<Shader> m_TerrainShader;
		bool m_UseProceduralTerrain = true;
		float m_TerrainMaxHeight = 24.0f;

		// Gizmos
		int m_GizmoType = 0; // 0=Translate, 1=Rotate, 2=Scale

		// 场景状态
		enum class SceneState { Edit = 0, Play = 1 };
		SceneState m_SceneState = SceneState::Edit;


		// 设置灯光参数
		glm::vec3 m_LightPos = { 2.0f, 2.0f, 2.0f };
		glm::vec4 m_SquareColor = { 0.1f, 0.1f, 0.1f, 1 };
	};

}
