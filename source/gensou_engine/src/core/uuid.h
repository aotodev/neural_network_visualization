#pragma once

namespace gs {

	class uuid
	{
	public:
		uuid();
		uuid(uint64_t uniqueID);

		operator uint64_t() const { return m_uuid; }

	private:
		uint64_t m_uuid;

	};
}

namespace std
{
	template<>
	struct hash<gs::uuid>
	{
		std::size_t operator()(const gs::uuid& uniqueID) const
		{
			return hash<uint64_t>()((uint64_t)uniqueID);
		}
	};
}