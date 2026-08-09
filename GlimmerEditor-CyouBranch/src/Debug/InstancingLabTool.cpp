#include "InstancingLabTool.h"

#include "Glimmer/Renderer/Model.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <imgui.h>
#include <limits>
#include <utility>

namespace gl {

	namespace {

		constexpr uint32_t MaxInstancesPerDraw = 1024;
		constexpr uint32_t MaxLabEntities = 100000;
		struct ShadowBenchmarkSetting
		{
			uint32_t Cascades;
			uint32_t Resolution;
		};

		constexpr std::array<ShadowBenchmarkSetting, 9>
			ShadowBenchmarkConfigurations = { {
				{ 1, 1024 }, { 2, 1024 }, { 4, 1024 },
				{ 1, 2048 }, { 2, 2048 }, { 4, 2048 },
				{ 1, 4096 }, { 2, 4096 }, { 4, 4096 }
			} };

		uint32_t DivideRoundUp(uint32_t value, uint32_t divisor)
		{
			return value == 0 ? 0 : 1 + (value - 1) / divisor;
		}

		const char* AssetTypeName(AssetType type)
		{
			switch (type)
			{
			case AssetType::Model: return "Model";
			case AssetType::Material: return "Material";
			default: return "Asset";
			}
		}

	}

	void InstancingLabTool::SetCallbacks(
		ActivateSceneCallback activateScene,
		ExitSceneCallback exitScene,
		SelectEntityCallback selectEntity)
	{
		m_ActivateScene = std::move(activateScene);
		m_ExitScene = std::move(exitScene);
		m_SelectEntity = std::move(selectEntity);
	}

	void InstancingLabTool::SetDefaultAssets(
		AssetHandle modelHandle,
		AssetHandle materialHandle,
		AssetHandle skyboxHandle)
	{
		m_ModelHandle = modelHandle;
		m_MaterialHandle = materialHandle;
		m_SkyboxHandle = skyboxHandle;
	}

