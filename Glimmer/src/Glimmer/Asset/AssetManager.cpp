#include "glpch.h"
#include "AssetManager.h"

#include "Glimmer/Renderer/Texture.h"
#include "Glimmer/Renderer/Material.h"
#include "Glimmer/Renderer/Model.h"
#include "Glimmer/Renderer/Shader.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <mutex>

namespace gl {

	namespace {

		struct AssetManagerData
		{
			std::filesystem::path AssetDirectory;
			std::filesystem::path RegistryPath;
			std::unordered_map<AssetHandle, AssetMetadata> Registry;
			std::unordered_map<std::string, AssetHandle> PathToHandle;
			std::unordered_map<AssetHandle, Ref<Texture2D>> TextureCache;
			std::unordered_map<AssetHandle, Ref<Material>> MaterialCache;
			std::unordered_map<AssetHandle, Ref<Model>> ModelCache;
			std::unordered_map<AssetHandle, Ref<Shader>> ShaderCache;
			std::mutex Mutex;
			bool Initialized = false;
		};

		AssetManagerData s_Data;
		const AssetMetadata s_NullMetadata;

		std::string NormalizePathKey(const std::filesystem::path& path)
		{
			std::string key = path.lexically_normal().generic_string();
#ifdef GL_PLATFORM_WINDOWS
			std::transform(key.begin(), key.end(), key.begin(),
				[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
#endif
			return key;
		}

		const char* AssetTypeToString(AssetType type)
		{
			switch (type)
			{
				case AssetType::Texture2D: return "Texture2D";
				case AssetType::Model: return "Model";
				case AssetType::Shader: return "Shader";
				case AssetType::Material: return "Material";
				default: return "None";
			}
		}

		AssetType AssetTypeFromString(const std::string& type)
		{
			if (type == "Texture2D") return AssetType::Texture2D;
			if (type == "Model") return AssetType::Model;
			if (type == "Shader") return AssetType::Shader;
			if (type == "Material") return AssetType::Material;
			return AssetType::None;
		}

	}

	void AssetManager::Initialize(
		const std::filesystem::path& assetDirectory,
		const std::filesystem::path& registryPath)
	{
		std::scoped_lock lock(s_Data.Mutex);
		if (s_Data.Initialized)
			return;

		std::error_code error;
		s_Data.AssetDirectory = std::filesystem::weakly_canonical(assetDirectory, error);
		if (error)
			s_Data.AssetDirectory = std::filesystem::absolute(assetDirectory).lexically_normal();

		s_Data.RegistryPath = registryPath.empty()
			? s_Data.AssetDirectory / "AssetRegistry.yaml"
			: std::filesystem::absolute(registryPath).lexically_normal();
		s_Data.Initialized = true;
		DeserializeRegistry();
	}

	void AssetManager::Shutdown()
	{
		std::scoped_lock lock(s_Data.Mutex);
		s_Data.TextureCache.clear();
		s_Data.MaterialCache.clear();
		s_Data.ModelCache.clear();
		s_Data.ShaderCache.clear();
		s_Data.PathToHandle.clear();
		s_Data.Registry.clear();
		s_Data.AssetDirectory.clear();
		s_Data.RegistryPath.clear();
		s_Data.Initialized = false;
	}

	AssetHandle AssetManager::ImportAsset(const std::filesystem::path& path)
	{
		std::scoped_lock lock(s_Data.Mutex);
		if (!s_Data.Initialized)
		{
			GL_CORE_ERROR("AssetManager must be initialized before importing assets.");
			return AssetHandle(0);
		}

		std::error_code error;
		std::filesystem::path absolutePath = path.is_absolute()
			? std::filesystem::weakly_canonical(path, error)
			: std::filesystem::weakly_canonical(
				std::filesystem::current_path() / path, error);
		if (error || !std::filesystem::is_regular_file(absolutePath))
		{
			GL_CORE_ERROR("Asset does not exist: {0}", path.string());
			return AssetHandle(0);
		}

		std::filesystem::path relativePath =
			std::filesystem::relative(absolutePath, s_Data.AssetDirectory, error);
		if (error || relativePath.empty()
			|| (!relativePath.empty() && *relativePath.begin() == ".."))
		{
			GL_CORE_ERROR("Asset is outside the project asset directory: {0}", path.string());
			return AssetHandle(0);
		}

		const AssetType type = GetAssetTypeFromExtension(relativePath);
		if (type == AssetType::None)
		{
			GL_CORE_WARN("Unsupported asset type: {0}", path.string());
			return AssetHandle(0);
		}

		const std::string key = NormalizePathKey(relativePath);
		auto existing = s_Data.PathToHandle.find(key);
		if (existing != s_Data.PathToHandle.end())
			return existing->second;

		AssetMetadata metadata;
		metadata.Handle = AssetHandle();
		metadata.Type = type;
		metadata.FilePath = relativePath.lexically_normal();
		s_Data.Registry.emplace(metadata.Handle, metadata);
		s_Data.PathToHandle.emplace(key, metadata.Handle);
		SerializeRegistry();
		return metadata.Handle;
	}

	bool AssetManager::IsAssetHandleValid(AssetHandle handle)
	{
		std::scoped_lock lock(s_Data.Mutex);
		return static_cast<uint64_t>(handle) != 0
			&& s_Data.Registry.find(handle) != s_Data.Registry.end();
	}

	AssetMetadata AssetManager::GetMetadata(AssetHandle handle)
	{
		std::scoped_lock lock(s_Data.Mutex);
		auto iterator = s_Data.Registry.find(handle);
		return iterator != s_Data.Registry.end() ? iterator->second : s_NullMetadata;
	}

	std::filesystem::path AssetManager::GetFileSystemPath(AssetHandle handle)
	{
		std::scoped_lock lock(s_Data.Mutex);
		auto iterator = s_Data.Registry.find(handle);
		return iterator != s_Data.Registry.end()
			? s_Data.AssetDirectory / iterator->second.FilePath
			: std::filesystem::path{};
	}

	Ref<Texture2D> AssetManager::GetTexture2D(AssetHandle handle)
	{
		std::scoped_lock lock(s_Data.Mutex);
		if (static_cast<uint64_t>(handle) == 0)
			return nullptr;

		auto cached = s_Data.TextureCache.find(handle);
		if (cached != s_Data.TextureCache.end())
			return cached->second;

		auto metadata = s_Data.Registry.find(handle);
		if (metadata == s_Data.Registry.end() || metadata->second.Type != AssetType::Texture2D)
			return nullptr;

		const auto path = s_Data.AssetDirectory / metadata->second.FilePath;
		if (!std::filesystem::is_regular_file(path))
		{
			GL_CORE_ERROR("Texture asset is missing: {0}", path.string());
			return nullptr;
		}

		Ref<Texture2D> texture = Texture2D::Create(path.string());
		s_Data.TextureCache.emplace(handle, texture);
		return texture;
	}

	Ref<Material> AssetManager::GetMaterial(AssetHandle handle)
	{
		std::scoped_lock lock(s_Data.Mutex);
		if (static_cast<uint64_t>(handle) == 0)
			return nullptr;

		auto cached = s_Data.MaterialCache.find(handle);
		if (cached != s_Data.MaterialCache.end())
			return cached->second;

		auto metadata = s_Data.Registry.find(handle);
		if (metadata == s_Data.Registry.end() || metadata->second.Type != AssetType::Material)
			return nullptr;

		const auto path = s_Data.AssetDirectory / metadata->second.FilePath;
		Ref<Material> material = Material::Create(path);
		if (material)
			s_Data.MaterialCache.emplace(handle, material);
		return material;
	}
	Ref<Model> AssetManager::GetModel(AssetHandle handle)
	{
		std::scoped_lock lock(s_Data.Mutex);
		if (static_cast<uint64_t>(handle) == 0)
			return nullptr;

		auto cached = s_Data.ModelCache.find(handle);
		if (cached != s_Data.ModelCache.end())
			return cached->second;

		auto metadata = s_Data.Registry.find(handle);
		if (metadata == s_Data.Registry.end() || metadata->second.Type != AssetType::Model)
			return nullptr;

		Ref<Model> model = CreateRef<Model>(
			(s_Data.AssetDirectory / metadata->second.FilePath).string());
		if (!model->IsValid())
			return nullptr;

		s_Data.ModelCache.emplace(handle, model);
		return model;
	}

	Ref<Shader> AssetManager::GetShader(AssetHandle handle)
	{
		std::scoped_lock lock(s_Data.Mutex);
		if (static_cast<uint64_t>(handle) == 0)
			return nullptr;

		auto cached = s_Data.ShaderCache.find(handle);
		if (cached != s_Data.ShaderCache.end())
			return cached->second;

		auto metadata = s_Data.Registry.find(handle);
		if (metadata == s_Data.Registry.end() || metadata->second.Type != AssetType::Shader)
			return nullptr;

		Ref<Shader> shader = Shader::Create(
			(s_Data.AssetDirectory / metadata->second.FilePath).string());
		if (shader)
			s_Data.ShaderCache.emplace(handle, shader);
		return shader;
	}
	AssetType AssetManager::GetAssetTypeFromExtension(const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });

