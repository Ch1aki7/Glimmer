#pragma once

#include "Glimmer/Core/UUID.h"
#include "Glimmer/Terrain/TerrainSettings.h"
#include <algorithm>

namespace gl {
	// Editor list operations preserve existing IDs and leave rejected edits untouched.
	struct TerrainRecipeEditor
	{
		static bool Add(TerrainSpecification& spec, TerrainStampShape shape, TerrainStampOperation operation)
		{
			TerrainStamp stamp;
			do { stamp.ID = uint64_t(UUID()); }
			while (stamp.ID == 0 || std::any_of(spec.Recipe.Stamps.begin(), spec.Recipe.Stamps.end(),
				[&](const TerrainStamp& other) { return other.ID == stamp.ID; }));
			stamp.Shape = shape;
			stamp.Operation = operation;
			stamp.Height = operation == TerrainStampOperation::SetHeight ? spec.HeightScale * 0.4f : -spec.HeightScale * 0.1f;
			auto candidate = spec.Recipe;
			candidate.Stamps.push_back(stamp);
			if (!ValidateTerrainRecipe(candidate, spec.HeightScale, spec.DataVersion, spec.Procedural).Valid()) return false;
			spec.Recipe = std::move(candidate);
			return true;
		}
		static bool Remove(TerrainRecipe& recipe, size_t index)
		{
			if (index >= recipe.Stamps.size()) return false;
			recipe.Stamps.erase(recipe.Stamps.begin() + index);
			return true;
		}
		static bool Move(TerrainRecipe& recipe, size_t index, int direction)
		{
			if (index >= recipe.Stamps.size() || (direction != -1 && direction != 1)
				|| (direction == -1 && index == 0) || (direction == 1 && index + 1 == recipe.Stamps.size())) return false;
			std::swap(recipe.Stamps[index], recipe.Stamps[size_t(int(index) + direction)]);
			return true;
		}
	};
}
