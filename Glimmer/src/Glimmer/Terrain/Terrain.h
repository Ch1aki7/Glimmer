#pragma once

#include "Glimmer/Core/Core.h"
#include "Glimmer/Core/UUID.h"
#include "Glimmer/Renderer/TerrainMesh.h"
#include "Glimmer/Terrain/TerrainGenerator.h"
#include "Glimmer/Simulation/TerrainHydrologyRuntime.h"
#include "Glimmer/Simulation/TerrainHydrologyGPU.h"
#include "Glimmer/Simulation/TerrainClimateGPU.h"
#include "Glimmer/Simulation/TerrainEnvironmentGPU.h"

#include <array>
#include <vector>
#include <unordered_set>

namespace gl {
	struct TerrainResourceUsage
	{
		uint64_t GeneratorBytes = 0, PendingGeneratorBytes = 0, HydrologyBytes = 0, ClimateBytes = 0;
		uint64_t OtherReferencedTextureBytes = 0, MeshBytes = 0, InitialHeightCPUBytes = 0;
		uint32_t TextureCount = 0;
		uint64_t TextureBytes() const { return GeneratorBytes + PendingGeneratorBytes + HydrologyBytes + ClimateBytes + OtherReferencedTextureBytes; }
	};

	struct TerrainPreparationStatistics
	{
		TextureAllocationStatistics Textures;
		uint64_t LifetimePeakTextureBytes = 0, TotalTextureAllocations = 0, PrepareCount = 0;
		double LastCPUGenerationMilliseconds = 0;
		double CPUPrepareMilliseconds = 0, CPUGenerationMilliseconds = 0;
		double CPUEnvironmentMilliseconds = 0, CPUDerivedMilliseconds = 0;
	};

	struct TerrainRuntime
	{
		uint64_t SurfaceIdentity = UUID(); // New Runtime (including scene copies) has a distinct query identity.
		Scope<TerrainGenerator> Generator;
		Scope<TerrainGenerator> PendingGenerator;
		TerrainSpecification PendingSpecification;
		TerrainSpecification PublishedSpecification;
		bool HasPublishedSpecification = false;
		std::string GenerationError;
		Scope<TerrainHydrologyRuntime> Hydrology;
		Scope<TerrainHydrologyGPU> GPUHydrology;
		Scope<TerrainClimateGPU> GPUClimate;
		Scope<TerrainEnvironmentGPU> GPUEnvironment;
		uint64_t HydrologyResetRequest = 0;
		uint64_t HydrologySingleStepRequest = 0;
		uint64_t HydrologySedimentSeedRequest = 0;
		uint64_t HydrologyGenerationVersion = 0;
		uint64_t HydrologyFrameSerial = 0;
		uint64_t ClimateResetRequest = 0;
		uint64_t ClimateSingleStepRequest = 0;
		uint64_t ClimateGenerationVersion = 0;
		uint64_t ClimateFrameSerial = 0;
		Ref<TerrainMesh> Mesh;
		std::array<Ref<TerrainMesh>, 3> LODMeshes;
		std::vector<uint32_t> ChunkLODLevels;
		bool HasChunkLODHistory = false;
		Ref<Texture2D> HeightMap;
		Ref<Texture2D> NormalSlopeMap;
		Ref<Texture2D> AnalysisMap;
		Ref<Texture2D> MaterialWeightMap;
		Ref<Texture2D> ProtectionMap; // Static recipe output; simulation does not own or update it.
		AssetHandle LoadedHeightMapHandle{ 0 };
		AssetHandle LoadedGenerationShaderHandle{ 0 };
		AssetHandle LoadedErosionShaderHandle{ 0 };
		AssetHandle LoadedDerivationShaderHandle{ 0 };
		uint32_t LoadedHeightMapResolution = 0;
		uint32_t LoadedMeshResolution = 0;
		uint32_t LastGenerationDispatchCount = 0;
		uint64_t GenerationVersion = 0;
		uint64_t PreparedFrameSerial = 0; // Pin one complete publication/update across frame passes.
		bool ValidationComplete = false;
		bool RecipeValidationComplete = false;
		bool Dirty = true;
		TerrainPreparationStatistics Preparation; // Diagnostics only; never serialized or copied with specifications.

		// Read-only census of actual references. Aliases and Ping-Pong reads are counted once.
		// OtherReferencedTextureBytes includes shared imported/retained source assets, not exclusive ownership.
		TerrainResourceUsage GetResourceUsage() const
		{
			TerrainResourceUsage usage; std::unordered_set<uint32_t> textures;
			auto add = [&](const std::vector<Ref<Texture2D>>& resources, uint64_t& category) {
				for (const auto& texture : resources)
					if (texture && texture->GetRendererID() && textures.insert(texture->GetRendererID()).second) {
						category += texture->GetStorageByteSize(); ++usage.TextureCount;
					}
			};
			if (Generator) add(Generator->GetTextureResources(), usage.GeneratorBytes);
			if (PendingGenerator) add(PendingGenerator->GetTextureResources(), usage.PendingGeneratorBytes);
			if (GPUHydrology) {
				add(GPUHydrology->GetTextureResources(), usage.HydrologyBytes);
				usage.InitialHeightCPUBytes = GPUHydrology->GetInitialHeightCPUBytes();
			}
			if (GPUClimate) add(GPUClimate->GetTextureResources(), usage.ClimateBytes);
			add({ HeightMap, NormalSlopeMap, AnalysisMap, MaterialWeightMap, ProtectionMap,
				GPUHydrology ? GPUHydrology->GetInitialHeightTexture() : nullptr }, usage.OtherReferencedTextureBytes);
			std::unordered_set<const TerrainMesh*> meshes;
			auto addMesh = [&](const Ref<TerrainMesh>& mesh) {
				if (mesh && meshes.insert(mesh.get()).second) {
					const uint64_t side = uint64_t(mesh->GetGridSize()) + 1;
					usage.MeshBytes += (side * side + 4 * side) * 6 * sizeof(float) + uint64_t(mesh->GetIndexCount()) * sizeof(uint32_t);
				}
			};
			for (const auto& mesh : LODMeshes) addMesh(mesh); addMesh(Mesh);
			return usage;
		}
	};
}
