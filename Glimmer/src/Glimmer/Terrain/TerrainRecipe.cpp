#include "glpch.h"
#include "TerrainRecipe.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace gl {

	TerrainRecipeValidationResult ValidateTerrainRecipe(const TerrainRecipe& recipe,
		float heightScale, uint32_t dataVersion, bool procedural)
	{
		using Error = TerrainRecipeError;
		auto fail = [](Error error, const char* message,
			size_t index = std::numeric_limits<size_t>::max()) {
			return TerrainRecipeValidationResult{ error, index, message };
		};
		if (recipe.Version != TerrainRecipe::CurrentVersion)
			return fail(Error::UnsupportedVersion, "Unsupported Terrain Recipe version.");
		if (recipe.Stamps.size() > TerrainRecipe::MaximumStamps)
			return fail(Error::TooManyStamps, "Terrain Recipe exceeds 64 stamps.");
		// Empty recipes leave the legacy path, including zero-height terrain, intact.
		if (recipe.Stamps.empty()) return {};
		if (!procedural || dataVersion != 2)
			return fail(Error::UnsupportedTerrain, "Terrain stamps require procedural Data v2.");
		if (!std::isfinite(heightScale) || heightScale <= 0.0f)
			return fail(Error::InvalidHeightScale, "Terrain stamps require a finite positive HeightScale.");
		std::unordered_set<uint64_t> ids;
		for (size_t i = 0; i < recipe.Stamps.size(); ++i)
		{
			const auto& stamp = recipe.Stamps[i];
			if (stamp.ID == 0) return fail(Error::InvalidID, "Stamp ID must be nonzero.", i);
			if (!ids.insert(stamp.ID).second)
				return fail(Error::DuplicateID, "Stamp IDs must be unique within the recipe.", i);
			if (stamp.Shape != TerrainStampShape::Ellipse && stamp.Shape != TerrainStampShape::Rectangle)
				return fail(Error::InvalidShape, "Unsupported stamp shape.", i);
			if (stamp.Operation != TerrainStampOperation::Add && stamp.Operation != TerrainStampOperation::SetHeight)
				return fail(Error::InvalidOperation, "Unsupported stamp operation.", i);
			if (!std::isfinite(stamp.Center.x) || !std::isfinite(stamp.Center.y)
				|| !std::isfinite(stamp.Size.x) || !std::isfinite(stamp.Size.y)
				|| !std::isfinite(stamp.RotationDegrees) || !std::isfinite(stamp.TransitionWidth)
				|| !std::isfinite(stamp.Strength) || !std::isfinite(stamp.Height))
				return fail(Error::NonFiniteParameter, "Stamp parameters must be finite.", i);
			if (stamp.Size.x <= 0.0f || stamp.Size.y <= 0.0f)
				return fail(Error::InvalidSize, "Stamp core dimensions must be positive.", i);
			if (stamp.TransitionWidth < 0.0f)
				return fail(Error::InvalidTransition, "Stamp transition width cannot be negative.", i);
			if (stamp.Strength < 0.0f || stamp.Strength > 1.0f)
				return fail(Error::InvalidStrength, "Stamp strength must be in [0, 1].", i);
		}
		return {};
	}

	TerrainRecipeEvaluationResult EvaluateTerrainRecipe(const TerrainRecipe& recipe,
		glm::vec2 localXZ, float baseNormalizedHeight, float heightScale)
	{
		TerrainRecipeEvaluationResult result;
		result.NormalizedHeight = baseNormalizedHeight;
		result.Validation = ValidateTerrainRecipe(recipe, heightScale);
		if (!result.Validation.Valid()) return result;
		if (!std::isfinite(localXZ.x) || !std::isfinite(localXZ.y)
			|| !std::isfinite(baseNormalizedHeight)
			|| baseNormalizedHeight < 0.0f || baseNormalizedHeight > 1.0f)
		{
			result.Validation = { TerrainRecipeError::InvalidSample,
				std::numeric_limits<size_t>::max(), "Reference sample must be finite with base height in [0, 1]." };
			return result;
		}
		if (recipe.Stamps.empty()) return result;
		// Double intermediates prevent overflow for finite but extreme source parameters.
		double height = double(baseNormalizedHeight) * heightScale;
		for (const auto& stamp : recipe.Stamps)
		{
			if (!stamp.Enabled || stamp.Strength == 0.0f) continue;
			// Positive rotation follows Transform's right-handed rotation around local Y.
			const double angle = std::remainder(double(stamp.RotationDegrees), 360.0) * 0.017453292519943295;
			const double c = std::cos(angle), s = std::sin(angle);
			const double dx = double(localXZ.x) - stamp.Center.x;
			const double dz = double(localXZ.y) - stamp.Center.y;
			const double x = c * dx - s * dz, z = s * dx + c * dz;
			const double halfX = double(stamp.Size.x) * 0.5, halfZ = double(stamp.Size.y) * 0.5;
			double distance;
			if (stamp.Shape == TerrainStampShape::Ellipse)
				// Radial metric scaled by the minor radius; not an exact ellipse SDF.
				distance = (std::hypot(x / halfX, z / halfZ) - 1.0) * std::min(halfX, halfZ);
			else
			{
				const double qx = std::abs(x) - halfX, qz = std::abs(z) - halfZ;
				distance = std::hypot(std::max(qx, 0.0), std::max(qz, 0.0))
					+ std::min(std::max(qx, qz), 0.0);
			}
			double weight = distance <= 0.0 ? 1.0 : 0.0;
			if (distance > 0.0 && stamp.TransitionWidth > 0.0f)
			{
				const double t = std::clamp(distance / stamp.TransitionWidth, 0.0, 1.0);
				weight = 1.0 - t * t * (3.0 - 2.0 * t);
			}
			weight *= stamp.Strength;
			if (weight == 0.0) continue;
			const double composed = stamp.Operation == TerrainStampOperation::Add
				? height + weight * stamp.Height : height + weight * (double(stamp.Height) - height);
			height = std::clamp(composed, 0.0, double(heightScale));
			result.Clipped |= height != composed;
		}
		result.NormalizedHeight = static_cast<float>(height / heightScale);
		return result;
	}

}
