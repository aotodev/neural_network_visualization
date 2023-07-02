#include "core/uuid.h"

namespace gs {

	static std::random_device s_random_device;
	static std::mt19937_64 s_engine(s_random_device());
	static std::uniform_int_distribution<uint64_t> s_uiniform_distribution;

	uuid::uuid() : uuid(s_uiniform_distribution(s_engine)) {}
	uuid::uuid(uint64_t uniqueID) : m_uuid(uniqueID) {}
}