#include "core/cmd_queue.h"

#include "core/log.h"

namespace gs {

	static constexpr size_t BUFFER_ALIGMENT = 8ULL;

	cmd_queue::cmd_queue(size_t cmdQueueInitSize)
		: m_capacity(cmdQueueInitSize), m_cmd_count(0UL)
	{
		/* start command queue at the beginning of a cacheline(on 64-bit) */
		m_buffer = (byte*) operator new[](sizeof(byte)* cmdQueueInitSize, std::align_val_t{ 64ULL });
		m_queue_ptr = m_buffer;

		memset(m_buffer, 0, cmdQueueInitSize);

		LOG_ENGINE(trace, "cmd_queue resized, new capacity == %zu", cmdQueueInitSize);
	}

	cmd_queue::~cmd_queue()
	{
		::operator delete[](m_buffer, std::align_val_t{ 64ULL });
	}

	cmd_queue::cmd_queue(cmd_queue&& other) noexcept
	{
		m_capacity = other.m_capacity;
		m_size = other.m_size;
		m_cmd_count = other.m_cmd_count;
		m_buffer = other.m_buffer;
		m_queue_ptr = m_buffer;

		other.m_capacity = 0;
		other.m_size = 0;
		other.m_cmd_count = 0;
		other.m_buffer = nullptr;
		other.m_queue_ptr = nullptr;
	}

	cmd_queue& cmd_queue::operator=(cmd_queue&& other) noexcept
	{
		m_capacity = other.m_capacity;
		m_size = other.m_size;
		m_cmd_count = other.m_cmd_count;
		m_buffer = other.m_buffer;
		m_queue_ptr = m_buffer;

		other.m_capacity = 0;
		other.m_size = 0;
		other.m_cmd_count = 0;
		other.m_buffer = nullptr;
		other.m_queue_ptr = nullptr;

		return *this;
	}

	void cmd_queue::resize(size_t newCapacity)
	{
		if (newCapacity <= m_capacity)
			return;

		/* start command queue at the beginning of a cacheline(on 64 - bit) */
		byte* newBuffer = (byte*) operator new[](sizeof(byte)* newCapacity, std::align_val_t{ 64ULL });
		byte* newQueuePtr = newBuffer;

		memset(newBuffer, 0, newCapacity);

		if (m_capacity) /* has other data */
		{
			if (m_size)
			{
				memcpy(newBuffer, m_buffer, m_size);
			}

			::operator delete[](m_buffer, std::align_val_t{ 64ULL });
		}

		m_buffer = newBuffer;
		m_queue_ptr = m_buffer + m_size;
		m_capacity = newCapacity;

		LOG_ENGINE(trace, "cmd_queue resized, new capacity == %zu", newCapacity);
	}

	void* cmd_queue::enqueue(cmd_fn cmdFn, uint32_t size)
	{
		/* object structure
		 * 0 | total size of the object including padding if any [ always sizeof(uint32_t)]
		 * 1 | function [always sizeof of a function pointer]
		 * 2 | size of function args/parameters [ always sizeof(uint32_t) ]
		 * 3 | args/parameters [variable size, mostly lambda captures / sizeof(functor)]
		 * 4 | padding for aligment (if any) [size from 0 to BUFFER_ALIGMENT -1 bytes]
		 */

		size_t baseSize = sizeof(cmd_fn) + sizeof(uint32_t) /*args size*/ + sizeof(uint32_t) /* total size */ + size;
		size_t padding = baseSize % BUFFER_ALIGMENT;
		size_t totalSize = baseSize + padding;

		if ((m_size + totalSize) > m_capacity)
		{
			resize(m_capacity + (m_capacity / 2));
		}

		*(uint32_t*)m_queue_ptr = (uint32_t)totalSize;
		m_queue_ptr += sizeof(uint32_t);

		*(cmd_fn*)m_queue_ptr = cmdFn;
		m_queue_ptr += sizeof(cmd_fn);

		*(uint32_t*)m_queue_ptr = size;
		m_queue_ptr += sizeof(uint32_t);

		void* argsLocation = m_queue_ptr;
		m_queue_ptr += size;

		/* add pad (if any) */
		m_queue_ptr += padding;

		m_cmd_count++;
		m_size += totalSize;

		return argsLocation;
	}

	void cmd_queue::dequeue_all()
	{
		uint8_t* localPtr = m_buffer;

		uint32_t baseSize = sizeof(cmd_fn) + sizeof(uint32_t) /*args size*/ + sizeof(uint32_t) /* total size */;

		for (uint32_t i = 0; i < m_cmd_count; i++)
		{
			/* size of this block */
			localPtr += sizeof(uint32_t);

			cmd_fn function = *(cmd_fn*)localPtr;
			localPtr += sizeof(cmd_fn);

			uint32_t size = *(uint32_t*)localPtr;
			localPtr += sizeof(uint32_t);
			function(localPtr);
			localPtr += size;

			/* padding (if any) */
			localPtr += (size + baseSize) % BUFFER_ALIGMENT;
		}

		/* reset command queue */
		m_queue_ptr = m_buffer;
		m_cmd_count = 0;
		m_size = 0;
	}

}
