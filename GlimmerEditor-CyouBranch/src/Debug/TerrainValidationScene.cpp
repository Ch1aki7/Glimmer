#include "TerrainValidationScene.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Core/Log.h"
#include "Glimmer/Scene/Components.h"
#include "Glimmer/Scene/Entity.h"
#include "Glimmer/Scene/Scene.h"
#include "Glimmer/Terrain/Terrain.h"
#include "Glimmer/Renderer/TerrainRenderer.h"
#include "Glimmer/Scene/SceneSerializer.h"
#include "../Editor/TerrainRecipeEditor.h"
#include "../Editor/EditorCommand.h"
#include "../Panels/InspectorPanel.h"
#include <imgui_internal.h>
#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace gl {
	// Exercise actual widgets in an isolated ImGui context; diagnostics require the caller's GL context.
	struct TerrainInspectorValidation
	{
		static void RunExecutionModes(const Ref<Scene>& scene, Entity entity)
		{
			auto& terrain = entity.GetComponent<TerrainComponent>();
			const auto original = terrain.Specification;
			auto require = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
			InspectorPanel panel; EditorCommandHistory history; panel.SetContext(scene); panel.SetCommandHistory(&history);
			TerrainComponent after = terrain; after.Specification.ExecutionMode = TerrainExecutionMode::Static;
			panel.ExecuteComponentEdit(entity, "Edit Terrain Execution Mode", terrain, after);
			require(TerrainRenderer::Prepare(terrain) && !terrain.Runtime->GPUHydrology && !terrain.Runtime->GPUClimate
				&& !terrain.Runtime->GPUEnvironment && !terrain.Runtime->Hydrology
				&& terrain.Runtime->HeightMap == terrain.Runtime->Generator->GetHeightMap(), "Static mode allocates simulation or publishes dynamic height.");
			const auto staticVersion = TerrainRenderer::GetSurfaceVersion(terrain); const auto staticMap = terrain.Runtime->HeightMap;
			TerrainSurfaceSnapshot snapshot; require(TerrainRenderer::CaptureSurfaceSnapshot(terrain, snapshot) == TerrainQueryStatus::Ready, "Static snapshot capture failed.");
			TerrainRenderer::RequestHydrologySingleStep(); TerrainRenderer::RequestClimateSingleStep();
			require(TerrainRenderer::Prepare(terrain) && TerrainRenderer::GetSurfaceVersion(terrain) == staticVersion
				&& terrain.Runtime->HeightMap == staticMap && !terrain.Runtime->GPUHydrology, "Static repeated Prepare changes publication or creates simulation.");
			terrain.Specification.ExecutionMode = TerrainExecutionMode::Simulation; terrain.Specification.Recipe.Version = 999;
			TerrainRenderer::Invalidate(terrain);
			require(TerrainRenderer::Prepare(terrain) && TerrainRenderer::GetSurfaceSpecification(terrain).ExecutionMode == TerrainExecutionMode::Static
				&& !terrain.Runtime->GPUHydrology && terrain.Runtime->HeightMap == staticMap, "Failed mode switch discards static publication.");
			terrain.Specification = original; terrain.Specification.ExecutionMode = TerrainExecutionMode::Static;
			require(history.Undo() && TerrainRenderer::Prepare(terrain) && terrain.Runtime->GPUHydrology && terrain.Runtime->GPUClimate
				&& terrain.Runtime->GPUEnvironment && snapshot.Query({ 0, 0 }, TerrainRenderer::GetSurfaceVersion(terrain)).Status == TerrainQueryStatus::StaleVersion,
				"Mode Undo fails to recreate simulation or invalidate old snapshot.");
			const size_t count = size_t(terrain.Runtime->HeightMap->GetWidth()) * terrain.Runtime->HeightMap->GetHeight();
			std::vector<float> initial(count), simulated(count);
			terrain.Runtime->Generator->GetHeightMap()->GetImageData(initial.data(), uint32_t(count * sizeof(float)));
			terrain.Runtime->GPUHydrology->GetHeightTexture()->GetImageData(simulated.data(), uint32_t(count * sizeof(float)));
			require(initial == simulated, "Enabled simulation does not initialize from static height.");
			terrain.Runtime->GPUHydrology->GetHeightTexture()->Clear(glm::vec4(0));
			terrain.Runtime->GPUHydrology->Reset();
			terrain.Runtime->GPUHydrology->GetHeightTexture()->GetImageData(simulated.data(), uint32_t(count * sizeof(float)));
			require(initial == simulated, "Mode switch Reset loses static initial height.");
			const auto simulationMap = terrain.Runtime->HeightMap; const auto simulationVersion = TerrainRenderer::GetSurfaceVersion(terrain);
			terrain.Specification.ExecutionMode = TerrainExecutionMode::Static; terrain.Specification.Recipe.Version = 999; TerrainRenderer::Invalidate(terrain);
			require(TerrainRenderer::Prepare(terrain) && terrain.Runtime->GPUHydrology && terrain.Runtime->HeightMap == simulationMap
				&& TerrainRenderer::GetSurfaceVersion(terrain) == simulationVersion, "Failed static switch discards simulation publication.");
			terrain.Specification = original;
			require(history.Redo() && TerrainRenderer::Prepare(terrain) && !terrain.Runtime->GPUHydrology, "Mode Redo fails to release simulation.");
			const auto copy = Scene::Copy(scene); auto copied = copy->FindEntityByUUID(entity.GetUUID());
			require(copied.GetComponent<TerrainComponent>().Specification.ExecutionMode == TerrainExecutionMode::Static
				&& !copied.GetComponent<TerrainComponent>().Runtime, "Static mode copy shares runtime or loses mode.");
			auto& copiedTerrain = copied.GetComponent<TerrainComponent>();
			require(TerrainRenderer::Prepare(copiedTerrain) && !copiedTerrain.Runtime->GPUHydrology && !copiedTerrain.Runtime->GPUClimate
				&& !copiedTerrain.Runtime->GPUEnvironment && copiedTerrain.Runtime != terrain.Runtime, "Cold static Prepare allocates simulation or shares resources.");
			terrain.Specification = original; TerrainRenderer::Invalidate(terrain); require(TerrainRenderer::Prepare(terrain), "Mode fixture recovery failed.");
			GL_CORE_INFO("Terrain execution modes PASS: static resource absence, repeated Prepare, Undo/Redo, both failed switches, snapshot staleness, simulation initialization/Reset and scene copy isolation.");
		}

		static void RunDiagnostics(const Ref<Scene>& scene, Entity entity)
		{
			struct Guard {
				ImGuiContext* Previous = ImGui::GetCurrentContext();
				ImGuiContext* Test = ImGui::CreateContext();
				~Guard() { ImGui::DestroyContext(Test); ImGui::SetCurrentContext(Previous); }
			} context;
			ImGui::SetCurrentContext(context.Test);
			auto& io = ImGui::GetIO(); io.DisplaySize = { 1100, 1800 }; io.DeltaTime = 1.0f / 60;
			io.IniFilename = nullptr; io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
			InspectorPanel panel; panel.SetContext(scene);
			auto& terrain = entity.GetComponent<TerrainComponent>();
			auto require = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
			auto frame = [&](const char* activate = nullptr) {
				ImGui::NewFrame(); ImGui::SetNextWindowSize({ 1100, 1800 });
				ImGui::Begin("Diagnostic Contract", nullptr, ImGuiWindowFlags_NoSavedSettings);
				if (activate) {
					const auto id = ImGui::GetID(activate); auto& g = *ImGui::GetCurrentContext();
					g.NavWindow = ImGui::GetCurrentWindow(); g.NavId = id; g.NavInputSource = ImGuiInputSource_Keyboard;
					g.NavActivateId = g.NavActivatePressedId = g.NavActivateDownId = id;
				}
				panel.DrawTerrainDiagnostics(entity, terrain); ImGui::End(); ImGui::Render();
			};
			frame(); frame();
			require(panel.m_TerrainSnapshot.GetWidth() == 0 && !panel.m_TerrainClippedCount, "Diagnostics read back without a button.");
			frame("Capture Static Snapshot"); frame("Read Clipping Count");
			const auto version = TerrainRenderer::GetSurfaceVersion(terrain);
			require(panel.m_TerrainCaptureStatus == TerrainQueryStatus::Ready && panel.m_TerrainSnapshot.GetVersion() == version
				&& panel.m_TerrainClippedCount.has_value(), "Diagnostic capture/count widgets failed.");
			panel.m_TerrainQueryXZ = { -96, 0 }; frame();
			require(panel.m_TerrainSnapshot.Query(panel.m_TerrainQueryXZ, version).Status == TerrainQueryStatus::Ready, "Diagnostic query failed.");
			TerrainRenderer::Invalidate(terrain); require(TerrainRenderer::Prepare(terrain), "Diagnostic rebuild failed."); frame();
			require(panel.m_TerrainSnapshot.Query(panel.m_TerrainQueryXZ, TerrainRenderer::GetSurfaceVersion(terrain)).Status == TerrainQueryStatus::StaleVersion
				&& !(panel.m_TerrainClipVersion == TerrainRenderer::GetSurfaceVersion(terrain)), "Diagnostics silently refreshed a stale capture/count.");
			frame("Capture Static Snapshot");
			panel.m_TerrainQueryXZ = { terrain.Specification.WorldSize, 0 }; frame();
			require(panel.m_TerrainSnapshot.Query(panel.m_TerrainQueryXZ, TerrainRenderer::GetSurfaceVersion(terrain)).Status == TerrainQueryStatus::OutOfBounds, "Diagnostic query clamps coordinates.");
			const auto spec = terrain.Specification;
			terrain.Specification.Recipe.Version = 999; TerrainRenderer::Invalidate(terrain);
			TerrainRenderer::Prepare(terrain); frame();
			require(panel.m_TerrainSnapshot.Query({ 0, 0 }, TerrainRenderer::GetSurfaceVersion(terrain)).Status == TerrainQueryStatus::Ready, "Diagnostic failure discarded published snapshot.");
			terrain.Specification = spec; TerrainRenderer::Invalidate(terrain); require(TerrainRenderer::Prepare(terrain), "Diagnostic recovery failed.");
			panel.SetContext(CreateRef<Scene>());
			require(panel.m_TerrainSnapshot.GetWidth() == 0 && !panel.m_TerrainClippedCount, "Diagnostic scene switch retains CPU cache.");
			const auto view = TerrainRenderer::GetAuthoringVisualizationMode();
			const auto published = TerrainRenderer::GetSurfaceVersion(terrain); const auto protection = terrain.Runtime->ProtectionMap;
			for (const auto mode : { TerrainRenderer::AuthoringVisualizationMode::Protection, TerrainRenderer::AuthoringVisualizationMode::Clipping, TerrainRenderer::AuthoringVisualizationMode::None }) {
				TerrainRenderer::SetAuthoringVisualizationMode(mode);
				require(TerrainRenderer::GetAuthoringVisualizationMode() == mode && TerrainRenderer::GetSurfaceVersion(terrain) == published
					&& terrain.Runtime->ProtectionMap == protection, "Diagnostic view mutates published resources.");
			}
			TerrainRenderer::SetAuthoringVisualizationMode(view);
			GL_CORE_INFO("Terrain authoring diagnostics PASS: actual capture/count widgets, no automatic capture, stale rebuild, bounds, failed publication retention, context reset and view isolation.");
		}

		static void Run()
		{
			struct ContextGuard {
				ImGuiContext* Previous = ImGui::GetCurrentContext();
				ImGuiContext* Test = ImGui::CreateContext();
				~ContextGuard() { ImGui::DestroyContext(Test); ImGui::SetCurrentContext(Previous); }
			} context;
			ImGui::SetCurrentContext(context.Test);
			auto& io = ImGui::GetIO();
			io.DisplaySize = { 1100, 1800 }; io.DeltaTime = 1.0f / 60;
			io.IniFilename = nullptr; io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			unsigned char* pixels; int width, height;
			io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
			const auto scene = CreateRef<Scene>();
			Entity entity = scene->CreateEntity("Terrain Inspector Contract");
			auto& terrain = entity.AddComponent<TerrainComponent>();
			EditorCommandHistory history;
			InspectorPanel panel; panel.SetContext(scene); panel.SetCommandHistory(&history);
			auto require = [](bool condition, const char* message) { if (!condition) throw std::runtime_error(message); };
			auto frame = [&](const char* activate = nullptr, uint64_t stampID = 0) {
				ImGui::NewFrame();
				ImGui::SetNextWindowSize({ 1100, 1800 });
				ImGui::Begin("Inspector Contract", nullptr, ImGuiWindowFlags_NoSavedSettings);
				if (activate)
				{
					if (stampID) { ImGui::PushID(int(stampID >> 32)); ImGui::PushID(int(stampID & 0xffffffffu)); ImGui::PushID("Stamp"); }
					const ImGuiID id = ImGui::GetID(activate);
					if (stampID) { ImGui::PopID(); ImGui::PopID(); ImGui::PopID(); }
					auto& g = *ImGui::GetCurrentContext();
					g.NavWindow = ImGui::GetCurrentWindow(); g.NavId = id; g.NavInputSource = ImGuiInputSource_Keyboard;
					g.NavActivateId = id; g.NavActivatePressedId = id; g.NavActivateDownId = id;
				}
				panel.DrawTerrainRecipe(entity, terrain);
				ImGui::End(); ImGui::Render();
			};
			frame(); frame();
			frame("Add Rectangle");
			require(terrain.Specification.Recipe.Stamps.size() == 1, "Inspector Add Rectangle widget failed.");
			frame("Add Ellipse");
			require(terrain.Specification.Recipe.Stamps.size() == 2 && history.Undo()
				&& terrain.Specification.Recipe.Stamps.size() == 1 && history.Redo(), "Inspector Add Undo/Redo failed.");
			const auto first = terrain.Specification.Recipe.Stamps[0].ID;
			frame("Enabled", first);
			require(!terrain.Specification.Recipe.Stamps[0].Enabled && history.Undo()
				&& terrain.Specification.Recipe.Stamps[0].Enabled, "Inspector enable command failed.");
			frame("Down", first);
			require(terrain.Specification.Recipe.Stamps[1].ID == first && history.Undo(), "Inspector reorder widget failed.");
			frame("Remove", first);
			require(terrain.Specification.Recipe.Stamps.size() == 1 && history.Undo(), "Inspector remove widget failed.");
			frame("Clear Stamps");
			require(terrain.Specification.Recipe.Stamps.empty() && history.Undo(), "Inspector clear widget failed.");
			const float initialHeight = terrain.Specification.Recipe.Stamps[0].Height;
			frame("Target Height", first);
			io.AddKeyEvent(ImGuiKey_RightArrow, true); frame();
			io.AddKeyEvent(ImGuiKey_RightArrow, false); frame();
			frame("Target Height", first); frame();
			require(terrain.Specification.Recipe.Stamps[0].Height != initialHeight
				&& std::string(history.GetUndoName()) == "Edit Terrain Stamp Height" && history.Undo()
				&& terrain.Specification.Recipe.Stamps[0].Height == initialHeight && history.Redo(),
				"Inspector continuous widget does not commit one reversible edit.");
			GL_CORE_INFO("Terrain Inspector widgets PASS: Add, enable, reorder, remove, clear and continuous height activation/release Undo/Redo.");
		}
	};

	static constexpr uint64_t TerrainFixtureID = 0x5445525241494eULL;
	bool ValidateTerrainRecipeEditorIntegration(const Ref<Scene>& scene)
	{
		Entity entity = scene->FindEntityByUUID(UUID(TerrainFixtureID));
		if (!entity) return false;
		auto& terrain = entity.GetComponent<TerrainComponent>();
		const auto original = terrain.Specification;
		const auto temporaryScenePath = std::filesystem::temp_directory_path()
			/ ("Glimmer-Recipe-Integration-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".glimmer");
		bool passed = false;
		try
		{
			TerrainInspectorValidation::Run();
			auto require = [](bool condition, const char* message) { if (!condition) throw std::runtime_error(message); };
			terrain.Specification.HeightMapResolution = 129;
			terrain.Specification.MeshResolution = 96;
			TerrainRenderer::Invalidate(terrain);
			require(TerrainRenderer::Prepare(terrain), "Initial recipe Prepare failed.");
			TerrainInspectorValidation::RunDiagnostics(scene, entity);
			TerrainInspectorValidation::RunExecutionModes(scene, entity);
			auto hash = [&]() { const auto result = terrain.Runtime->Generator->ValidateOutputs(); require(result.Valid, "Invalid composed maps."); return result.Hash; };
			auto protectionValues = [&](const TerrainComponent& target) {
				const auto map = target.Runtime->ProtectionMap;
				require(bool(map), "Expected published protection map.");
				std::vector<float> values(size_t(map->GetWidth()) * map->GetHeight());
				map->GetImageData(values.data(), uint32_t(values.size() * sizeof(float))); return values;
			};
			const auto baselineProtection = protectionValues(terrain);
			auto initialAndReset = [&]() {
				const size_t count = size_t(terrain.Runtime->HeightMap->GetWidth()) * terrain.Runtime->HeightMap->GetHeight();
				std::vector<float> height(count), simulated(count);
				terrain.Runtime->Generator->GetHeightMap()->GetImageData(height.data(), uint32_t(count * sizeof(float)));
				terrain.Runtime->GPUHydrology->GetHeightTexture()->GetImageData(simulated.data(), uint32_t(count * sizeof(float)));
				require(height == simulated, "Prepare simulation initial surface differs.");
				std::fill(simulated.begin(), simulated.end(), 0.0f);
				terrain.Runtime->GPUHydrology->GetHeightTexture()->SetData(simulated.data(), uint32_t(count * sizeof(float)));
				terrain.Runtime->GPUHydrology->Reset();
				terrain.Runtime->GPUHydrology->GetHeightTexture()->GetImageData(simulated.data(), uint32_t(count * sizeof(float)));
				require(height == simulated, "Reset simulation surface differs.");
			};
			const auto baselineHash = hash();
			using QueryStatus = TerrainQueryStatus;
			TerrainSurfaceSnapshot baselineSnapshot;
			require(TerrainRenderer::CaptureSurfaceSnapshot(terrain, baselineSnapshot) == QueryStatus::Ready,
				"Explicit static snapshot capture failed.");
			// The ellipse transition overlaps the origin; choose an unaffected platform core cell.
			const auto baselineSample = baselineSnapshot.Query({ -96, 0 }, TerrainRenderer::GetSurfaceVersion(terrain));
			GL_CORE_INFO("Terrain snapshot platform diagnostic: height={0}, target={1}, slope={2}, status={3}",
				baselineSample.Height, terrain.Specification.Recipe.Stamps[0].Height, baselineSample.SlopeDegrees, int(baselineSample.Status));
			require(baselineSample.Status == QueryStatus::Ready
				&& std::abs(baselineSample.Height - terrain.Specification.Recipe.Stamps[0].Height) < 1e-3f
				&& baselineSample.SlopeDegrees < 1e-3f, "Snapshot platform query differs from composed GPU height.");
			EditorCommandHistory history;
			auto apply = [&](const TerrainComponent& value) { terrain.Specification = value.Specification; TerrainRenderer::Invalidate(terrain); return true; };
			TerrainComponent before = terrain, after = terrain;
			require(TerrainRecipeEditor::Add(after.Specification, TerrainStampShape::Ellipse, TerrainStampOperation::Add), "Editor Add rejected valid recipe.");
			require(history.Execute(std::make_unique<ValueEditorCommand<TerrainComponent>>("Add Terrain Stamp", before, after, apply))
				&& TerrainRenderer::Prepare(terrain), "Add command Prepare failed.");
			const auto addedHash = hash();
			require(baselineSnapshot.Query({ 0, 0 }, TerrainRenderer::GetSurfaceVersion(terrain)).Status == QueryStatus::StaleVersion,
				"A rebuilt terrain accepts an old snapshot.");
			require(addedHash != baselineHash, "Added stamp did not change height."); initialAndReset();
			require(history.Undo() && TerrainRenderer::Prepare(terrain) && hash() == baselineHash, "Undo fails to rebuild original surface.");
			require(protectionValues(terrain) == baselineProtection, "Undo protection differs from original surface.");
			require(history.Redo() && TerrainRenderer::Prepare(terrain) && hash() == addedHash, "Redo fails to rebuild added surface.");
			EditorValueTransaction<TerrainComponent> drag;
			drag.Begin(terrain);
			terrain.Specification.Recipe.Stamps.back().Center.x = 10;
			terrain.Specification.Recipe.Stamps.back().Center.x = 30;
			terrain.Specification.Recipe.Stamps.back().Height = -25;
			TerrainRenderer::Invalidate(terrain);
			require(TerrainRenderer::Prepare(terrain), "Continuous edit Prepare failed.");
			history.PushExecuted(std::make_unique<ValueEditorCommand<TerrainComponent>>("Drag Terrain Stamp", drag.GetBefore(), terrain, apply));
			drag.Reset();
			const auto draggedHash = hash();
			require(history.Undo() && TerrainRenderer::Prepare(terrain) && hash() == addedHash, "One drag Undo does not restore full recipe.");
			require(history.Redo() && TerrainRenderer::Prepare(terrain) && hash() == draggedHash, "Drag Redo differs.");
			before = terrain; after = terrain;
			const auto movedID = after.Specification.Recipe.Stamps.back().ID;
			require(TerrainRecipeEditor::Move(after.Specification.Recipe, after.Specification.Recipe.Stamps.size() - 1, -1), "Editor Move failed.");
			require(history.Execute(std::make_unique<ValueEditorCommand<TerrainComponent>>("Reorder Terrain Stamps", before, after, apply))
				&& TerrainRenderer::Prepare(terrain), "Reorder Prepare failed.");
			require(terrain.Specification.Recipe.Stamps[terrain.Specification.Recipe.Stamps.size() - 2].ID == movedID, "Reorder changes stable ID.");
			const auto persistedHash = hash();
			const auto persistedProtection = protectionValues(terrain);
			require(SceneSerializer(scene).Serialize(temporaryScenePath.string()), "Integration scene save failed.");
			const auto restoredScene = CreateRef<Scene>();
			require(SceneSerializer(restoredScene).Deserialize(temporaryScenePath.string()), "Integration scene reload failed.");
			auto& restored = restoredScene->FindEntityByUUID(entity.GetUUID()).GetComponent<TerrainComponent>();
			require(!restored.Runtime && TerrainRenderer::Prepare(restored) && restored.Runtime != terrain.Runtime
				&& restored.Runtime->Generator->ValidateOutputs().Hash == persistedHash, "Reload surface or Runtime isolation differs.");
			require(protectionValues(restored) == persistedProtection, "Reload protection differs from saved recipe.");
			const auto playScene = Scene::Copy(scene);
			auto& playTerrain = playScene->FindEntityByUUID(entity.GetUUID()).GetComponent<TerrainComponent>();
			require(!playTerrain.Runtime && TerrainRenderer::Prepare(playTerrain)
				&& playTerrain.Runtime->Generator->ValidateOutputs().Hash == persistedHash, "Play copy regeneration differs.");
			require(protectionValues(playTerrain) == persistedProtection, "Play protection differs from saved recipe.");
			TerrainSurfaceSnapshot persistedSnapshot;
			require(TerrainRenderer::CaptureSurfaceSnapshot(terrain, persistedSnapshot) == QueryStatus::Ready
				&& persistedSnapshot.Query({ 0, 0 }, TerrainRenderer::GetSurfaceVersion(restored)).Status == QueryStatus::StaleVersion
				&& persistedSnapshot.Query({ 0, 0 }, TerrainRenderer::GetSurfaceVersion(playTerrain)).Status == QueryStatus::StaleVersion,
				"Reload or Play copy reuses another Runtime snapshot identity.");
			terrain.Specification.HeightMapResolution = 145;
			terrain.Specification.WorldSize = 1536;
			TerrainRenderer::Invalidate(terrain);
			require(TerrainRenderer::Prepare(terrain) && terrain.Runtime->HeightMap->GetWidth() == 145
				&& TerrainRenderer::GetSurfaceSpecification(terrain).WorldSize == 1536, "Resize publication failed."); initialAndReset();
			const auto published = terrain.Specification;
			TerrainSurfaceSnapshot resizedSnapshot;
			require(TerrainRenderer::CaptureSurfaceSnapshot(terrain, resizedSnapshot) == QueryStatus::Ready
				&& resizedSnapshot.GetWidth() == 145 && resizedSnapshot.GetWorldSize() == 1536,
				"Resize snapshot dimensions or published range differ.");
			std::vector<float> queryHeight(145 * 145);
			const auto resizedProtection = protectionValues(terrain);
			terrain.Runtime->Generator->GetHeightMap()->GetImageData(queryHeight.data(), uint32_t(queryHeight.size() * sizeof(float)));
			float maxQueryDifference = 0;
			for (uint32_t z = 0; z < 145; ++z) for (uint32_t x = 0; x < 145; ++x)
			{
				const auto sample = resizedSnapshot.Query({ float(-768.0 + x * (1536.0 / 144)), float(-768.0 + z * (1536.0 / 144)) },
					TerrainRenderer::GetSurfaceVersion(terrain));
				require(sample.Status == QueryStatus::Ready && std::isfinite(sample.SlopeDegrees) && sample.SlopeDegrees <= 90
					&& std::abs(glm::length(sample.Normal) - 1) < 1e-5f, "Snapshot query normal/slope is invalid.");
				maxQueryDifference = std::max(maxQueryDifference, std::abs(sample.Height / published.HeightScale - queryHeight[size_t(z) * 145 + x]));
				const auto protection = EvaluateTerrainRecipe(published.Recipe, { float(-768.0 + x * (1536.0 / 144)), float(-768.0 + z * (1536.0 / 144)) }, 0, published.HeightScale);
				require(protection.Validation.Valid() && std::abs(protection.ProtectionWeight - resizedProtection[size_t(z) * 145 + x]) < 1e-5f,
					"Resized GPU protection differs from CPU authoring coordinates.");
			}
			require(maxQueryDifference <= 1e-5f, "Snapshot endpoints differ from GPU static height.");
			// Mutating simulation must not change the static authoring snapshot source.
			terrain.Runtime->GPUHydrology->GetHeightTexture()->Clear(glm::vec4(0));
			TerrainSurfaceSnapshot whileSimulated;
			require(TerrainRenderer::CaptureSurfaceSnapshot(terrain, whileSimulated) == QueryStatus::Ready
				&& whileSimulated.Query({ 0, 0 }, TerrainRenderer::GetSurfaceVersion(terrain)).Height
				== resizedSnapshot.Query({ 0, 0 }, TerrainRenderer::GetSurfaceVersion(terrain)).Height,
				"Static snapshot accidentally reads simulated Runtime height.");
			terrain.Runtime->GPUHydrology->Reset();
			const auto oldHeight = terrain.Runtime->HeightMap, oldNormal = terrain.Runtime->NormalSlopeMap,
				oldAnalysis = terrain.Runtime->AnalysisMap, oldWeights = terrain.Runtime->MaterialWeightMap;
			const auto oldMesh = terrain.Runtime->Mesh;
			const auto oldProtection = terrain.Runtime->ProtectionMap;
			require(oldProtection && oldProtection->GetWidth() == 145 && oldProtection == terrain.Runtime->Generator->GetProtectionMap(),
				"Resize does not publish matching static protection.");
			const auto oldHydrology = terrain.Runtime->GPUHydrology.get();
			const auto oldVersion = terrain.Runtime->GenerationVersion;
			for (int failure = 0; failure < 4; ++failure)
			{
				terrain.Specification = published;
				terrain.Specification.HeightMapResolution = 161;
				terrain.Specification.WorldSize = 2048;
				if (failure == 0) terrain.Specification.Recipe.Version = 999;
				if (failure == 1) terrain.Specification.HeightScale = 0;
				if (failure == 2) terrain.Specification.GenerationShaderHandle = AssetHandle(0);
				if (failure == 3) { terrain.Specification.Procedural = false; terrain.Specification.Recipe.Stamps.clear(); terrain.Specification.HeightMapHandle = AssetHandle(0); }
				TerrainRenderer::Invalidate(terrain);
				require(TerrainRenderer::Prepare(terrain) && !terrain.Runtime->GenerationError.empty()
					&& terrain.Runtime->HeightMap == oldHeight && terrain.Runtime->NormalSlopeMap == oldNormal
					&& terrain.Runtime->AnalysisMap == oldAnalysis && terrain.Runtime->MaterialWeightMap == oldWeights
					&& terrain.Runtime->ProtectionMap == oldProtection
					&& terrain.Runtime->Mesh == oldMesh && terrain.Runtime->GPUHydrology.get() == oldHydrology
					&& terrain.Runtime->GenerationVersion == oldVersion
					&& TerrainRenderer::GetSurfaceSpecification(terrain).WorldSize == 1536, "Failed edit changes published surface/state.");
				require(resizedSnapshot.Query({ 0, 0 }, TerrainRenderer::GetSurfaceVersion(terrain)).Status == QueryStatus::Ready,
					"Rejected edit invalidates a snapshot of the retained publication.");
				require(protectionValues(terrain) == resizedProtection, "Rejected edit mutates retained protection bytes.");
			}
			TerrainComponent firstFailure;
			firstFailure.Specification = published;
			firstFailure.Specification.GenerationShaderHandle = AssetHandle(0);
			require(!TerrainRenderer::Prepare(firstFailure) && !firstFailure.Runtime->HeightMap, "First failure draws uninitialized surface.");
			require(TerrainRenderer::CaptureSurfaceSnapshot(firstFailure, whileSimulated) == QueryStatus::NotReady,
				"Failed first publication returns a query snapshot.");
			terrain.Specification = published; TerrainRenderer::Invalidate(terrain);
			require(TerrainRenderer::Prepare(terrain) && terrain.Runtime->GenerationError.empty(), "Generation does not recover.");
			terrain.Specification.Procedural = false;
			terrain.Specification.Recipe.Stamps.clear();
			terrain.Specification.HeightMapHandle = AssetManager::ImportAsset("assets/textures/NoiseTex.png");
			TerrainRenderer::Invalidate(terrain);
			require(TerrainRenderer::Prepare(terrain) && !terrain.Runtime->GPUHydrology && !terrain.Runtime->GPUClimate
				&& !terrain.Runtime->GPUEnvironment && !terrain.Runtime->NormalSlopeMap
				&& !terrain.Runtime->ProtectionMap
				&& !TerrainRenderer::GetSurfaceSpecification(terrain).Procedural, "Successful imported source retains old procedural resources.");
			require(TerrainRenderer::CaptureSurfaceSnapshot(terrain, whileSimulated) == QueryStatus::UnsupportedSurface
				&& resizedSnapshot.Query({ 0, 0 }, TerrainRenderer::GetSurfaceVersion(terrain)).Status == QueryStatus::StaleVersion,
				"Imported surface silently reuses a static endpoint snapshot.");
			terrain.Specification = published; TerrainRenderer::Invalidate(terrain);
			require(TerrainRenderer::Prepare(terrain) && terrain.Runtime->GPUHydrology, "Procedural source cannot recover after import."); initialAndReset();
			terrain.Specification.DataVersion = 1; terrain.Specification.Recipe.Stamps.clear(); TerrainRenderer::Invalidate(terrain);
			require(TerrainRenderer::Prepare(terrain)
				&& TerrainRenderer::CaptureSurfaceSnapshot(terrain, whileSimulated) == QueryStatus::UnsupportedSurface,
				"Legacy cell-centered data is silently queried as endpoint nodes.");
			passed = true;
			GL_CORE_INFO("Terrain surface snapshot PASS: static platform, GPU nodes, version/identity, Resize, rejection retention, import and simulation isolation; max normalized query difference={0}", maxQueryDifference);
			GL_CORE_INFO("Terrain recipe editor integration PASS: Add/drag/reorder Undo/Redo, save/reload, Play isolation, Resize, failed edits/first failure, procedural/imported recovery and simulation initial/Reset.");
		}
		catch (const std::exception& error) { GL_CORE_ERROR("Terrain recipe editor integration FAIL: {0}", error.what()); }
		std::error_code ignored; std::filesystem::remove(temporaryScenePath, ignored);
		terrain.Specification = original; TerrainRenderer::Invalidate(terrain);
		return passed;
	}

	void SeedTerrainValidationWater(const Ref<Scene>& scene)
	{
		Entity entity = scene->FindEntityByUUID(UUID(TerrainFixtureID));
		if (entity)
		{
			auto& terrain = entity.GetComponent<TerrainComponent>();
			if (!TerrainRenderer::Prepare(terrain) || !terrain.Runtime->GPUHydrology) return;
			const auto& height = terrain.Runtime->HeightMap;
			std::vector<float> water(size_t(height->GetWidth()) * height->GetHeight());
			height->GetImageData(water.data(), uint32_t(water.size() * sizeof(float)));
			for (auto& value : water) value = std::max(0.0f, terrain.Specification.HeightScale * (0.42f - value));
			terrain.Runtime->GPUHydrology->GetWaterTexture()->SetData(water.data(), uint32_t(water.size() * sizeof(float)));
			GL_CORE_INFO("Terrain recipe water fixture seeded from composed height; simulation remains paused.");
		}
	}

	Ref<Scene> CreateTerrainValidationScene(
		AssetHandle skyboxHandle,
		AssetHandle renderShaderHandle,
		AssetHandle generationShaderHandle,
		AssetHandle erosionShaderHandle,
		AssetHandle derivationShaderHandle,
		uint32_t synthesisVersion, uint32_t dataVersion)
	{
		auto scene = CreateRef<Scene>();
		auto sunEntity = scene->CreateEntity("Sun");
		sunEntity.AddComponent<DirectionalLightComponent>();
		sunEntity.GetComponent<TransformComponent>().Rotation =
			{ -50.0f, 30.0f, 0.0f };

		auto pointLightEntity = scene->CreateEntity("Point Light");
		auto& pointLight = pointLightEntity.AddComponent<PointLightComponent>();
		pointLight.Intensity = 80.0f;
		pointLight.Range = 40.0f;
		pointLightEntity.GetComponent<TransformComponent>().Translation =
			{ 0.0f, 12.0f, 0.0f };

		scene->CreateEntity("Sky Light")
			.AddComponent<SkyLightComponent>(skyboxHandle);

		auto terrainEntity = scene->CreateEntityWithUUID(UUID(TerrainFixtureID), "Terrain");
		auto& terrain = terrainEntity.AddComponent<TerrainComponent>();
		// This verification fixture preserves the legacy simulation baseline.
		terrain.Specification.ExecutionMode = TerrainExecutionMode::Simulation;
		char* modeValue = nullptr; size_t modeLength = 0;
		if (_dupenv_s(&modeValue, &modeLength, "GLIMMER_TERRAIN_EXECUTION_MODE") == 0
			&& modeValue && std::string(modeValue) == "static") terrain.Specification.ExecutionMode = TerrainExecutionMode::Static;
		std::free(modeValue);
		ApplyTerrainPreset(terrain.Specification, TerrainPreset::Alpine);
		terrain.Specification.DataVersion = std::clamp(dataVersion, 1u, 2u);
		terrain.Specification.Noise.SynthesisVersion =
			std::clamp(synthesisVersion, 1u, 2u);
		terrain.Specification.RenderShaderHandle = renderShaderHandle;
		terrain.Specification.GenerationShaderHandle = generationShaderHandle;
		terrain.Specification.ErosionShaderHandle = erosionShaderHandle;
		terrain.Specification.DerivationShaderHandle = derivationShaderHandle;
		terrain.Specification.TerrainMaterialHandle = AssetManager::ImportAsset(
			"assets/materials/DefaultTerrain.glterrainmat");
		char* recipeFixture = nullptr;
		size_t recipeFixtureLength = 0;
		const bool enableRecipeFixture = _dupenv_s(&recipeFixture, &recipeFixtureLength,
			"GLIMMER_TERRAIN_RECIPE_FIXTURE") == 0 && recipeFixture && std::string(recipeFixture) == "1";
		std::free(recipeFixture);
		if (enableRecipeFixture)
		{
			terrain.Specification.DataVersion = 2;
			TerrainStamp platform;
			platform.ID = 1;
			platform.Shape = TerrainStampShape::Rectangle;
			platform.Size = { 256, 160 };
			platform.Height = terrain.Specification.HeightScale * 0.4f;
			platform.TransitionWidth = 64;
			platform.RotationDegrees = 23;
			TerrainStamp basin = platform;
			basin.ID = 2;
			basin.Shape = TerrainStampShape::Ellipse;
			basin.Operation = TerrainStampOperation::Add;
			basin.Center = { 180, -70 };
			basin.Size = { 200, 80 };
			basin.Height = -16;
			terrain.Specification.Recipe.Stamps = { platform, basin };
			if (const char* clip = std::getenv("GLIMMER_TERRAIN_RECIPE_CLIP_FIXTURE"); clip && std::string(clip) == "1") {
				TerrainStamp clipped = platform;
				clipped.ID = 3; clipped.Operation = TerrainStampOperation::Add;
				clipped.Center = { -80, 0 }; clipped.Size = { 80, 80 }; clipped.TransitionWidth = 16; clipped.Height = -1000;
				terrain.Specification.Recipe.Stamps.push_back(clipped);
			}
		}
		return scene;
	}

}
