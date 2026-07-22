#pragma once
#include "Glimmer/Core/Core.h"
#include <string>
#include <glm/glm.hpp>

namespace gl {

	// Compute Shader 图像访问模式
	enum class ImageAccess { Read = 0, Write = 1, ReadWrite = 2 };

	// Compute Shader 图像格式
	enum class ImageFormat { RGBA8 = 0, RGBA16F = 1, RGBA32F = 2, R32F = 3 };

	class ComputeShader {
	public:
		virtual ~ComputeShader() = default;

		virtual void Bind() const = 0;
		virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z) const = 0;

		virtual const std::string& GetName() const = 0;

		virtual void BindImageTexture(uint32_t binding, uint32_t textureID, uint32_t level, ImageAccess access, ImageFormat format) = 0;

		// GPU 内存屏障：确保 Compute 写入对后续渲染/读取可见
		static void Barrier();

		static Ref<ComputeShader> Create(const std::string& filepath);
	};

}
