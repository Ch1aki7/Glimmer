#pragma once
#include <string>
#include "Glimmer/Core/Core.h"

namespace gl {

	class Texture
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		// slot 代表纹理单元（0-31），显卡可以同时绑定多个纹理
		virtual void Bind(uint32_t slot = 0) const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(const std::string& path);
	};

}
