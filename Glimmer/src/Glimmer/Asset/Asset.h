#pragma once

#include "Glimmer/Core/UUID.h"
#include "Glimmer/Asset/TextureAssetMetadata.h"

#include <filesystem>

namespace gl {

	using AssetHandle = UUID;

	enum class AssetType
	{
		None = 0,
		Texture2D,
		Model,
		Shader,
		Material
	};

	struct AssetMetadata
	{
		AssetHandle Handle{ 0 };
		AssetType Type = AssetType::None;
		std::filesystem::path FilePath;
		TextureColorSpace ColorSpace = TextureColorSpace::Linear;
		TextureSemantic Semantic = TextureSemantic::Data;

		bool IsValid() const
		{
			return static_cast<uint64_t>(Handle) != 0
				&& Type != AssetType::None
				&& !FilePath.empty();
		}
	};

}
