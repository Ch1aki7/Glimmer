#include "glpch.h"
#include "MaterialInstance.h"

namespace gl {

	namespace {

		uint32_t ToMask(MaterialOverride property)
		{
			return static_cast<uint32_t>(property);
		}

	}

	bool MaterialOverrides::IsEnabled(MaterialOverride property) const
	{
		return (Mask & ToMask(property)) != 0;
	}

	void MaterialOverrides::SetEnabled(MaterialOverride property, bool enabled)
	{
		const uint32_t previousMask = Mask;
		if (enabled)
			Mask |= ToMask(property);
		else
			Mask &= ~ToMask(property);
		if (Mask != previousMask)
			MarkDirty();
	}

	void MaterialOverrides::Clear()
	{
		if (Mask == 0 && Values == MaterialProperties{})
			return;
		Mask = 0;
		Values = MaterialProperties{};
		MarkDirty();
	}

	MaterialInstance::MaterialInstance(
		const Ref<Material>& material,
		const MaterialOverrides& overrides)
		: m_Material(material)
	{
		if (!m_Material)
			return;

		m_Properties = m_Material->GetProperties();
		if (overrides.IsEnabled(MaterialOverride::BaseColor))
			m_Properties.BaseColor = overrides.Values.BaseColor;
		if (overrides.IsEnabled(MaterialOverride::BaseColorTexture))
			m_Properties.BaseColorTexture = overrides.Values.BaseColorTexture;
		if (overrides.IsEnabled(MaterialOverride::TilingFactor))
			m_Properties.TilingFactor = glm::max(overrides.Values.TilingFactor, 0.01f);
		if (overrides.IsEnabled(MaterialOverride::Metallic))
			m_Properties.Metallic = glm::clamp(overrides.Values.Metallic, 0.0f, 1.0f);
		if (overrides.IsEnabled(MaterialOverride::Roughness))
			m_Properties.Roughness = glm::clamp(overrides.Values.Roughness, 0.04f, 1.0f);
	}

	AssetHandle MaterialInstance::GetShaderHandle() const
	{
		return m_Material ? m_Material->GetShaderHandle() : AssetHandle(0);
	}

}
