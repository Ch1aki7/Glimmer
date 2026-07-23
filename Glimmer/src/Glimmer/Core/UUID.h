#pragma once

#include <cstdint>
#include <functional>

namespace gl {

	class UUID
	{
	public:
		UUID();
		explicit UUID(uint64_t uuid);
		UUID(const UUID&) = default;

		operator uint64_t() const { return m_UUID; }

		bool operator==(const UUID& other) const { return m_UUID == other.m_UUID; }
		bool operator!=(const UUID& other) const { return !(*this == other); }

	private:
		uint64_t m_UUID = 0;
	};

}

namespace std {

	template<>
	struct hash<gl::UUID>
	{
		size_t operator()(const gl::UUID& uuid) const noexcept
		{
			return hash<uint64_t>()(static_cast<uint64_t>(uuid));
		}
	};

}