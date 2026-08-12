#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include "entt/entt.hpp"
#include "Glimmer/Core/Core.h"
#include "Glimmer/Core/Timestep.h"
#include "Glimmer/Core/UUID.h"

namespace gl {

	class Entity;

	class Scene
	{
	public:
		Scene();
		~Scene();

		static Ref<Scene> Copy(const Ref<Scene>& source);

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		void DestroyEntity(Entity entity);
		Entity DuplicateEntity(Entity entity);

		Entity GetPrimaryCameraEntity();
		Entity GetSkyLightEntity();
		Entity GetDirectionalLightEntity();
		Entity GetEntityByID(uint32_t id);              // entt entity ID 反向查找
		Entity FindEntityByUUID(UUID uuid);

		void OnRuntimeStart();
		void OnRuntimeStop();
		void OnUpdateRuntime(Timestep ts, bool deferSpritePass = false);
		void OnUpdateEditor(Timestep ts, const glm::mat4& view,
			const glm::mat4& projection, const glm::vec3& cameraPosition,
			float cameraNear, float cameraFar, bool deferSpritePass = false);
		void FlushSpritePass();
		void OnViewportResize(uint32_t width, uint32_t height);

	private:
		void UploadLightEnvironment();
		void RenderDirectionalShadowMap(const glm::mat4& cameraView,
			const glm::mat4& cameraProjection, float cameraNear, float cameraFar);
		void RenderSprites(const glm::mat4& viewProjection);

		template<typename T>
		void OnComponentAdded(Entity entity, T& component);

	private:
		entt::registry m_Registry;
		std::unordered_map<UUID, entt::entity> m_EntityMap;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
		bool m_SpritePassPending = false;
		glm::mat4 m_DeferredSpriteViewProjection{ 1.0f };

		friend class Entity;
		friend class SceneHierarchyPanel;
		friend class SceneSerializer;
	};

}
