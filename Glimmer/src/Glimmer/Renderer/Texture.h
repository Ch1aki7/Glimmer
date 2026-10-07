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

	struct TextureAllocationStatistics
	{
		uint64_t CurrentBytes = 0, PeakBytes = 0, AllocatedBytes = 0, ReleasedBytes = 0;
		uint64_t Allocations = 0, Releases = 0;
	};

	// Render-thread logical Texture2D events in a bounded operation; no driver queries.
	// The caller supplies its existing referenced storage. Nested scopes propagate events.
	class TextureAllocationScope
	{
	public:
		explicit TextureAllocationScope(uint64_t initialBytes = 0) : m_Previous(s_Active)
		{ m_Statistics.CurrentBytes = m_Statistics.PeakBytes = initialBytes; s_Active = this; }
		~TextureAllocationScope() { s_Active = m_Previous; }
		TextureAllocationScope(const TextureAllocationScope&) = delete;
		TextureAllocationScope& operator=(const TextureAllocationScope&) = delete;
		const TextureAllocationStatistics& GetStatistics() const { return m_Statistics; }
		static void RecordAllocation(uint64_t bytes) { Record(bytes, true); }
		static void RecordRelease(uint64_t bytes) { Record(bytes, false); }
	private:
		static void Record(uint64_t bytes, bool allocate)
		{
			if (!bytes) return;
			for (auto* scope = s_Active; scope; scope = scope->m_Previous) {
				auto& stats = scope->m_Statistics;
				if (allocate) {
					++stats.Allocations; stats.AllocatedBytes += bytes; stats.CurrentBytes += bytes;
					if (stats.CurrentBytes > stats.PeakBytes) stats.PeakBytes = stats.CurrentBytes;
				} else {
					++stats.Releases; stats.ReleasedBytes += bytes;
					stats.CurrentBytes = bytes > stats.CurrentBytes ? 0 : stats.CurrentBytes - bytes;
				}
			}
		}
		inline static thread_local TextureAllocationScope* s_Active = nullptr;
		TextureAllocationScope* m_Previous = nullptr;
		TextureAllocationStatistics m_Statistics;
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

		// Logical immutable storage, including opt-in Mips; excludes driver overhead.
		uint64_t GetStorageByteSize() const
		{
			if (!GetRendererID()) return 0;
			const auto& spec = GetSpecification();
			uint64_t bytesPerTexel = 0;
			switch (spec.Format) {
			case TextureFormat::R8: bytesPerTexel = 1; break;
			case TextureFormat::RGB8: bytesPerTexel = 3; break;
			case TextureFormat::RGBA8: case TextureFormat::RG16F: case TextureFormat::R32F: bytesPerTexel = 4; break;
			case TextureFormat::R16F: bytesPerTexel = 2; break;
			case TextureFormat::RGBA16F: bytesPerTexel = 8; break;
			default: return 0;
			}
			uint32_t width = spec.Width, height = spec.Height; uint64_t bytes = 0;
			while (width && height) {
				bytes += uint64_t(width) * height * bytesPerTexel;
				if (spec.MinFilter != TextureFilter::LinearMipmapLinear || (width == 1 && height == 1)) break;
				width = width > 1 ? width / 2 : 1; height = height > 1 ? height / 2 : 1;
			}
			return bytes;
		}
	};

	class Texture2D : public Texture {
	public:
		static Ref<Texture2D> Create(const std::string& path,
			TextureColorSpace colorSpace = TextureColorSpace::SRGB,
			TextureFilter minFilter = TextureFilter::Linear, TextureFilter magFilter = TextureFilter::Nearest);
		static Ref<Texture2D> Create(uint32_t width, uint32_t height);
		static Ref<Texture2D> Create(const TextureSpecification& specification);
	};

}
