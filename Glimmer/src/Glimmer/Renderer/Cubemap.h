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
		const std::filesystem::path& GetSourcePath() const
		{
			return m_SourcePath;
		}
		bool IsHDR() const { return m_IsHDR; }
		uint64_t GetVersion() const { return m_Version; }

	private:
		explicit Cubemap(std::filesystem::path descriptorPath);

	private:
		std::filesystem::path m_Path;
		std::filesystem::path m_SourcePath;
		Ref<TextureCube> m_Texture;
		uint64_t m_Version = 0;
		bool m_IsHDR = false;
	};

}
