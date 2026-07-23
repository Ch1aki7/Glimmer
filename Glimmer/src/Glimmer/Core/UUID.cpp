#include "glpch.h"
#include "UUID.h"

#include <mutex>
#include <random>

namespace gl {

	namespace {

		std::random_device s_RandomDevice;
		std::mt19937_64 s_Engine(s_RandomDevice());
		std::uniform_int_distribution<uint64_t> s_UniformDistribution;
		std::mutex s_UUIDMutex;

	}

	UUID::UUID()
	{
		std::lock_guard<std::mutex> lock(s_UUIDMutex);
		m_UUID = s_UniformDistribution(s_Engine);
		while (m_UUID == 0)
			m_UUID = s_UniformDistribution(s_Engine);
	}

	UUID::UUID(uint64_t uuid)
		: m_UUID(uuid)
	{
	}

}