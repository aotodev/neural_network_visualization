#pragma once

#include "core/log.h"
#include "renderer/memory_manager.h"

#include <vulkan/vulkan.h>

namespace gs {

	/* not a vulkan buffer at all, always allocated on host memory */
	class base_cpu_buffer
	{
	public:
		void* data() { return (void*)m_buffer; }
		size_t size() const { return m_used_buffer_size; }
		size_t capacity() const { return m_buffer_size; }

		void* write(const void* srcData, size_t dataSize, size_t offset);
		void* resize(size_t newSize, bool keepOldData = true);
		void reset() { m_used_buffer_size = 0; }

		// builds an object in-place and returns a pointer to it
		template<typename T, typename... Args>
		T* emplace(size_t offset, Args&&... args)
		{
			size_t size = sizeof(T);

			if (m_buffer_size < offset + size)
			{
				LOG_ENGINE(info, "resizing buffer to emplace new object");
				size_t newSize = std::max((size_t)((float)m_buffer_size * 1.5f), m_buffer_size + offset + size);
				resize(newSize, true);
			}

			byte* loc = m_buffer;
			loc += offset;

			new((void*)loc) T(std::forward<Args>(args)...);

			return (T*)loc;
		}

	protected:
		base_cpu_buffer(size_t bufferSize, const void* data = nullptr, size_t dataSize = 0);
		virtual ~base_cpu_buffer();

		byte* m_buffer = nullptr;
		size_t m_buffer_size = 0;
		size_t m_used_buffer_size = 0;
	};

	/* vulkan buffer, could be either on host or device memory depending on the memory type */
	class base_buffer
	{
	protected:
		base_buffer(size_t bufferSize, VkBufferUsageFlags flags, VmaMemoryUsage memoryUsage)
			: m_buffer_size(bufferSize), m_usage(flags), m_memory_usage(memoryUsage) {}

		base_buffer(VkBufferUsageFlags flags, VmaMemoryUsage memoryUsage)
			: m_usage(flags), m_memory_usage(memoryUsage) {}

	public:
		virtual ~base_buffer();

		/* we don't want to accidentally spam a bunch of gpu memory alocations */
		base_buffer(const base_buffer&) = delete;
		base_buffer& operator=(const base_buffer&) = delete;

		/* but we are able to move it */
		base_buffer(base_buffer&&) noexcept;
		base_buffer& operator=(base_buffer&&) noexcept;

		VkBuffer get() const { return m_buffer; }

		/* returns total used memory size (from the allocated total buffer) in bytes */
		size_t size() const { return m_used_buffer_size; }
		size_t capacity() const { return m_buffer_size; }

		VkBufferUsageFlags usage_flags() const { return m_usage; }
		VmaMemoryUsage memory_usage() const { return m_memory_usage; }

	protected:
		VkBuffer m_buffer = VK_NULL_HANDLE;
		VmaAllocation m_buffer_memory = VK_NULL_HANDLE;

		size_t m_buffer_size = 0;
		size_t m_used_buffer_size = 0;

		VkBufferUsageFlags m_usage;
		VmaMemoryUsage m_memory_usage;
	};

	class base_device_only_buffer : public base_buffer
	{
	public:
		virtual ~base_device_only_buffer() = default;	

		/* returns offset to the data in the buffer in bytes */
		void write(const void* srcData, size_t dataSize, size_t offset);
		void reset() { m_used_buffer_size = 0; }

		void resize(size_t newSize, bool keepOldData = true);
		void copy(const base_device_only_buffer& buffer, size_t size, size_t srcOffset, size_t dstOffset);

		template<typename T, typename... Args>
		void emplace(size_t offset, Args&&... args)
		{
			size_t size = sizeof(T);
			auto object = new T(std::forward<Args>(args)...);
			update(object, size, offset);
			delete object;
		}

	protected:
		base_device_only_buffer(size_t size, VkBufferUsageFlags flags, const void* data, size_t dataSize);
		base_device_only_buffer(VkBufferUsageFlags flags) : base_buffer(flags, VMA_MEMORY_USAGE_GPU_ONLY) {}
	};

	class base_host_visible_device_buffer : public base_buffer
	{
	public:
		virtual ~base_host_visible_device_buffer();

