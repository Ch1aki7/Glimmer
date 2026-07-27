#pragma once

#include "Asset.h"
#include "Glimmer/Core/Core.h"

namespace gl {

	class Texture2D;
	class Material;
	class Model;
	class Shader;

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
		static bool SetTextureMetadata(AssetHandle handle,
			TextureColorSpace colorSpace,
			TextureSemantic semantic);
		static std::filesystem::path GetFileSystemPath(AssetHandle handle);

		static Ref<Texture2D> GetTexture2D(AssetHandle handle);
		static Ref<Material> GetMaterial(AssetHandle handle);
		static Ref<Model> GetModel(AssetHandle handle);
		static Ref<Shader> GetShader(AssetHandle handle);

	private:
		static AssetType GetAssetTypeFromExtension(const std::filesystem::path& path);
		static void SerializeRegistry();
		static void DeserializeRegistry();
	};

}
