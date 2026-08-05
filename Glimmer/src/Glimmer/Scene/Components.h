#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Core/UUID.h"
#include "SceneCamera.h"
#include "ScriptableEntity.h"
#include "Glimmer/Renderer/MaterialInstance.h"
#include "Glimmer/Terrain/TerrainSettings.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace gl {

	struct TerrainRuntime;

	struct IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		explicit IDComponent(UUID id) : ID(id) {}
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag) : Tag(tag) {}
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation) : Translation(translation) {}

		glm::mat4 GetTransform() const
		{
			glm::quat q = glm::angleAxis(glm::radians(Rotation.z), glm::vec3(0, 0, 1))
				* glm::angleAxis(glm::radians(Rotation.y), glm::vec3(0, 1, 0))
				* glm::angleAxis(glm::radians(Rotation.x), glm::vec3(1, 0, 0));
			glm::mat4 rotation = glm::toMat4(q);

			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	struct SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		AssetHandle TextureHandle{ 0 };
		float TilingFactor = 1.0f;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color) : Color(color) {}
	};

	struct ModelRendererComponent
	{
		AssetHandle ModelHandle{ 0 };

		ModelRendererComponent() = default;
		ModelRendererComponent(const ModelRendererComponent&) = default;
		explicit ModelRendererComponent(AssetHandle handle) : ModelHandle(handle) {}
	};
	struct MaterialComponent
	{
		AssetHandle MaterialHandle{ 0 };
		MaterialOverrides Overrides;

		MaterialComponent() = default;
		MaterialComponent(const MaterialComponent&) = default;
		explicit MaterialComponent(AssetHandle handle) : MaterialHandle(handle) {}
	};
	struct TerrainComponent
	{
		TerrainSpecification Specification;
		Ref<TerrainRuntime> Runtime;

		TerrainComponent() = default;
		TerrainComponent(const TerrainComponent& other)
			: Specification(other.Specification) {}
	};
	struct DirectionalLightComponent
	{
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float AmbientIntensity = 0.05f;
		bool Enabled = true;

		DirectionalLightComponent() = default;
		DirectionalLightComponent(const DirectionalLightComponent&) = default;
	};

	struct PointLightComponent
	{
		glm::vec3 Color{ 1.0f };
		float Intensity = 10.0f;
		float Range = 10.0f;
		bool Enabled = true;

		PointLightComponent() = default;
		PointLightComponent(const PointLightComponent&) = default;
	};
	struct SkyLightComponent
	{
		AssetHandle CubemapHandle{ 0 };
		float Intensity = 1.0f;
		bool Enabled = true;

		SkyLightComponent() = default;
		SkyLightComponent(const SkyLightComponent&) = default;
		explicit SkyLightComponent(AssetHandle handle)
			: CubemapHandle(handle) {}
	};
	struct CameraComponent
	{
		gl::SceneCamera Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	struct NativeScriptComponent
	{
		ScriptableEntity* Instance = nullptr;

		ScriptableEntity* (*InstantiateScript)() = nullptr;
		void (*DestroyScript)(NativeScriptComponent*) = nullptr;

		template<typename T>
		void Bind()
		{
			InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}
	};

}
