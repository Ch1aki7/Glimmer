#pragma once

#include "Glimmer.h"

#include <functional>
#include <string>
#include <vector>

namespace gl {

	class PBRMaterialLabTool
	{
	public:
		using ActivateSceneCallback = std::function<bool(const Ref<Scene>&)>;
		using ExitSceneCallback = std::function<void()>;
		using SelectEntityCallback = std::function<void(Entity)>;

		void SetCallbacks(ActivateSceneCallback activateScene,
			ExitSceneCallback exitScene, SelectEntityCallback selectEntity);
		void SetDefaultAssets(AssetHandle sphereModel, AssetHandle material,
			AssetHandle skybox, AssetHandle normalTexture,
			AssetHandle aoTexture, AssetHandle emissiveTexture);
		void OnImGuiRender(const Renderer3D::Statistics& statistics,
			bool anotherTemporaryToolActive);
		void UpdateValidation(const Renderer3D::Statistics& statistics);
		bool GenerateForValidation();
		void Exit();
		bool IsActive() const { return m_Active; }

	private:
		bool Generate();
		bool RunSerializationValidation() const;
		bool DrawAssetTarget(const char* label, AssetType type, AssetHandle& handle);

	private:
		ActivateSceneCallback m_ActivateScene;
		ExitSceneCallback m_ExitScene;
		SelectEntityCallback m_SelectEntity;
		Ref<Scene> m_Scene;
		std::vector<UUID> m_Entities;
		AssetHandle m_SphereModel{ 0 };
		AssetHandle m_Material{ 0 };
		AssetHandle m_Skybox{ 0 };
		AssetHandle m_NormalTexture{ 0 };
		AssetHandle m_AOTexture{ 0 };
		AssetHandle m_EmissiveTexture{ 0 };
		glm::vec3 m_Origin{ 0.0f };
		float m_Spacing = 2.4f;
		std::string m_Status = "Generate an isolated material-channel scene.";
		bool m_Succeeded = true;
		bool m_Active = false;
		bool m_ValidationLogged = false;
		uint32_t m_ExpectedItems = 0;
	};

}
