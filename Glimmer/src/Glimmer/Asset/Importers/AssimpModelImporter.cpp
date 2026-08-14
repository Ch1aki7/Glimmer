#include "glpch.h"
#include "AssimpModelImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <array>

namespace gl {

	namespace {

		glm::vec3 ToVec3(const aiVector3D& value)
		{
			return { value.x, value.y, value.z };
		}

		std::filesystem::path NormalizeReferencedPath(const aiString& value)
		{
			std::string path = value.C_Str();
			std::replace(path.begin(), path.end(), '\\', '/');
			return std::filesystem::path(path).lexically_normal();
		}

		std::filesystem::path ResolveTexturePath(
			const std::filesystem::path& modelPath,
			const aiString& referencedPath)
		{
			const std::filesystem::path reference =
				NormalizeReferencedPath(referencedPath);
			if (reference.empty() || reference.string().front() == '*')
				return {};

			std::array<std::filesystem::path, 2> candidates =
				reference.is_absolute()
				? std::array<std::filesystem::path, 2>{
					reference,
					modelPath.parent_path() / reference.filename() }
				: std::array<std::filesystem::path, 2>{
					modelPath.parent_path() / reference,
					modelPath.parent_path() / reference.filename() };
			for (const auto& candidate : candidates)
			{
				std::error_code error;
				if (std::filesystem::is_regular_file(candidate, error))
					return std::filesystem::weakly_canonical(candidate, error);
			}
			return {};
		}

		std::filesystem::path FindTexture(
			const aiMaterial& material,
			const std::filesystem::path& modelPath,
			std::initializer_list<aiTextureType> types)
		{
			for (const aiTextureType type : types)
			{
				for (unsigned int index = 0;
					index < material.GetTextureCount(type); ++index)
				{
					aiString path;
					if (material.GetTexture(type, index, &path) != AI_SUCCESS)
						continue;
					const auto resolved = ResolveTexturePath(modelPath, path);
					if (!resolved.empty())
						return resolved;
				}
			}
			return {};
		}

		std::string RemoveKnownSuffix(std::string stem)
		{
			static constexpr std::array<const char*, 11> suffixes{
				"_BaseColor", "_basecolor", "_Albedo", "_albedo",
				"_Diffuse", "_diffuse", "_LOD0", "_lod0", "_LP", "_lp", "_A"
			};
			for (const char* suffix : suffixes)
			{
				const size_t length = std::char_traits<char>::length(suffix);
				if (stem.size() >= length
					&& stem.compare(stem.size() - length, length, suffix) == 0)
				{
					stem.erase(stem.size() - length);
					break;
				}
			}
			return stem;
		}

		std::filesystem::path FindSidecarTexture(
			const std::filesystem::path& modelPath,
			const std::filesystem::path& baseColorPath,
			std::initializer_list<const char*> suffixes)
		{
			std::vector<std::string> stems;
			if (!baseColorPath.empty())
				stems.push_back(RemoveKnownSuffix(baseColorPath.stem().string()));
			stems.push_back(RemoveKnownSuffix(modelPath.stem().string()));

			std::vector<std::filesystem::path> directories;
			if (!baseColorPath.empty())
			{
				directories.push_back(baseColorPath.parent_path());
				directories.push_back(baseColorPath.parent_path() / "Raw");
			}
			directories.push_back(modelPath.parent_path());
			directories.push_back(modelPath.parent_path() / "Textures");
			directories.push_back(modelPath.parent_path() / "Textures" / "Raw");

			static constexpr std::array<const char*, 5> extensions{
				".tga", ".png", ".jpg", ".jpeg", ".bmp"
			};
			for (const auto& directory : directories)
			{
				for (const std::string& stem : stems)
				{
					for (const char* suffix : suffixes)
					{
						for (const char* extension : extensions)
						{
							const auto candidate = directory
								/ (stem + suffix + extension);
							std::error_code error;
							if (std::filesystem::is_regular_file(candidate, error))
								return std::filesystem::weakly_canonical(candidate, error);
						}
					}
				}
			}
			return {};
		}

