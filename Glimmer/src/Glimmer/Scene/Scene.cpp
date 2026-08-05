#include "glpch.h"
#include "Scene.h"

#include "Components.h"
#include "Glimmer/Renderer/Renderer.h"
#include "Glimmer/Renderer/Renderer2D.h"
#include "Glimmer/Renderer/Renderer3D.h"
#include "Glimmer/Renderer/TerrainRenderer.h"
#include "Entity.h"

#include <glm/glm.hpp>

namespace gl {

	template<typename... Component>
	static void CopyComponents(
		entt::registry& destination,
		entt::registry& source,
		const std::unordered_map<UUID, entt::entity>& entityMap)
	{
		([&]()
		{
			auto view = source.view<Component>();
			for (auto sourceEntity : view)
			{
				UUID uuid = source.get<IDComponent>(sourceEntity).ID;
				auto destinationEntity = entityMap.at(uuid);
				destination.emplace_or_replace<Component>(
					destinationEntity,
					source.get<Component>(sourceEntity));
			}
		}(), ...);
	}

	template<typename T>
	static void CopyComponentIfPresent(Entity source, Entity destination)
	{
		if (!source.HasComponent<T>())
			return;
		if (destination.HasComponent<T>())
			destination.GetComponent<T>() = source.GetComponent<T>();
		else
			destination.AddComponent<T>(source.GetComponent<T>());
	}
	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
		OnRuntimeStop();
	}

	Ref<Scene> Scene::Copy(const Ref<Scene>& source)
	{
		GL_CORE_ASSERT(source, "Cannot copy a null Scene.");

		auto destination = CreateRef<Scene>();
		destination->m_ViewportWidth = source->m_ViewportWidth;
		destination->m_ViewportHeight = source->m_ViewportHeight;

		std::unordered_map<UUID, entt::entity> entityMap;
		auto idView = source->m_Registry.view<IDComponent>();
		for (auto sourceEntity : idView)
		{
			UUID uuid = idView.get<IDComponent>(sourceEntity).ID;
			std::string name = "Entity";
			if (source->m_Registry.all_of<TagComponent>(sourceEntity))
				name = source->m_Registry.get<TagComponent>(sourceEntity).Tag;

			Entity destinationEntity = destination->CreateEntityWithUUID(uuid, name);
			entityMap[uuid] = static_cast<entt::entity>(destinationEntity);
		}

		CopyComponents<TransformComponent, TagComponent, SpriteRendererComponent, ModelRendererComponent, MaterialComponent, DirectionalLightComponent, PointLightComponent, SkyLightComponent, CameraComponent, TerrainComponent>(
			destination->m_Registry,
			source->m_Registry,
			entityMap);

		auto scriptView = source->m_Registry.view<NativeScriptComponent>();
		for (auto sourceEntity : scriptView)
		{
			UUID uuid = source->m_Registry.get<IDComponent>(sourceEntity).ID;
			const auto& sourceScript = scriptView.get<NativeScriptComponent>(sourceEntity);
			auto& destinationScript = destination->m_Registry.emplace<NativeScriptComponent>(
				entityMap.at(uuid));
			destinationScript.InstantiateScript = sourceScript.InstantiateScript;
			destinationScript.DestroyScript = sourceScript.DestroyScript;
		}

		if (destination->m_ViewportWidth > 0 && destination->m_ViewportHeight > 0)
			destination->OnViewportResize(
				destination->m_ViewportWidth,
				destination->m_ViewportHeight);
		return destination;
	}
	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		GL_CORE_ASSERT(static_cast<uint64_t>(uuid) != 0, "Entity UUID cannot be zero.");
		GL_CORE_ASSERT(m_EntityMap.find(uuid) == m_EntityMap.end(), "Entity UUID already exists.");

		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;
		m_EntityMap[uuid] = entity;
		return entity;
	}

	Entity Scene::DuplicateEntity(Entity source)
	{
		if (!source || !source.HasComponent<TagComponent>())
			return {};

		Entity destination = CreateEntity(source.GetComponent<TagComponent>().Tag + " Copy");
		CopyComponentIfPresent<TransformComponent>(source, destination);
		CopyComponentIfPresent<SpriteRendererComponent>(source, destination);
		CopyComponentIfPresent<ModelRendererComponent>(source, destination);
		CopyComponentIfPresent<MaterialComponent>(source, destination);
		CopyComponentIfPresent<TerrainComponent>(source, destination);
		CopyComponentIfPresent<DirectionalLightComponent>(source, destination);
		CopyComponentIfPresent<PointLightComponent>(source, destination);
		CopyComponentIfPresent<SkyLightComponent>(source, destination);
		CopyComponentIfPresent<CameraComponent>(source, destination);
		return destination;
	}
	void Scene::DestroyEntity(Entity entity)
	{
		if (entity.HasComponent<NativeScriptComponent>())
		{
			auto& script = entity.GetComponent<NativeScriptComponent>();
			if (script.Instance)
			{
				script.Instance->OnDestroy();
				if (script.DestroyScript)
					script.DestroyScript(&script);
				else
				{
					delete script.Instance;
					script.Instance = nullptr;
				}
			}
		}

		if (entity.HasComponent<IDComponent>())
			m_EntityMap.erase(entity.GetUUID());
		m_Registry.destroy(entity);
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			const auto& camera = view.get<CameraComponent>(entity);
			if (camera.Primary)
				return Entity{ entity, this };
		}
		return {};
	}

	Entity Scene::GetSkyLightEntity()
	{
		auto view = m_Registry.view<SkyLightComponent>();
		for (auto entity : view)
		{
			const auto& skyLight = view.get<SkyLightComponent>(entity);
			if (skyLight.Enabled)
				return Entity{ entity, this };
		}
		return {};
	}
	Entity Scene::GetEntityByID(uint32_t id)
	{
		entt::entity handle = (entt::entity)id;
		if (m_Registry.valid(handle))
			return Entity{ handle, this };
		return {};
	}

	Entity Scene::FindEntityByUUID(UUID uuid)
	{
		auto iterator = m_EntityMap.find(uuid);
		if (iterator == m_EntityMap.end())
			return {};
		if (!m_Registry.valid(iterator->second))
		{
			m_EntityMap.erase(iterator);
			return {};
		}
		return Entity{ iterator->second, this };
	}

	void Scene::OnRuntimeStart()
	{
	}

	void Scene::OnRuntimeStop()
	{
		auto view = m_Registry.view<NativeScriptComponent>();
		for (auto entity : view)
		{
			auto& script = view.get<NativeScriptComponent>(entity);
			if (!script.Instance)
				continue;

			script.Instance->OnDestroy();
			if (script.DestroyScript)
				script.DestroyScript(&script);
			else
			{
				delete script.Instance;
				script.Instance = nullptr;
			}
		}
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{
		UploadLightEnvironment();
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;

		{
			// update scripts
			{
				m_Registry.view<NativeScriptComponent>().each([&](auto entity, auto& script)
				{
					if (!script.Instance)
					{
						if (!script.InstantiateScript)
						{
							GL_CORE_ERROR("NativeScriptComponent has no bound script.");
							return;
						}

						script.Instance = script.InstantiateScript();
						script.Instance->m_Entity = Entity{ entity, this };
						script.Instance->OnCreate();
					}

					script.Instance->OnUpdate(ts);
				});
			}

			// 寻找主相机
			{
				Entity camEntity = GetPrimaryCameraEntity();
				if (camEntity)
				{
					mainCamera = &camEntity.GetComponent<CameraComponent>().Camera;
					cameraTransform = camEntity.GetComponent<TransformComponent>().GetTransform();
				}
			}
		}

		// 执行渲染
		if (mainCamera)
		{
			const glm::mat4 viewProjection = mainCamera->GetProjection() * glm::inverse(cameraTransform);
			const glm::vec3 cameraPosition = glm::vec3(cameraTransform[3]);
			Renderer3D::BeginScene(viewProjection, cameraPosition);
			auto modelView = m_Registry.view<TransformComponent, ModelRendererComponent>();
			for (auto entity : modelView)
			{
				const auto& transform = modelView.get<TransformComponent>(entity);
				const auto& model = modelView.get<ModelRendererComponent>(entity);
				const auto* material = m_Registry.try_get<MaterialComponent>(entity);
				Renderer3D::DrawModel(
					transform.GetTransform(), model.ModelHandle,
					material ? material->MaterialHandle : AssetHandle(0),
					static_cast<int>(static_cast<uint32_t>(entity)),
					material ? &material->Overrides : nullptr);
			}

			auto terrainView = m_Registry.view<TransformComponent, TerrainComponent>();
			for (auto entity : terrainView)
			{
				auto& transform = terrainView.get<TransformComponent>(entity);
				auto& terrain = terrainView.get<TerrainComponent>(entity);
				TerrainRenderer::Draw(terrain, transform.GetTransform(), viewProjection,
					cameraPosition, static_cast<int>(static_cast<uint32_t>(entity)));
			}

			Renderer2D::BeginScene(viewProjection);

			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

				// 提交渲染，直接使用组件里的 Transform 矩阵
				const auto* material = m_Registry.try_get<MaterialComponent>(entity);
				Renderer2D::DrawSprite(
					transform.GetTransform(), sprite, (int)(uint32_t)entity,
					material ? material->MaterialHandle : AssetHandle(0),
					material ? &material->Overrides : nullptr);
			}

			Renderer2D::EndScene();
		}
	}

	void Scene::OnUpdateEditor(Timestep ts, const glm::mat4& viewProjection, const glm::vec3& cameraPosition)
	{
		UploadLightEnvironment();
		Renderer3D::BeginScene(viewProjection, cameraPosition);
		auto modelView = m_Registry.view<TransformComponent, ModelRendererComponent>();
		for (auto entity : modelView)
		{
			const auto& transform = modelView.get<TransformComponent>(entity);
			const auto& model = modelView.get<ModelRendererComponent>(entity);
			const auto* material = m_Registry.try_get<MaterialComponent>(entity);
			Renderer3D::DrawModel(
				transform.GetTransform(), model.ModelHandle,
				material ? material->MaterialHandle : AssetHandle(0),
				static_cast<int>(static_cast<uint32_t>(entity)),
				material ? &material->Overrides : nullptr);
		}

		auto terrainView = m_Registry.view<TransformComponent, TerrainComponent>();
		for (auto entity : terrainView)
		{
			auto& transform = terrainView.get<TransformComponent>(entity);
			auto& terrain = terrainView.get<TerrainComponent>(entity);
			TerrainRenderer::Draw(terrain, transform.GetTransform(), viewProjection,
				cameraPosition, static_cast<int>(static_cast<uint32_t>(entity)));
		}

		Renderer2D::BeginScene(viewProjection);

		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entityHandle : group)
		{
			auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entityHandle);
			const auto* material = m_Registry.try_get<MaterialComponent>(entityHandle);
			Renderer2D::DrawSprite(
				transform.GetTransform(), sprite, (int)(uint32_t)entityHandle,
				material ? material->MaterialHandle : AssetHandle(0),
				material ? &material->Overrides : nullptr);
		}

		Renderer2D::EndScene();
	}

	void Scene::UploadLightEnvironment()
	{
		LightEnvironment environment;

		auto directionalView = m_Registry.view<TransformComponent, DirectionalLightComponent>();
		for (auto entity : directionalView)
		{
			const auto& transform = directionalView.get<TransformComponent>(entity);
			const auto& component = directionalView.get<DirectionalLightComponent>(entity);
			if (!component.Enabled)
				continue;

			const glm::vec3 forward = glm::mat3(transform.GetTransform())
				* glm::vec3(0.0f, 0.0f, -1.0f);
			environment.Directional.Direction = glm::length(forward) > 0.0001f
				? glm::normalize(forward)
				: glm::vec3(0.0f, -1.0f, 0.0f);
			environment.Directional.Color = component.Color;
			environment.Directional.Intensity = component.Intensity;
			environment.Directional.AmbientIntensity = component.AmbientIntensity;
			environment.Directional.Enabled = true;
			break;
		}

		auto pointView = m_Registry.view<TransformComponent, PointLightComponent>();
		environment.PointLights.reserve(LightEnvironment::MaxPointLights);

		for (auto entity : pointView)
		{
			const auto& component = pointView.get<PointLightComponent>(entity);
			if (!component.Enabled)
				continue;
			if (environment.PointLights.size() >= LightEnvironment::MaxPointLights)
				break;

			const auto& transform = pointView.get<TransformComponent>(entity);
			PointLight light;
			light.Position = transform.Translation;
			light.Color = component.Color;
			light.Intensity = component.Intensity;
			light.Range = component.Range;
			environment.PointLights.push_back(light);
		}

		Renderer::UploadLightEnvironment(environment);
	}
	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		// 遍历所有相机，更新非固定纵横比相机的投影
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
			{
				cameraComponent.Camera.SetViewportSize(width, height);
			}
		}
	}

	// 各个组件添加时的回调模板特化
	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component) {}

	template<>
	void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component) {}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component) {}

	template<>
	void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component) {}

	template<>
	void Scene::OnComponentAdded<ModelRendererComponent>(Entity entity, ModelRendererComponent& component) {}
	template<>
	void Scene::OnComponentAdded<MaterialComponent>(Entity entity, MaterialComponent& component) {}
	template<>
	void Scene::OnComponentAdded<TerrainComponent>(Entity entity, TerrainComponent& component) {}
	template<>
	void Scene::OnComponentAdded<DirectionalLightComponent>(Entity entity, DirectionalLightComponent& component) {}

	template<>
	void Scene::OnComponentAdded<PointLightComponent>(Entity entity, PointLightComponent& component) {}

	template<>
	void Scene::OnComponentAdded<SkyLightComponent>(Entity entity, SkyLightComponent& component) {}
	template<>
	void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component) {}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		// 当相机被添加时，如果还没设置 Viewport 大小，则初始化一次
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
			component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
	}

	template<>
	void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component) {}

}