	bool InstancingLabTool::DrawAssetTarget(
		const char* label, AssetType type, AssetHandle& handle)
	{
		bool changed = false;
		const AssetMetadata metadata = AssetManager::GetMetadata(handle);
		const std::string value = metadata.IsValid()
			? metadata.FilePath.filename().string()
			: std::string("None (drop ") + AssetTypeName(type) + ")";

		ImGui::Text("%s: %s", label, value.c_str());
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
			{
				const std::string path(
					static_cast<const char*>(payload->Data), payload->DataSize - 1);
				const AssetHandle droppedHandle = AssetManager::ImportAsset(path);
				if (AssetManager::GetMetadata(droppedHandle).Type == type)
				{
					handle = droppedHandle;
					changed = true;
				}
			}
			ImGui::EndDragDropTarget();
		}
		return changed;
	}

	uint32_t InstancingLabTool::GetRequestedEntityCount() const
	{
		uint64_t count = 1;
		for (int dimension : m_Count)
			count *= static_cast<uint64_t>(std::max(dimension, 1));
		return static_cast<uint32_t>(std::min<uint64_t>(count, MaxLabEntities));
	}

	InstancingLabTool::ExpectedStatistics
	InstancingLabTool::CalculateExpectedStatistics(uint32_t submeshCount) const
	{
		ExpectedStatistics expected;
		expected.Entities = GetRequestedEntityCount();
		expected.Items = expected.Entities * submeshCount;

		if (m_Preset == Preset::TransparentComparison)
		{
			expected.DrawCalls = expected.Items;
			expected.IndividualDrawCalls = expected.Items;
			return expected;
		}

		const auto addGroup = [&](uint32_t groupSize, ExpectedStatistics& result) {
			if (groupSize == 0)
				return;
			if (groupSize == 1)
			{
				result.DrawCalls += submeshCount;
				result.IndividualDrawCalls += submeshCount;
				return;
			}

			const uint32_t chunks = DivideRoundUp(groupSize, MaxInstancesPerDraw);
			result.DrawCalls += chunks * submeshCount;
			result.InstancedDrawCalls += chunks * submeshCount;
			result.InstanceCount += groupSize * submeshCount;
		};

		if (m_Preset == Preset::MaterialSplit)
		{
			addGroup((expected.Entities + 1) / 2, expected);
			addGroup(expected.Entities / 2, expected);
		}
		else
			addGroup(expected.Entities, expected);

		return expected;
	}

	bool InstancingLabTool::Generate()
	{
		CancelShadowBenchmark();
		if (!m_ActivateScene)
		{
			m_Status = "The editor did not provide a temporary-scene callback.";
			m_LastOperationSucceeded = false;
			return false;
		}

		const Ref<Model> model = AssetManager::GetModel(m_ModelHandle);
		const Ref<Material> material = AssetManager::GetMaterial(m_MaterialHandle);
		if (!model || model->GetMeshes().empty() || !material)
		{
			m_Status = "Select a valid model and material before generating the lab.";
			m_LastOperationSucceeded = false;
			return false;
		}

		for (int& dimension : m_Count)
			dimension = std::clamp(dimension, 1, 1000);
		const uint32_t entityCount = GetRequestedEntityCount();
		if (static_cast<uint64_t>(m_Count[0]) * m_Count[1] * m_Count[2]
			> MaxLabEntities)
		{
			m_Status = "Requested entity count exceeds the 100000 entity safety limit.";
			m_LastOperationSucceeded = false;
			return false;
		}

		const auto start = std::chrono::steady_clock::now();
		Ref<Scene> scene = CreateRef<Scene>();
		Entity sun = scene->CreateEntity("Instancing Lab Sun");
		auto& light = sun.AddComponent<DirectionalLightComponent>();
		light.Intensity = 2.0f;
		light.AmbientIntensity = 0.12f;
		sun.GetComponent<TransformComponent>().Rotation = { -50.0f, 30.0f, 0.0f };

		if (AssetManager::GetMetadata(m_SkyboxHandle).Type == AssetType::Cubemap)
		{
			Entity sky = scene->CreateEntity("Instancing Lab Sky Light");
			sky.AddComponent<SkyLightComponent>(m_SkyboxHandle);
		}

		std::vector<UUID> representatives;
		representatives.reserve(3);
		const uint32_t middleIndex = entityCount / 2;
		const glm::vec3 dimensions{
			static_cast<float>(m_Count[0] - 1),
			static_cast<float>(m_Count[1] - 1),
			static_cast<float>(m_Count[2] - 1) };
		const glm::vec3 firstPosition = m_Origin - dimensions * m_Spacing * 0.5f;

		uint32_t instanceIndex = 0;
		for (int z = 0; z < m_Count[2]; ++z)
		{
			for (int y = 0; y < m_Count[1]; ++y)
			{
				for (int x = 0; x < m_Count[0]; ++x, ++instanceIndex)
				{
					Entity entity = scene->CreateEntity(
						"Instancing Lab Entity " + std::to_string(instanceIndex));
					auto& transform = entity.GetComponent<TransformComponent>();
					transform.Translation = firstPosition + glm::vec3{
						static_cast<float>(x) * m_Spacing.x,
						static_cast<float>(y) * m_Spacing.y,
						static_cast<float>(z) * m_Spacing.z };
					entity.AddComponent<ModelRendererComponent>(m_ModelHandle);
					auto& materialComponent =
						entity.AddComponent<MaterialComponent>(m_MaterialHandle);

					materialComponent.Overrides.Values.AlphaMode =
						m_Preset == Preset::TransparentComparison
						? MaterialAlphaMode::Blend : MaterialAlphaMode::Opaque;
					materialComponent.Overrides.SetEnabled(
						MaterialOverride::AlphaMode, true);

					if (m_Preset == Preset::MaterialSplit)
					{
						materialComponent.Overrides.Values.Roughness =
							(instanceIndex & 1u) == 0 ? 0.2f : 0.8f;
						materialComponent.Overrides.SetEnabled(
							MaterialOverride::Roughness, true);
					}

					if (instanceIndex == 0 || instanceIndex == middleIndex
						|| instanceIndex + 1 == entityCount)
						representatives.push_back(entity.GetUUID());
				}
			}
		}

		if (!m_ActivateScene(scene))
		{
			m_Status = "The editor rejected the temporary lab scene.";
			m_LastOperationSucceeded = false;
			return false;
		}

		m_Scene = std::move(scene);
		m_SunEntity = sun.GetUUID();
		m_RepresentativeEntities = std::move(representatives);
		m_Expected = CalculateExpectedStatistics(
			static_cast<uint32_t>(model->GetMeshes().size()));
		m_Active = true;
		m_LastGenerationMilliseconds = std::chrono::duration<float, std::milli>(
			std::chrono::steady_clock::now() - start).count();
		m_Status = "Temporary lab generated. Statistics validate after the next rendered frame.";
		m_LastOperationSucceeded = true;
		return true;
	}

	void InstancingLabTool::SelectRepresentative(size_t index) const
	{
		if (!m_Scene || !m_SelectEntity || index >= m_RepresentativeEntities.size())
			return;
		m_SelectEntity(m_Scene->FindEntityByUUID(m_RepresentativeEntities[index]));
	}

	bool InstancingLabTool::ApplyShadowBenchmarkConfiguration(size_t index)
	{
		if (!m_Scene || index >= ShadowBenchmarkConfigurations.size())
			return false;
		Entity sun = m_Scene->FindEntityByUUID(m_SunEntity);
		if (!sun || !sun.HasComponent<DirectionalLightComponent>())
			return false;

		const ShadowBenchmarkSetting& configuration =
			ShadowBenchmarkConfigurations[index];
		auto& light = sun.GetComponent<DirectionalLightComponent>();
		light.CastShadows = true;
		light.ShadowCascadeCount = configuration.Cascades;
		light.ShadowMapResolution = configuration.Resolution;
		return true;
	}

	bool InstancingLabTool::StartShadowBenchmark()
	{
		if (!m_Active || !m_Scene)
		{
			m_ShadowBenchmarkStatus = "Generate an Instancing Lab scene first.";
			return false;
		}

		m_ShadowBenchmarkWarmupFrames = std::clamp(
			m_ShadowBenchmarkWarmupFrames, 4u, 120u);
		m_ShadowBenchmarkSamplesPerConfiguration = std::clamp(
			m_ShadowBenchmarkSamplesPerConfiguration, 5u, 300u);
		m_ShadowBenchmarkResults.clear();
		m_ShadowBenchmarkConfigurationIndex = 0;
		m_ShadowBenchmarkWarmupRemaining = m_ShadowBenchmarkWarmupFrames;
		m_ShadowBenchmarkSamplesCollected = 0;
		m_ShadowBenchmarkSumMilliseconds = 0.0;
		m_ShadowBenchmarkMinimumMilliseconds =
			std::numeric_limits<float>::max();
		m_ShadowBenchmarkMaximumMilliseconds = 0.0f;
		m_LastShadowTimingSample = 0;
		if (!ApplyShadowBenchmarkConfiguration(0))
		{
			m_ShadowBenchmarkState = ShadowBenchmarkState::Idle;
			m_ShadowBenchmarkStatus = "The temporary directional light is unavailable.";
			return false;
		}

		m_ShadowBenchmarkState = ShadowBenchmarkState::Warmup;
		m_ShadowBenchmarkStatus =
			"Benchmark running. Keep the camera and editor window unchanged.";
		return true;
	}

	bool InstancingLabTool::GenerateForShadowBenchmark()
	{
		m_Preset = Preset::MaximumInstancing;
		m_Count = { 50, 1, 50 };
		m_ShadowBenchmarkWarmupFrames = 15;
		m_ShadowBenchmarkSamplesPerConfiguration = 30;
		if (!Generate())
		{
			GL_CORE_ERROR("Shadow Benchmark autorun failed to generate the Instancing Lab: {0}",
				m_Status);
			return false;
		}
		if (!StartShadowBenchmark())
		{
			GL_CORE_ERROR("Shadow Benchmark autorun failed to start: {0}",
				m_ShadowBenchmarkStatus);
			return false;
		}
		GL_CORE_INFO(
			"Shadow Benchmark autorun started: 2500 entities, 15 warmup frames and 30 samples per configuration.");
		return true;
	}

	void InstancingLabTool::CancelShadowBenchmark(const char* status)
	{
		const bool wasRunning = m_ShadowBenchmarkState == ShadowBenchmarkState::Warmup
			|| m_ShadowBenchmarkState == ShadowBenchmarkState::Sampling;
		m_ShadowBenchmarkState = ShadowBenchmarkState::Idle;
		m_ShadowBenchmarkWarmupRemaining = 0;
		m_ShadowBenchmarkSamplesCollected = 0;
		if (status)
			m_ShadowBenchmarkStatus = status;
		else if (wasRunning)
			m_ShadowBenchmarkStatus = "Benchmark cancelled.";
	}

	void InstancingLabTool::FinishShadowBenchmarkConfiguration(
		const ShadowRenderer::Statistics& statistics)
	{
		ShadowBenchmarkResult result;
		const ShadowBenchmarkSetting& configuration =
			ShadowBenchmarkConfigurations[m_ShadowBenchmarkConfigurationIndex];
		result.Configuration = { configuration.Cascades, configuration.Resolution };
		result.Samples = m_ShadowBenchmarkSamplesCollected;
		result.AverageMilliseconds = static_cast<float>(
			m_ShadowBenchmarkSumMilliseconds / m_ShadowBenchmarkSamplesCollected);
		result.MinimumMilliseconds = m_ShadowBenchmarkMinimumMilliseconds;
		result.MaximumMilliseconds = m_ShadowBenchmarkMaximumMilliseconds;
		result.DrawCalls = statistics.DrawCalls;
		result.SavedDrawCalls = statistics.GetSavedDrawCalls();
		m_ShadowBenchmarkResults.push_back(result);

		++m_ShadowBenchmarkConfigurationIndex;
		if (m_ShadowBenchmarkConfigurationIndex >= ShadowBenchmarkConfigurations.size())
		{
			m_ShadowBenchmarkState = ShadowBenchmarkState::Complete;
			m_ShadowBenchmarkStatus =
				"Benchmark complete. Results are runtime-only and were not saved to the scene.";
			GL_CORE_INFO("Shadow Benchmark PASS: {0} configurations completed.",
				m_ShadowBenchmarkResults.size());
			for (const ShadowBenchmarkResult& completed : m_ShadowBenchmarkResults)
			{
				GL_CORE_INFO(
					"Shadow Benchmark Result: cascades={0}, resolution={1}, samples={2}, avg={3:.3f} ms, min={4:.3f} ms, max={5:.3f} ms, draws={6}, saved={7}",
					completed.Configuration.Cascades,
					completed.Configuration.Resolution,
					completed.Samples,
					completed.AverageMilliseconds,
					completed.MinimumMilliseconds,
					completed.MaximumMilliseconds,
					completed.DrawCalls,
					completed.SavedDrawCalls);
			}
			return;
		}

		m_ShadowBenchmarkSamplesCollected = 0;
		m_ShadowBenchmarkSumMilliseconds = 0.0;
		m_ShadowBenchmarkMinimumMilliseconds =
			std::numeric_limits<float>::max();
		m_ShadowBenchmarkMaximumMilliseconds = 0.0f;
		m_ShadowBenchmarkWarmupRemaining = m_ShadowBenchmarkWarmupFrames;
		if (!ApplyShadowBenchmarkConfiguration(m_ShadowBenchmarkConfigurationIndex))
		{
			CancelShadowBenchmark("Benchmark stopped because the directional light disappeared.");
			return;
		}
		m_ShadowBenchmarkState = ShadowBenchmarkState::Warmup;
	}

	void InstancingLabTool::UpdateShadowBenchmark(
		const ShadowRenderer::Statistics& statistics)
	{
		if (m_ShadowBenchmarkState != ShadowBenchmarkState::Warmup
			&& m_ShadowBenchmarkState != ShadowBenchmarkState::Sampling)
			return;
		if (!m_Active || !m_Scene)
		{
			CancelShadowBenchmark("Benchmark stopped because the lab is no longer active.");
			return;
		}

		if (m_ShadowBenchmarkState == ShadowBenchmarkState::Warmup)
		{
			m_LastShadowTimingSample = statistics.GpuTimingSample;
			if (m_ShadowBenchmarkWarmupRemaining > 0)
				--m_ShadowBenchmarkWarmupRemaining;
			if (m_ShadowBenchmarkWarmupRemaining == 0)
				m_ShadowBenchmarkState = ShadowBenchmarkState::Sampling;
			return;
		}

		if (!statistics.GpuTimingAvailable
			|| statistics.GpuTimingSample == m_LastShadowTimingSample)
			return;

		m_LastShadowTimingSample = statistics.GpuTimingSample;
		m_ShadowBenchmarkSumMilliseconds += statistics.GpuMilliseconds;
		m_ShadowBenchmarkMinimumMilliseconds = std::min(
			m_ShadowBenchmarkMinimumMilliseconds, statistics.GpuMilliseconds);
		m_ShadowBenchmarkMaximumMilliseconds = std::max(
			m_ShadowBenchmarkMaximumMilliseconds, statistics.GpuMilliseconds);
		++m_ShadowBenchmarkSamplesCollected;
		if (m_ShadowBenchmarkSamplesCollected
			>= m_ShadowBenchmarkSamplesPerConfiguration)
			FinishShadowBenchmarkConfiguration(statistics);
	}

	void InstancingLabTool::DrawShadowBenchmark()
	{
		ImGui::SeparatorText("Shadow Benchmark");
		ImGui::TextWrapped(
			"Measures 1/2/4 cascades at 1024/2048/4096 using unique asynchronous GPU timer samples. Frame the scene first and keep the camera and window unchanged.");
		const bool running = m_ShadowBenchmarkState == ShadowBenchmarkState::Warmup
			|| m_ShadowBenchmarkState == ShadowBenchmarkState::Sampling;
		int warmupFrames = static_cast<int>(m_ShadowBenchmarkWarmupFrames);
		int samples = static_cast<int>(m_ShadowBenchmarkSamplesPerConfiguration);
		ImGui::BeginDisabled(running);
		if (ImGui::DragInt("Warmup Frames", &warmupFrames, 1.0f, 4, 120))
			m_ShadowBenchmarkWarmupFrames = static_cast<uint32_t>(warmupFrames);
		if (ImGui::DragInt("Samples / Configuration", &samples, 1.0f, 5, 300))
			m_ShadowBenchmarkSamplesPerConfiguration = static_cast<uint32_t>(samples);
		if (ImGui::Button("Start Shadow Benchmark"))
			StartShadowBenchmark();
		ImGui::EndDisabled();
		if (running)
		{
			ImGui::SameLine();
			if (ImGui::Button("Cancel Benchmark"))
				CancelShadowBenchmark();
		}

		ImGui::TextWrapped("%s", m_ShadowBenchmarkStatus.c_str());
		if (running)
		{
			const ShadowBenchmarkSetting& configuration =
				ShadowBenchmarkConfigurations[m_ShadowBenchmarkConfigurationIndex];
			ImGui::Text("Configuration %zu / %zu: %u cascades, %u px",
				m_ShadowBenchmarkConfigurationIndex + 1,
				ShadowBenchmarkConfigurations.size(),
				configuration.Cascades, configuration.Resolution);
			if (m_ShadowBenchmarkState == ShadowBenchmarkState::Warmup)
				ImGui::Text("Warmup remaining: %u", m_ShadowBenchmarkWarmupRemaining);
			else
				ImGui::Text("Samples: %u / %u", m_ShadowBenchmarkSamplesCollected,
					m_ShadowBenchmarkSamplesPerConfiguration);
		}

		if (m_ShadowBenchmarkResults.empty())
			return;
		ImGui::Columns(7, "ShadowBenchmarkResults", true);
		ImGui::TextUnformatted("Cascades"); ImGui::NextColumn();
		ImGui::TextUnformatted("Resolution"); ImGui::NextColumn();
		ImGui::TextUnformatted("Avg ms"); ImGui::NextColumn();
		ImGui::TextUnformatted("Min ms"); ImGui::NextColumn();
		ImGui::TextUnformatted("Max ms"); ImGui::NextColumn();
		ImGui::TextUnformatted("Draws"); ImGui::NextColumn();
		ImGui::TextUnformatted("Saved"); ImGui::NextColumn();
		ImGui::Separator();
		for (const ShadowBenchmarkResult& result : m_ShadowBenchmarkResults)
		{
			ImGui::Text("%u", result.Configuration.Cascades); ImGui::NextColumn();
			ImGui::Text("%u", result.Configuration.Resolution); ImGui::NextColumn();
			ImGui::Text("%.3f", result.AverageMilliseconds); ImGui::NextColumn();
			ImGui::Text("%.3f", result.MinimumMilliseconds); ImGui::NextColumn();
			ImGui::Text("%.3f", result.MaximumMilliseconds); ImGui::NextColumn();
			ImGui::Text("%u", result.DrawCalls); ImGui::NextColumn();
			ImGui::Text("%u", result.SavedDrawCalls); ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}

	void InstancingLabTool::DrawExpectedAndActual(
		const Renderer3D::Statistics& statistics) const
	{
		ImGui::Columns(3, "InstancingComparison", false);
		ImGui::TextUnformatted("Metric"); ImGui::NextColumn();
		ImGui::TextUnformatted("Expected"); ImGui::NextColumn();
		ImGui::TextUnformatted("Actual"); ImGui::NextColumn();
		ImGui::Separator();

		auto row = [](const char* label, uint32_t expected, uint32_t actual) {
			ImGui::TextUnformatted(label); ImGui::NextColumn();
			ImGui::Text("%u", expected); ImGui::NextColumn();
			ImGui::Text("%u", actual); ImGui::NextColumn();
		};
		row("Items", m_Expected.Items, statistics.SubmittedItems);
		row("Draw Calls", m_Expected.DrawCalls, statistics.DrawCalls);
		row("Instanced Draws", m_Expected.InstancedDrawCalls,
			statistics.InstancedDrawCalls);
		row("Individual Draws", m_Expected.IndividualDrawCalls,
			statistics.IndividualDrawCalls);
		row("Instances", m_Expected.InstanceCount, statistics.InstanceCount);
		ImGui::Columns(1);

		const bool currentFrame = statistics.SubmittedItems == m_Expected.Items;
		const bool passed = currentFrame
			&& statistics.RenderedItems == m_Expected.Items
			&& statistics.DrawCalls == m_Expected.DrawCalls
			&& statistics.InstancedDrawCalls == m_Expected.InstancedDrawCalls
			&& statistics.IndividualDrawCalls == m_Expected.IndividualDrawCalls
			&& statistics.InstanceCount == m_Expected.InstanceCount
			&& statistics.SkippedModels == 0;

		const ImVec4 color = !currentFrame
			? ImVec4(0.95f, 0.75f, 0.25f, 1.0f)
			: passed ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f)
			: ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextUnformatted(!currentFrame ? "PENDING" : passed ? "PASS" : "FAIL");
		ImGui::PopStyleColor();
	}

	void InstancingLabTool::OnImGuiRender(
		const Renderer3D::Statistics& statistics)
	{
		ImGui::SeparatorText("GPU Instancing Lab");
		ImGui::TextWrapped(
			"Creates a temporary in-memory ECS scene. It is never serialized and does not enter Undo/Redo history.");

		DrawAssetTarget("Model", AssetType::Model, m_ModelHandle);
		DrawAssetTarget("Material", AssetType::Material, m_MaterialHandle);

		const char* presets[] = {
			"Maximum Instancing",
			"Material Split",
			"Transparent Comparison"
		};
		int preset = static_cast<int>(m_Preset);
		if (ImGui::Combo("Preset", &preset, presets, IM_ARRAYSIZE(presets)))
			m_Preset = static_cast<Preset>(preset);

		ImGui::DragInt3("Count XYZ", m_Count.data(), 1.0f, 1, 1000);
		ImGui::DragFloat3("Spacing", &m_Spacing.x, 0.1f, 0.1f, 100.0f);
		ImGui::DragFloat3("Origin", &m_Origin.x, 0.1f);
		ImGui::Text("Requested entities: %u / %u",
			GetRequestedEntityCount(), MaxLabEntities);

		if (ImGui::Button(m_Active ? "Regenerate" : "Generate"))
			Generate();
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_Active);
		if (ImGui::Button("Exit Lab"))
			Exit();
		ImGui::EndDisabled();

		const ImVec4 statusColor = m_LastOperationSucceeded
			? ImVec4(0.35f, 0.85f, 0.45f, 1.0f)
			: ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
		ImGui::TextWrapped("%s", m_Status.c_str());
		ImGui::PopStyleColor();

		if (!m_Active)
			return;

		ImGui::Text("Generation: %.2f ms", m_LastGenerationMilliseconds);
		ImGui::Text("Entities / submesh items: %u / %u",
			m_Expected.Entities, m_Expected.Items);
		ImGui::Text("Material cache hit / miss: %u / %u",
			statistics.MaterialCacheHits, statistics.MaterialCacheMisses);
		ImGui::Text("Saved Draw Calls: %u", statistics.GetSavedDrawCalls());

		DrawExpectedAndActual(statistics);
		DrawShadowBenchmark();

		ImGui::BeginDisabled(m_RepresentativeEntities.empty());
		if (ImGui::Button("Select First"))
			SelectRepresentative(0);
		ImGui::SameLine();
		if (ImGui::Button("Select Middle"))
			SelectRepresentative(m_RepresentativeEntities.size() / 2);
		ImGui::SameLine();
		if (ImGui::Button("Select Last"))
			SelectRepresentative(m_RepresentativeEntities.size() - 1);
		ImGui::EndDisabled();

		ImGui::TextDisabled(
			"Scene Hierarchy enumeration and scene saving are disabled while the lab is active.");
	}

	void InstancingLabTool::Exit()
	{
		CancelShadowBenchmark();
		if (!m_Active)
			return;
		if (m_ExitScene)
			m_ExitScene();
		m_Scene.reset();
		m_RepresentativeEntities.clear();
		m_SunEntity = UUID(0);
		m_Expected = {};
		m_Active = false;
		m_Status = "Temporary lab exited; the editor scene was restored.";
		m_LastOperationSucceeded = true;
	}

}
