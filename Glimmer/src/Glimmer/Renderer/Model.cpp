#include "glpch.h"
#include "Model.h"

#include "Glimmer/Asset/Importers/ModelImporter.h"

namespace gl {

	namespace {

		Ref<Texture2D> LoadImportedTexture(
			const std::filesystem::path& path,
			TextureColorSpace colorSpace)
		{
			if (path.empty())
				return nullptr;
			std::error_code error;
			if (!std::filesystem::is_regular_file(path, error))
				return nullptr;
			return Texture2D::Create(path.string(), colorSpace);
		}

		MeshMaterialTextures LoadImportedMaterial(
			const MeshMaterialSource& source)
		{
			MeshMaterialTextures textures;
			textures.BaseColor = LoadImportedTexture(
				source.BaseColorTexturePath, TextureColorSpace::SRGB);
			textures.Normal = LoadImportedTexture(
				source.NormalTexturePath, TextureColorSpace::Linear);
			textures.Metallic = LoadImportedTexture(
				source.MetallicTexturePath, TextureColorSpace::Linear);
			textures.Roughness = LoadImportedTexture(
				source.RoughnessTexturePath, TextureColorSpace::Linear);
			textures.AmbientOcclusion = LoadImportedTexture(
				source.AOTexturePath, TextureColorSpace::Linear);
			textures.Emissive = LoadImportedTexture(
				source.EmissiveTexturePath, TextureColorSpace::SRGB);
			return textures;
		}

	}

	Model::Model(const std::filesystem::path& path)
	{
		ModelImportResult imported = ModelImporter::Import(path);
		if (!imported)
		{
			GL_CORE_ERROR("Model import failed for '{0}': {1}",
				path.string(), imported.Error);
			return;
		}

		m_Meshes.reserve(imported.Source.Submeshes.size());
		std::vector<MeshMaterialTextures> loadedMaterials(
			imported.Source.Materials.size());
		std::vector<bool> materialLoaded(imported.Source.Materials.size(), false);
		for (const SubmeshSource& submesh : imported.Source.Submeshes)
		{
			MeshMaterialTextures materialTextures;
			if (submesh.MaterialIndex != InvalidMaterialIndex
				&& submesh.MaterialIndex < imported.Source.Materials.size())
			{
				if (!materialLoaded[submesh.MaterialIndex])
				{
					loadedMaterials[submesh.MaterialIndex] = LoadImportedMaterial(
						imported.Source.Materials[submesh.MaterialIndex]);
					materialLoaded[submesh.MaterialIndex] = true;
				}
				materialTextures = loadedMaterials[submesh.MaterialIndex];
			}

			m_Meshes.push_back(CreateRef<Mesh>(
				submesh.Vertices, submesh.Indices, std::move(materialTextures)));
		}

		GL_CORE_INFO("Model Loaded: {0}. Submeshes: {1}",
			path.string(), m_Meshes.size());
	}

}
