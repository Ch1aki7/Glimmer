#pragma once

#include "Glimmer/Renderer/Material.h"

#include <cstdint>

namespace gl {

	enum class MaterialOverride : uint32_t
	{
		BaseColor = 1u << 0,
		BaseColorTexture = 1u << 1,
		TilingFactor = 1u << 2,
		Metallic = 1u << 3,
		Roughness = 1u << 4,
		AlphaMode = 1u << 5,
		AlphaCutoff = 1u << 6,
		NormalTexture = 1u << 7,
		AOTexture = 1u << 8,
		EmissiveTexture = 1u << 9,
		NormalScale = 1u << 10,
		AOStrength = 1u << 11,
		EmissiveColor = 1u << 12,
		EmissiveStrength = 1u << 13
	};

	struct MaterialOverrides
	{
		uint32_t Mask = 0;
		MaterialProperties Values;

		bool IsEnabled(MaterialOverride property) const;
		void SetEnabled(MaterialOverride property, bool enabled);
		void Clear();
		bool Empty() const { return Mask == 0; }
		uint64_t GetVersion() const { return m_Version; }
		void MarkDirty() { ++m_Version; }
		bool operator==(const MaterialOverrides& other) const
		{
			return Mask == other.Mask && Values == other.Values;
		}
		bool operator!=(const MaterialOverrides& other) const { return !(*this == other); }

	private:
		uint64_t m_Version = 1;
	};

	class MaterialInstance
	{
	public:
		MaterialInstance(
			const Ref<Material>& material,
			const MaterialOverrides& overrides = {});

		bool IsValid() const { return static_cast<bool>(m_Material); }
		AssetHandle GetShaderHandle() const;
		const MaterialProperties& GetProperties() const { return m_Properties; }

	private:
		Ref<Material> m_Material;
		MaterialProperties m_Properties;
	};

}
