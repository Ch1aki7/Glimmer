#include "TerrainSamplingBenchmarkTool.h"

#include "Glimmer/Core/Log.h"

#include <imgui.h>

#include <array>
#include <limits>

namespace gl {
	namespace {
		struct Configuration
		{
			const char* Name;
			TerrainRenderer::SamplingMode Mode;
		};

		constexpr std::array<Configuration, 3> Configurations = { {
			{ "Full 4 Layers", TerrainRenderer::SamplingMode::FullFourLayers },
			{ "Top 2 Layers", TerrainRenderer::SamplingMode::TopTwoLayers },
			{ "Top 2 + Dominant Normal/AO",
				TerrainRenderer::SamplingMode::TopTwoDominantDetail }
		} };
	}

	bool TerrainSamplingBenchmarkTool::Start(bool waitForTexturedTerrain)
	{
		m_WarmupSamples = std::clamp(m_WarmupSamples, 4u, 120u);
		m_SamplesPerConfiguration = std::clamp(
			m_SamplesPerConfiguration, 5u, 300u);
		m_Results.clear();
		m_ConfigurationIndex = 0;
		m_RestoreMode = TerrainRenderer::GetSamplingMode();
		m_State = waitForTexturedTerrain ? State::WaitingForTerrain : State::Warmup;
		m_Status = waitForTexturedTerrain
			? "Waiting for a textured Terrain draw."
			: "Benchmark running. Keep camera, viewport and scene unchanged.";
		BeginConfiguration(0);
		return true;
	}

	void TerrainSamplingBenchmarkTool::BeginConfiguration(size_t index)
	{
		m_ConfigurationIndex = index;
		m_WarmupRemaining = m_WarmupSamples;
		m_SamplesCollected = 0;
		m_SumMilliseconds = 0.0;
		m_MinimumMilliseconds = std::numeric_limits<float>::max();
		m_MaximumMilliseconds = 0.0f;
		m_LastTimingSample = 0;
		TerrainRenderer::SetSamplingMode(Configurations[index].Mode);
	}

	void TerrainSamplingBenchmarkTool::Cancel(const char* status)
	{
		TerrainRenderer::SetSamplingMode(m_RestoreMode);
		m_State = State::Idle;
		m_Status = status;
	}

	void TerrainSamplingBenchmarkTool::FinishConfiguration()
	{
		Result result;
		result.Mode = Configurations[m_ConfigurationIndex].Mode;
		result.Samples = m_SamplesCollected;
		result.AverageMilliseconds = static_cast<float>(
			m_SumMilliseconds / static_cast<double>(m_SamplesCollected));
		result.MinimumMilliseconds = m_MinimumMilliseconds;
		result.MaximumMilliseconds = m_MaximumMilliseconds;
		m_Results.push_back(result);

		const size_t next = m_ConfigurationIndex + 1;
		if (next < Configurations.size())
		{
			BeginConfiguration(next);
			m_State = State::Warmup;
			return;
		}

		TerrainRenderer::SetSamplingMode(m_RestoreMode);
		m_State = State::Complete;
		m_Status = "Benchmark complete; results are runtime-only.";
		GL_CORE_INFO("Terrain Sampling Benchmark PASS: {0} configurations completed.",
			m_Results.size());
		for (size_t index = 0; index < m_Results.size(); ++index)
		{
			const Result& completed = m_Results[index];
			GL_CORE_INFO(
				"Terrain Sampling Result: mode={0}, samples={1}, avg={2:.3f} ms, min={3:.3f} ms, max={4:.3f} ms",
				Configurations[index].Name, completed.Samples,
				completed.AverageMilliseconds,
				completed.MinimumMilliseconds,
				completed.MaximumMilliseconds);
		}
		const float baseline = m_Results.front().AverageMilliseconds;
		for (size_t index = 1; index < m_Results.size(); ++index)
		{
			const float improvement = baseline > 0.0f
				? (baseline - m_Results[index].AverageMilliseconds) / baseline * 100.0f
				: 0.0f;
			GL_CORE_INFO("Terrain Sampling Improvement: mode={0}, vsFull={1:.1f}%",
				Configurations[index].Name, improvement);
		}
	}

