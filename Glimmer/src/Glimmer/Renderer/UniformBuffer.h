#pragma once
#include "Glimmer/Core/Core.h"

namespace gl {

	// Uniform Buffer Object (UBO) — 跨 Shader 共享 uniform 数据
	class UniformBuffer {
	public:
		virtual ~UniformBuffer() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;

		static Ref<UniformBuffer> Create(uint32_t size, uint32_t binding);
	};

}
