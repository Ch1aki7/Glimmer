#pragma once

#include "Glimmer.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace gl {

	class InstancingLabTool
	{
	public:
		using ActivateSceneCallback = std::function<bool(const Ref<Scene>&)>;
		using ExitSceneCallback = std::function<void()>;
		using SelectEntityCallback = std::function<void(Entity)>;

		void SetCallbacks(
			ActivateSceneCallback activateScene,
			ExitSceneCallback exitScene,
			SelectEntityCallback selectEntity);
		void SetDefaultAssets(
			AssetHandle modelHandle,
			AssetHandle materialHandle,
			AssetHandle skyboxHandle);

		void OnImGuiRender(const Renderer3D::Statistics& statistics);
		void Exit();

		bool IsActive() const { return m_Active; }
		const Ref<Scene>& GetScene() const { return m_Scene; }

	private:
		enum class Preset : int
		{
			MaximumInstancing = 0,
			MaterialSplit,
			TransparentComparison
		};

		struct ExpectedStatistics
		{
			uint32_t Entities = 0;
			uint32_t Items = 0;
			uint32_t DrawCalls = 0;
			uint32_t InstancedDrawCalls = 0;
			uint32_t IndividualDrawCalls = 0;
			uint32_t InstanceCount = 0;
		};

		bool Generate();
		bool DrawAssetTarget(const char* label, AssetType type, AssetHandle& handle);
		void DrawExpectedAndActual(const Renderer3D::Statistics& statistics) const;
		void SelectRepresentative(size_t index) const;
		ExpectedStatistics CalculateExpectedStatistics(uint32_t submeshCount) const;
		uint32_t GetRequestedEntityCount() const;

	private:
		ActivateSceneCallback m_ActivateScene;
		ExitSceneCallback m_ExitScene;
		SelectEntityCallback m_SelectEntity;

		Ref<Scene> m_Scene;
		std::vector<UUID> m_RepresentativeEntities;
		AssetHandle m_ModelHandle{ 0 };
		AssetHandle m_MaterialHandle{ 0 };
		AssetHandle m_SkyboxHandle{ 0 };
		std::array<int, 3> m_Count{ 50, 1, 50 };
		glm::vec3 m_Spacing{ 2.5f, 2.5f, 2.5f };
		glm::vec3 m_Origin{ 0.0f };
		Preset m_Preset = Preset::MaximumInstancing;
		ExpectedStatistics m_Expected;
		float m_LastGenerationMilliseconds = 0.0f;
		std::string m_Status = "Configure assets and generate a temporary lab scene.";
		bool m_LastOperationSucceeded = true;
		bool m_Active = false;
	};

}