	void TerrainSamplingBenchmarkTool::Update(
		const TerrainRenderer::Statistics& statistics)
	{
		if (m_State == State::WaitingForTerrain)
		{
			if (statistics.DrawCalls == 0 || statistics.BoundMaterialTextures == 0)
				return;
			m_State = State::Warmup;
			m_Status = "Benchmark running. Keep camera, viewport and scene unchanged.";
		}
		if (m_State != State::Warmup && m_State != State::Sampling)
			return;
		if (statistics.DrawCalls == 0)
		{
			Cancel("Benchmark stopped because no Terrain was rendered.");
			return;
		}
		if (statistics.BoundMaterialTextures == 0)
		{
			Cancel("Benchmark requires a TerrainMaterial with concrete textures.");
			return;
		}
		if (!statistics.GpuTimingAvailable
			|| statistics.GpuTimingSample == m_LastTimingSample
			|| statistics.Mode != Configurations[m_ConfigurationIndex].Mode)
			return;

		m_LastTimingSample = statistics.GpuTimingSample;
		if (m_State == State::Warmup)
		{
			if (m_WarmupRemaining > 0)
				--m_WarmupRemaining;
			if (m_WarmupRemaining == 0)
				m_State = State::Sampling;
			return;
		}

		m_SumMilliseconds += statistics.GpuMilliseconds;
		m_MinimumMilliseconds = std::min(
			m_MinimumMilliseconds, statistics.GpuMilliseconds);
		m_MaximumMilliseconds = std::max(
			m_MaximumMilliseconds, statistics.GpuMilliseconds);
		++m_SamplesCollected;
		if (m_SamplesCollected >= m_SamplesPerConfiguration)
			FinishConfiguration();
	}

	void TerrainSamplingBenchmarkTool::OnImGuiRender(
		const TerrainRenderer::Statistics& statistics)
	{
		ImGui::SeparatorText("Terrain Sampling Benchmark");
		ImGui::TextWrapped(
			"Compares unique asynchronous Terrain GPU timer samples. Use a textured TerrainMaterial and do not move the camera or resize the viewport while running.");
		const bool running = m_State == State::WaitingForTerrain
			|| m_State == State::Warmup || m_State == State::Sampling;
		int warmup = static_cast<int>(m_WarmupSamples);
		int samples = static_cast<int>(m_SamplesPerConfiguration);
		ImGui::BeginDisabled(running);
		if (ImGui::DragInt("Terrain Warmup Samples", &warmup, 1.0f, 4, 120))
			m_WarmupSamples = static_cast<uint32_t>(warmup);
		if (ImGui::DragInt("Terrain Samples / Mode", &samples, 1.0f, 5, 300))
			m_SamplesPerConfiguration = static_cast<uint32_t>(samples);
		ImGui::BeginDisabled(statistics.BoundMaterialTextures == 0);
		if (ImGui::Button("Start Terrain Benchmark"))
			Start();
		ImGui::EndDisabled();
		ImGui::EndDisabled();
		if (running)
		{
			ImGui::SameLine();
			if (ImGui::Button("Cancel Terrain Benchmark"))
				Cancel("Benchmark cancelled.");
		}
		ImGui::Text("Bound Terrain textures: %u",
			statistics.BoundMaterialTextures);
		ImGui::TextWrapped("%s", m_Status.c_str());
		if (running && m_State != State::WaitingForTerrain)
		{
			ImGui::Text("Mode %zu / %zu: %s", m_ConfigurationIndex + 1,
				Configurations.size(), Configurations[m_ConfigurationIndex].Name);
			if (m_State == State::Warmup)
				ImGui::Text("Warmup remaining: %u", m_WarmupRemaining);
			else
				ImGui::Text("Samples: %u / %u", m_SamplesCollected,
					m_SamplesPerConfiguration);
		}
		for (size_t index = 0; index < m_Results.size(); ++index)
		{
			const Result& result = m_Results[index];
			ImGui::Text("%s: %.3f ms (%.3f - %.3f)",
				Configurations[index].Name, result.AverageMilliseconds,
				result.MinimumMilliseconds, result.MaximumMilliseconds);
		}
	}
}
