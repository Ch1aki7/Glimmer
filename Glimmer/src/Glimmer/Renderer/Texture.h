#pragma once

#include "Glimmer/Core/Core.h"
#include "Glimmer/Asset/TextureAssetMetadata.h"

#include <glm/glm.hpp>
#include <string>

namespace gl {

	enum class TextureFormat {
		None = 0,
		R8,
		RGB8,
		RGBA8,
		R16F,
		RG16F,
		RGBA16F,
		R32F
	};

	enum class TextureFilter
	{
		Nearest = 0,
		Linear,
		LinearMipmapLinear
	};
	enum class TextureWrap { Repeat = 0, ClampToEdge, MirroredRepeat };

	enum class TextureUsage : uint32_t {
		None = 0,
		Sampled = BIT(0),
		Storage = BIT(1),
		RenderTarget = BIT(2),
		Readback = BIT(3)
	};

	inline TextureUsage operator|(TextureUsage left, TextureUsage right)
	{
		return static_cast<TextureUsage>(
			static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
	}

	struct TextureSpecification {
		uint32_t Width = 1;
		uint32_t Height = 1;
		TextureFormat Format = TextureFormat::RGBA8;
		TextureFilter MinFilter = TextureFilter::Linear;
		TextureFilter MagFilter = TextureFilter::Nearest;
		TextureWrap WrapS = TextureWrap::Repeat;
		TextureWrap WrapT = TextureWrap::Repeat;
		TextureUsage Usage = TextureUsage::Sampled;
		TextureColorSpace ColorSpace = TextureColorSpace::Linear;
	};

	class Texture {
	public:
		virtual ~Texture() = default;

		virtual const TextureSpecification& GetSpecification() const = 0;
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual TextureFormat GetFormat() const = 0;

		virtual void SetData(const void* data, uint32_t size) = 0;
		virtual void GetImageData(void* buffer, uint32_t size) const = 0;
		virtual void Clear(const glm::vec4& value) = 0;
		virtual void Bind(uint32_t slot = 0) const = 0;

		virtual bool operator==(const Texture& other) const = 0;
		virtual uint32_t GetRendererID() const = 0;
	};

	class Texture2D : public Texture {
	public:
		static Ref<Texture2D> Create(const std::string& path,
			TextureColorSpace colorSpace = TextureColorSpace::SRGB);
		static Ref<Texture2D> Create(uint32_t width, uint32_t height);
		static Ref<Texture2D> Create(const TextureSpecification& specification);
	};

}