		if (extension == ".png" || extension == ".jpg"
			|| extension == ".jpeg" || extension == ".tga"
			|| extension == ".bmp")
			return AssetType::Texture2D;
		if (extension == ".obj")
			return AssetType::Model;
		if (extension == ".glsl" || extension == ".comp")
			return AssetType::Shader;
		if (extension == ".glmat")
			return AssetType::Material;
		return AssetType::None;
	}

	void AssetManager::SerializeRegistry()
	{
		YAML::Emitter output;
		output << YAML::BeginMap;
		output << YAML::Key << "AssetRegistry" << YAML::Value << YAML::BeginSeq;
		std::vector<AssetMetadata> sortedMetadata;
		sortedMetadata.reserve(s_Data.Registry.size());
		for (const auto& [handle, metadata] : s_Data.Registry)
			sortedMetadata.push_back(metadata);
		std::sort(sortedMetadata.begin(), sortedMetadata.end(),
			[](const AssetMetadata& left, const AssetMetadata& right)
			{
				return static_cast<uint64_t>(left.Handle)
					< static_cast<uint64_t>(right.Handle);
			});

		for (const auto& metadata : sortedMetadata)
		{
			output << YAML::BeginMap;
			output << YAML::Key << "Handle" << YAML::Value
				<< static_cast<uint64_t>(metadata.Handle);
			output << YAML::Key << "Type" << YAML::Value << AssetTypeToString(metadata.Type);
			output << YAML::Key << "FilePath" << YAML::Value << metadata.FilePath.generic_string();
			output << YAML::EndMap;
		}
		output << YAML::EndSeq;
		output << YAML::EndMap;

		std::ofstream stream(s_Data.RegistryPath);
		if (!stream)
		{
			GL_CORE_ERROR("Could not write asset registry: {0}", s_Data.RegistryPath.string());
			return;
		}
		stream << output.c_str();
	}

	void AssetManager::DeserializeRegistry()
	{
		if (!std::filesystem::is_regular_file(s_Data.RegistryPath))
			return;

		try
		{
			YAML::Node root = YAML::LoadFile(s_Data.RegistryPath.string());
			auto registry = root["AssetRegistry"];
			if (!registry)
				return;

			for (const auto& entry : registry)
			{
				AssetMetadata metadata;
				metadata.Handle = AssetHandle(entry["Handle"].as<uint64_t>());
				metadata.Type = AssetTypeFromString(entry["Type"].as<std::string>());
				metadata.FilePath = entry["FilePath"].as<std::string>();
				if (!metadata.IsValid())
					continue;

				s_Data.PathToHandle.insert_or_assign(
					NormalizePathKey(metadata.FilePath), metadata.Handle);
				s_Data.Registry.insert_or_assign(metadata.Handle, metadata);
			}
		}
		catch (const YAML::Exception& exception)
		{
			GL_CORE_ERROR("Asset registry parse error: {0}", exception.what());
		}
	}

}
