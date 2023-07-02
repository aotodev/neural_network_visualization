#pragma once

#include "core/core.h"

namespace gs {

	/* only thread-safe if there is a single producer thread and single consumer thread */

	class cmd_queue
	{
	public:
		typedef void(*cmd_fn)(void*);

		cmd_queue() = default;
		cmd_queue(size_t commandQueueInitialSize);
		~cmd_queue();

		cmd_queue(const cmd_queue&)  = delete;
		cmd_queue& operator=(const cmd_queue&) = delete;

		cmd_queue(cmd_queue&&) noexcept;
		cmd_queue& operator=(cmd_queue&&) noexcept;

		void* enqueue(cmd_fn cmdFn, uint32_t size);
		void dequeue_all();

		void resize(size_t newCapacity);

		bool empty() const { return !(bool)m_cmd_count; }
		bool has_work() const { return (bool)m_cmd_count; }

		size_t capacity() const { return m_capacity; }
		size_t size() const { return m_size; }
		uint32_t count() const { return m_cmd_count; }

		const byte* data() const { return m_buffer; }

		operator bool() const { return m_buffer != nullptr; }

	private:
		size_t m_capacity = 0;
		size_t m_size = 0;

		uint32_t m_cmd_count = 0; 

		byte* m_buffer = nullptr;
		byte* m_queue_ptr = nullptr;
	};
}