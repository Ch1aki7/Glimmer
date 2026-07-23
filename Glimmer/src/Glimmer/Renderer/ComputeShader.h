#pragma once
#include "Glimmer/Core/Core.h"
#include "Glimmer/Renderer/ShaderReload.h"

#include <filesystem>
#include <string>

namespace gl {

	enum class ImageAccess { Read = 0, Write = 1, ReadWrite = 2 };
	enum class ImageFormat { RGBA8 = 0, RGBA16F = 1, RGBA32F = 2, R32F = 3 };

	class ComputeShader {
	public:
		virtual ~ComputeShader() = default;

		virtual void Bind() const = 0;
		virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z) const = 0;
		virtual void BindImageTexture(
			uint32_t binding,
			uint32_t textureID,
			uint32_t level,
			ImageAccess access,
			ImageFormat format) = 0;

		virtual const std::string& GetName() const = 0;
		virtual const std::filesystem::path& GetFilePath() const = 0;
		virtual uint64_t GetVersion() const = 0;
		virtual const ShaderReloadResult& GetLastReloadResult() const = 0;
		virtual ShaderReloadResult Reload() = 0;
		virtual ShaderReloadResult ReloadIfChanged() = 0;

		static void Barrier();
		static Ref<ComputeShader> Create(const std::string& filepath);
	};

}