		MeshMaterialSource ReadMaterial(
			const aiMaterial& material,
			const std::filesystem::path& modelPath)
		{
			MeshMaterialSource source;
			aiString name;
			if (material.Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
				source.Name = name.C_Str();

			aiColor4D baseColor;
			if (material.Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS
				|| material.Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS)
			{
				source.BaseColorFactor = {
					baseColor.r, baseColor.g, baseColor.b, baseColor.a };
			}
			aiColor3D emissive;
			if (material.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
				source.EmissiveFactor = { emissive.r, emissive.g, emissive.b };
			material.Get(AI_MATKEY_METALLIC_FACTOR, source.MetallicFactor);
			material.Get(AI_MATKEY_ROUGHNESS_FACTOR, source.RoughnessFactor);

			source.BaseColorTexturePath = FindTexture(material, modelPath,
				{ aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE });
			source.NormalTexturePath = FindTexture(material, modelPath,
				{ aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA,
				  aiTextureType_HEIGHT });
			source.MetallicTexturePath = FindTexture(material, modelPath,
				{ aiTextureType_METALNESS });
			source.RoughnessTexturePath = FindTexture(material, modelPath,
				{ aiTextureType_DIFFUSE_ROUGHNESS });
			source.AOTexturePath = FindTexture(material, modelPath,
				{ aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP,
				  aiTextureType_AMBIENT });
			source.EmissiveTexturePath = FindTexture(material, modelPath,
				{ aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE });

			if (source.NormalTexturePath.empty())
				source.NormalTexturePath = FindSidecarTexture(modelPath,
					source.BaseColorTexturePath, { "_N", "_Normal", "_normal" });
			if (source.MetallicTexturePath.empty())
				source.MetallicTexturePath = FindSidecarTexture(modelPath,
					source.BaseColorTexturePath, { "_M", "_Metallic", "_metallic" });
			if (source.RoughnessTexturePath.empty())
				source.RoughnessTexturePath = FindSidecarTexture(modelPath,
					source.BaseColorTexturePath, { "_R", "_Roughness", "_roughness" });
			if (source.AOTexturePath.empty())
				source.AOTexturePath = FindSidecarTexture(modelPath,
					source.BaseColorTexturePath, { "_AO", "_Occlusion", "_occlusion" });
			return source;
		}

		MeshVertex ReadVertex(const aiMesh& mesh, unsigned int index)
		{
			MeshVertex vertex;
			vertex.Position = ToVec3(mesh.mVertices[index]);
			if (mesh.HasNormals())
				vertex.Normal = ToVec3(mesh.mNormals[index]);
			if (mesh.HasTangentsAndBitangents())
				vertex.Tangent = ToVec3(mesh.mTangents[index]);
			if (mesh.HasTextureCoords(0))
				vertex.TexCoord = {
					mesh.mTextureCoords[0][index].x,
					mesh.mTextureCoords[0][index].y };
			if (glm::dot(vertex.Tangent, vertex.Tangent) <= 0.000001f)
			{
				const glm::vec3 normal =
					glm::dot(vertex.Normal, vertex.Normal) > 0.000001f
					? glm::normalize(vertex.Normal) : glm::vec3(0.0f, 1.0f, 0.0f);
				const glm::vec3 helper = glm::abs(normal.y) < 0.999f
					? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
				vertex.Tangent = glm::normalize(glm::cross(helper, normal));
			}
			return vertex;
		}

	}

	ModelImportResult AssimpModelImporter::Import(
		const std::filesystem::path& path)
	{
		ModelImportResult result;
		result.Source.SourcePath = path;

		Assimp::Importer importer;
		constexpr unsigned int flags =
			aiProcess_Triangulate
			| aiProcess_JoinIdenticalVertices
			| aiProcess_GenSmoothNormals
			| aiProcess_CalcTangentSpace
			| aiProcess_ImproveCacheLocality
			| aiProcess_SortByPType
			| aiProcess_ValidateDataStructure
			| aiProcess_PreTransformVertices;
		const aiScene* scene = importer.ReadFile(path.string(), flags);
		if (!scene || !scene->mRootNode
			|| (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0)
		{
			result.Error = importer.GetErrorString();
			if (result.Error.empty())
				result.Error = "Assimp could not parse the model.";
			return result;
		}

		result.Source.Materials.reserve(scene->mNumMaterials);
		for (unsigned int index = 0; index < scene->mNumMaterials; ++index)
			result.Source.Materials.push_back(
				ReadMaterial(*scene->mMaterials[index], path));

		result.Source.Submeshes.reserve(scene->mNumMeshes);
		for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
		{
			const aiMesh& mesh = *scene->mMeshes[meshIndex];
			if (!mesh.HasPositions() || mesh.mNumVertices == 0 || mesh.mNumFaces == 0)
				continue;

			SubmeshSource submesh;
			submesh.Name = mesh.mName.length > 0
				? mesh.mName.C_Str() : "Submesh " + std::to_string(meshIndex);
			submesh.MaterialIndex = mesh.mMaterialIndex < scene->mNumMaterials
				? mesh.mMaterialIndex : InvalidMaterialIndex;
			submesh.Vertices.reserve(mesh.mNumVertices);
			for (unsigned int vertex = 0; vertex < mesh.mNumVertices; ++vertex)
				submesh.Vertices.push_back(ReadVertex(mesh, vertex));

			for (unsigned int faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex)
			{
				const aiFace& face = mesh.mFaces[faceIndex];
				if (face.mNumIndices != 3)
					continue;
				submesh.Indices.insert(submesh.Indices.end(),
					face.mIndices, face.mIndices + face.mNumIndices);
			}
			if (submesh.IsValid())
				result.Source.Submeshes.push_back(std::move(submesh));
		}

		if (!result.Source.IsValid())
			result.Error = "Assimp model contains no valid triangle submeshes.";
		return result;
	}

}
