#pragma once

#include "Glimmer/Core/Core.h"
#include "Glimmer/Renderer/TextureCube.h"

#include <filesystem>

namespace gl {

	class Cubemap
	{
	public:
		static Ref<Cubemap> Create(const std::filesystem::path& descriptorPath);

		bool Reload();

		const std::filesystem::path& GetPath() const { return m_Path; }
		const Ref<TextureCube>& GetTexture() const { return m_Texture; }

	private:
		explicit Cubemap(std::filesystem::path descriptorPath);

	private:
		std::filesystem::path m_Path;
		Ref<TextureCube> m_Texture;
	};

}
