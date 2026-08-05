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
		Roughness = 1u << 4
	};

	struct MaterialOverrides
	{
		uint32_t Mask = 0;
		MaterialProperties Values;

		bool IsEnabled(MaterialOverride property) const;
		void SetEnabled(MaterialOverride property, bool enabled);
		void Clear();
		bool Empty() const { return Mask == 0; }
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