		base_host_visible_device_buffer(const base_host_visible_device_buffer&) = delete;
		base_host_visible_device_buffer& operator=(const base_host_visible_device_buffer&) = delete;

		base_host_visible_device_buffer(base_host_visible_device_buffer&&) noexcept;
		base_host_visible_device_buffer& operator=(base_host_visible_device_buffer&&) noexcept;

		void* data() { return m_buffer_location; }
		const void* data() const { return m_buffer_location; }

		void* read(size_t offset) { return (void*)((byte*)m_buffer_location + offset); }
		const void* read(size_t offset) const { return (void*)((byte*)m_buffer_location + offset); }

		void* write(const void* srcData, size_t dataSize, size_t offset);

		/* returns a pointer to the new buffer location in memory */
		void* resize(size_t newSize, bool keepOldData = true);
		void reset() { m_used_buffer_size = 0; }
		void copy(const base_host_visible_device_buffer& inBuffer, size_t size, size_t srcOffset, size_t dstOffset);

		/* builds an object in-place and returns a pointer to it */
		template<typename T, typename... Args>
		T* emplace(size_t offset, Args&&... args)
		{
			size_t size = sizeof(T);

			if (m_buffer_size < offset + size)
			{
				LOG_ENGINE(info, "resizing buffer to emplace new object");
				size_t newSize = std::max((size_t)((float)m_buffer_size * 1.5f), m_buffer_size + offset + size);
				resize(newSize, true);
			}

			byte* loc = (byte*)m_buffer_location;
			loc += offset;

			new((void*)loc) T(std::forward<Args>(args)...);

			return (T*)loc;
		}

	protected:
		base_host_visible_device_buffer(size_t size, VkBufferUsageFlags flags, VmaMemoryUsage memUsage, const void* data, size_t dataSize);
		base_host_visible_device_buffer(VkBufferUsageFlags flags, VmaMemoryUsage memUsage) : base_buffer(flags, memUsage) {}

	private:
		void* m_buffer_location = nullptr;
	};

	template<memory_type mem>
	class buffer : public base_host_visible_device_buffer
	{
	public:
		buffer(VkBufferUsageFlags flags)
			: base_host_visible_device_buffer(flags, VMA_MEMORY_USAGE_CPU_TO_GPU) {} 

		buffer(size_t size, VkBufferUsageFlags flags, const void* data, size_t dataSize)
			: base_host_visible_device_buffer(size, flags, VMA_MEMORY_USAGE_CPU_TO_GPU, data, dataSize)
		{}
	};

	template<>
	class buffer<no_vma_cpu> : public base_cpu_buffer
	{
	public:
		buffer(size_t size)
			: base_cpu_buffer(size) {}

		buffer(size_t size, const void* data, size_t dataSize)
			: base_cpu_buffer(size, data, dataSize) {}
	};

	template<>
	class buffer<cpu_only> : public base_host_visible_device_buffer
	{
	public:
		buffer(VkBufferUsageFlags flags)
			: base_host_visible_device_buffer(flags, VMA_MEMORY_USAGE_CPU_ONLY) {}

		buffer(size_t size, VkBufferUsageFlags flags, const void* data, size_t dataSize)
			: base_host_visible_device_buffer(size, flags, VMA_MEMORY_USAGE_CPU_ONLY, data, dataSize)
		{}
	};

	template<>
	class buffer<gpu_to_cpu> : public base_host_visible_device_buffer
	{
	public:
		buffer(VkBufferUsageFlags flags)
			: base_host_visible_device_buffer(flags, VMA_MEMORY_USAGE_GPU_TO_CPU) {}

		buffer(size_t size, VkBufferUsageFlags flags, const void* data, size_t dataSize)
			: base_host_visible_device_buffer(size, flags, VMA_MEMORY_USAGE_GPU_TO_CPU, data, dataSize)
		{}
	};

	template<>
	class buffer<gpu_only> : public base_device_only_buffer
	{
	public:
		buffer(VkBufferUsageFlags flags)
			: base_device_only_buffer(flags) {}

		buffer(size_t size, VkBufferUsageFlags flags, const void* data, size_t dataSize)
			: base_device_only_buffer(size, flags, data, dataSize)
		{}
	};

	template<>
	class buffer<gpu_lazy>
	{
		buffer() { assert(false); }
		
		/* this specialization should not be called, use memory_manager::create_image for creating a VkImage */
	};

}
