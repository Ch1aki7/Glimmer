#pragma once

#include "Asset.h"
#include "Glimmer/Core/Core.h"

namespace gl {

	class Texture2D;

	class AssetManager
	{
	public:
		static void Initialize(
			const std::filesystem::path& assetDirectory,
			const std::filesystem::path& registryPath = {});
		static void Shutdown();

		static AssetHandle ImportAsset(const std::filesystem::path& path);
		static bool IsAssetHandleValid(AssetHandle handle);
		static AssetMetadata GetMetadata(AssetHandle handle);
		static std::filesystem::path GetFileSystemPath(AssetHandle handle);

		static Ref<Texture2D> GetTexture2D(AssetHandle handle);

	private:
		static AssetType GetAssetTypeFromExtension(const std::filesystem::path& path);
		static void SerializeRegistry();
		static void DeserializeRegistry();
	};

}
