#pragma once

#include "Glimmer.h"
#include "Glimmer/Renderer/ShadowRenderer.h"

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
		using FrameSceneCallback = std::function<void(
			const glm::vec3&, float, float, float)>;

		void SetCallbacks(
			ActivateSceneCallback activateScene,
			ExitSceneCallback exitScene,
			SelectEntityCallback selectEntity,
			FrameSceneCallback frameScene);
		void SetDefaultAssets(
			AssetHandle modelHandle,
			AssetHandle materialHandle,
			AssetHandle skyboxHandle);

		void OnImGuiRender(const Renderer3D::Statistics& statistics);
		void UpdateShadowBenchmark(const ShadowRenderer::Statistics& statistics);
		bool GenerateForShadowBenchmark();
		bool GenerateForShadowVisualValidation(bool casterCloseup = false);
		void Exit();

		bool IsActive() const { return m_Active; }
		bool IsShadowBenchmarkComplete() const
		{
			return m_ShadowBenchmarkState == ShadowBenchmarkState::Complete;
		}
		const Ref<Scene>& GetScene() const { return m_Scene; }

	private:
		enum class Preset : int
		{
			MaximumInstancing = 0,
			MaterialSplit,
			TransparentComparison,
			ShadowVisualValidation
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

		struct ShadowBenchmarkConfiguration
		{
			uint32_t Cascades = 1;
			uint32_t Resolution = 1024;
		};

		struct ShadowBenchmarkResult
		{
			ShadowBenchmarkConfiguration Configuration;
			uint32_t Samples = 0;
			float AverageMilliseconds = 0.0f;
			float MinimumMilliseconds = 0.0f;
			float MaximumMilliseconds = 0.0f;
			uint32_t DrawCalls = 0;
			uint32_t SavedDrawCalls = 0;
		};

		enum class ShadowBenchmarkState
		{
			Idle,
			Warmup,
			Sampling,
			Complete
		};

		bool Generate();
		bool DrawAssetTarget(const char* label, AssetType type, AssetHandle& handle);
		void DrawExpectedAndActual(const Renderer3D::Statistics& statistics) const;
		void SelectRepresentative(size_t index) const;
		bool StartShadowBenchmark();
		void CancelShadowBenchmark(const char* status = nullptr);
		bool ApplyShadowBenchmarkConfiguration(size_t index);
		void FinishShadowBenchmarkConfiguration(
			const ShadowRenderer::Statistics& statistics);
		void DrawShadowBenchmark();
		void DrawShadowVisualControls();
		void FrameShadowVisualOverview() const;
		void FrameShadowVisualCasters() const;
		ExpectedStatistics CalculateExpectedStatistics(uint32_t submeshCount) const;
		uint32_t GetRequestedEntityCount() const;

	private:
		ActivateSceneCallback m_ActivateScene;
		ExitSceneCallback m_ExitScene;
		SelectEntityCallback m_SelectEntity;
		FrameSceneCallback m_FrameScene;

		Ref<Scene> m_Scene;
		std::vector<UUID> m_RepresentativeEntities;
		UUID m_SunEntity{ 0 };
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

		std::vector<ShadowBenchmarkResult> m_ShadowBenchmarkResults;
		ShadowBenchmarkState m_ShadowBenchmarkState = ShadowBenchmarkState::Idle;
		size_t m_ShadowBenchmarkConfigurationIndex = 0;
		uint32_t m_ShadowBenchmarkWarmupFrames = 15;
		uint32_t m_ShadowBenchmarkSamplesPerConfiguration = 30;
		uint32_t m_ShadowBenchmarkWarmupRemaining = 0;
		uint32_t m_ShadowBenchmarkSamplesCollected = 0;
		double m_ShadowBenchmarkSumMilliseconds = 0.0;
		float m_ShadowBenchmarkMinimumMilliseconds = 0.0f;
		float m_ShadowBenchmarkMaximumMilliseconds = 0.0f;
		uint64_t m_LastShadowTimingSample = 0;
		std::string m_ShadowBenchmarkStatus =
			"Generate the lab, frame the scene, then start the benchmark.";
	};

}
