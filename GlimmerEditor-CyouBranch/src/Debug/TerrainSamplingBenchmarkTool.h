#pragma once

#include "Glimmer/Renderer/TerrainRenderer.h"

namespace gl {

	class TerrainSamplingBenchmarkTool
	{
	public:
		void Update(const TerrainRenderer::Statistics& statistics);
		void OnImGuiRender(const TerrainRenderer::Statistics& statistics);
		bool Start(bool waitForTexturedTerrain = false);
		bool IsComplete() const { return m_State == State::Complete; }

	private:
		struct Result
		{
			TerrainRenderer::SamplingMode Mode =
				TerrainRenderer::SamplingMode::FullFourLayers;
			uint32_t Samples = 0;
			float AverageMilliseconds = 0.0f;
			float MinimumMilliseconds = 0.0f;
			float MaximumMilliseconds = 0.0f;
		};

		enum class State
		{
			Idle,
			WaitingForTerrain,
			Warmup,
			Sampling,
			Complete
		};

		void BeginConfiguration(size_t index);
		void FinishConfiguration();
		void Cancel(const char* status);

	private:
		std::vector<Result> m_Results;
		State m_State = State::Idle;
		size_t m_ConfigurationIndex = 0;
		uint32_t m_WarmupSamples = 15;
		uint32_t m_SamplesPerConfiguration = 30;
		uint32_t m_WarmupRemaining = 0;
		uint32_t m_SamplesCollected = 0;
		double m_SumMilliseconds = 0.0;
		float m_MinimumMilliseconds = 0.0f;
		float m_MaximumMilliseconds = 0.0f;
		uint64_t m_LastTimingSample = 0;
		TerrainRenderer::SamplingMode m_RestoreMode =
			TerrainRenderer::SamplingMode::FullFourLayers;
		std::string m_Status =
			"Assign a textured TerrainMaterial, fix the camera, then start.";
	};
}
