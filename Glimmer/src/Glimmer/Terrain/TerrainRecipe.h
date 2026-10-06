#pragma once

#include <glm/glm.hpp>
#include <cstddef>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace gl {

	enum class TerrainStampShape : uint32_t { Ellipse, Rectangle };
	enum class TerrainStampOperation : uint32_t { Add, SetHeight };

	// All positions, dimensions and heights use Terrain local units.
	struct TerrainStamp
	{
		uint64_t ID = 0;
		bool Enabled = true;
		TerrainStampShape Shape = TerrainStampShape::Ellipse;
		TerrainStampOperation Operation = TerrainStampOperation::SetHeight;
		glm::vec2 Center{ 0.0f };
		glm::vec2 Size{ 64.0f }; // Full core width/depth, excluding the transition.
		float RotationDegrees = 0.0f;
		float TransitionWidth = 8.0f; // Outside the core; zero means a hard edge.
		float Strength = 1.0f;
		float Height = 0.0f; // Add delta or absolute SetHeight target.
	};

	struct TerrainRecipe
	{
		static constexpr uint32_t CurrentVersion = 1;
		static constexpr size_t MaximumStamps = 64;
		uint32_t Version = CurrentVersion;
		std::vector<TerrainStamp> Stamps; // Stored order is evaluation order.
	};

	enum class TerrainRecipeError
	{
		None, UnsupportedVersion, TooManyStamps, UnsupportedTerrain,
		InvalidHeightScale, InvalidID, DuplicateID, InvalidShape,
		InvalidOperation, NonFiniteParameter, InvalidSize,
		InvalidTransition, InvalidStrength, InvalidSample
	};

	struct TerrainRecipeValidationResult
	{
		TerrainRecipeError Error = TerrainRecipeError::None;
		size_t StampIndex = std::numeric_limits<size_t>::max();
		std::string Message;
		bool Valid() const { return Error == TerrainRecipeError::None; }
	};

	struct TerrainRecipeEvaluationResult
	{
		TerrainRecipeValidationResult Validation;
		float NormalizedHeight = 0.0f;
		bool Clipped = false;
	};

	TerrainRecipeValidationResult ValidateTerrainRecipe(const TerrainRecipe& recipe,
		float heightScale, uint32_t dataVersion = 2, bool procedural = true);

	// CPU reference only: no scene, editor, GPU or simulation dependencies.
	TerrainRecipeEvaluationResult EvaluateTerrainRecipe(const TerrainRecipe& recipe,
		glm::vec2 localXZ, float baseNormalizedHeight, float heightScale);

}
