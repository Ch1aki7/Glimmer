#include "glpch.h"
#include "Model.h"

#include "Glimmer/Asset/Importers/ModelImporter.h"

namespace gl {

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
		for (const SubmeshSource& submesh : imported.Source.Submeshes)
		{
			Ref<Texture2D> baseColorTexture;
			if (submesh.MaterialIndex != InvalidMaterialIndex
				&& submesh.MaterialIndex < imported.Source.Materials.size())
			{
				const auto& material =
					imported.Source.Materials[submesh.MaterialIndex];
				if (!material.BaseColorTexturePath.empty())
				{
					baseColorTexture = Texture2D::Create(
						material.BaseColorTexturePath.string());
				}
			}

			m_Meshes.push_back(CreateRef<Mesh>(
				submesh.Vertices, submesh.Indices, std::move(baseColorTexture)));
		}

		GL_CORE_INFO("Model Loaded: {0}. Submeshes: {1}",
			path.string(), m_Meshes.size());
	}

}
