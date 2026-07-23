#include "glpch.h"
#include "Scene.h"

#include "Components.h"
#include "Glimmer/Renderer/Renderer2D.h"
#include "Entity.h"

#include <glm/glm.hpp>

namespace gl {

	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
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

	void Scene::DestroyEntity(Entity entity)
	{
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
	void Scene::OnUpdateRuntime(Timestep ts)
	{
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;

		{
			// update scripts
			{
				m_Registry.view<NativeScriptComponent>().each([&](auto entity, auto& nsc)
					{
						// 如果脚本还没实例化，则在此处创建（延迟加载）
						if (!nsc.Instance)
						{
							nsc.Instance = nsc.InstantiateScript();
							nsc.Instance->m_Entity = Entity{ entity, this };
							nsc.Instance->OnCreate();
						}

						nsc.Instance->OnUpdate(ts);
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
			// 视图矩阵是相机变换矩阵的逆矩阵
			// 在 2D 中，相机往右移，世界看起来往左移
			Renderer2D::BeginScene(mainCamera->GetProjection(), glm::inverse(cameraTransform));

			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

				// 提交渲染，直接使用组件里的 Transform 矩阵
				Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)(uint32_t)entity);
			}

			Renderer2D::EndScene();
		}
	}

	void Scene::OnUpdateEditor(Timestep ts, const glm::mat4& viewProjection)
	{
		Renderer2D::BeginScene(viewProjection);

		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entityHandle : group)
		{
			auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entityHandle);
			Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)(uint32_t)entityHandle);
		}

		Renderer2D::EndScene();
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
