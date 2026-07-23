#include "glpch.h"
#include "Entity.h"
#include "Components.h"

namespace gl {

	Entity::Entity(entt::entity handle, Scene* scene)
		: m_EntityHandle(handle), m_Scene(scene)
	{
	}


	UUID Entity::GetUUID() const
	{
		return GetComponent<IDComponent>().ID;
	}

}
