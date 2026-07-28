#pragma once

#include "Glimmer.h"

namespace gl
{
	enum class SelectionType
	{
		None = 0,
		Entity,
		Asset
	};

	class SelectionContext
	{
	public:
		void SelectEntity(Entity entity)
		{
			m_Type = entity ? SelectionType::Entity : SelectionType::None;
			m_Entity = entity;
			m_Asset = AssetHandle(0);
		}

		void SelectAsset(AssetHandle asset)
		{
			m_Type = static_cast<uint64_t>(asset) != 0
				? SelectionType::Asset
				: SelectionType::None;
			m_Asset = asset;
			m_Entity = Entity{};
		}

		void Clear()
		{
			m_Type = SelectionType::None;
			m_Entity = Entity{};
			m_Asset = AssetHandle(0);
		}

		SelectionType GetType() const { return m_Type; }
		Entity GetEntity() const { return m_Entity; }
		AssetHandle GetAsset() const { return m_Asset; }
		bool IsEntitySelected() const { return m_Type == SelectionType::Entity; }
		bool IsAssetSelected() const { return m_Type == SelectionType::Asset; }

	private:
		SelectionType m_Type = SelectionType::None;
		Entity m_Entity;
		AssetHandle m_Asset{ 0 };
	};
}
