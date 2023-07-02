#include "renderer/buffer.h"

#include "renderer/device.h"
#include "renderer/command_manager.h"

#include "core/core.h"
#include "core/log.h"


namespace gs {

	/* Tranfers of staging buffers for the update/resize functions should use the graphics queue
	 * graphics queues have lower latency, making it better suited for transfers that might get called between frames */
	static FORCEINLINE void transfer_buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkBufferCopy* bufferCopy, queue_family queueFamily = queue_family::transfer)
	{
		auto cmd = command_manager::get_cmd_buffer(queueFamily, std::this_thread::get_id());
		{
			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0x0, nullptr };
			vkBeginCommandBuffer(cmd, &beginInfo);

			vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, bufferCopy);

			vkEndCommandBuffer(cmd);
		}
		command_manager::submit(cmd, true);
	}

	base_cpu_buffer::base_cpu_buffer(size_t bufferSize, const void* data, size_t dataSize)
		: m_buffer_size(bufferSize)
	{
		m_buffer = (byte*) operator new[](sizeof(byte)* bufferSize, std::align_val_t{ 64ULL });
		memset(m_buffer, 0, bufferSize);

		if (data)
		{
			assert(dataSize);

			memcpy((void*)m_buffer, data, dataSize);
			m_used_buffer_size = dataSize;
		}
	}

	base_cpu_buffer::~base_cpu_buffer()
	{
		::operator delete[](m_buffer, std::align_val_t{ 64ULL });
	}

	void* base_cpu_buffer::write(const void* srcData, size_t dataSize, size_t offset)
	{
		assert(m_buffer_size > 0); // make sure buffer is initialized
		assert(srcData && dataSize);
		assert(offset < m_buffer_size&& dataSize <= m_buffer_size - offset);

		byte* loc = (byte*)m_buffer;
		loc += offset;
		memcpy((void*)loc, srcData, dataSize);

		return (void*)loc;
	}

	void* base_cpu_buffer::resize(size_t newSize, bool keepOldData)
	{
		LOG_ENGINE(trace, "Resizing cpu only buffer");

		byte* newBuffer = (byte*) operator new[](sizeof(byte) * newSize, std::align_val_t{ 64ULL });
		memset(newBuffer, 0, newSize);

		if (keepOldData)
		{
			memcpy((void*)newBuffer, (void*)m_buffer, m_buffer_size);
		}
		else
		{
			m_used_buffer_size = 0;
		}

		::operator delete[](m_buffer, std::align_val_t{ 64ULL });

		m_buffer = newBuffer;
		m_buffer_size = newSize;

		return m_buffer;
	}


	base_buffer::~base_buffer()
	{
		if(m_buffer && m_buffer_memory)
			memory_manager::destroy_buffer(m_buffer, m_buffer_memory);
	}

	base_buffer::base_buffer(base_buffer&& other) noexcept
		: m_buffer(other.m_buffer), m_buffer_memory(other.m_buffer_memory), m_buffer_size(other.m_buffer_size),
		  m_used_buffer_size(other.m_used_buffer_size), m_usage(other.m_usage), m_memory_usage(other.m_memory_usage)
	{
		LOG_ENGINE(warn, "move-constructing buffer");

		/* reset other's member variables */
		other.m_buffer = VK_NULL_HANDLE;
		other.m_buffer_memory = VK_NULL_HANDLE;
		other.m_buffer_size = 0;
		other.m_used_buffer_size = 0;
		other.m_usage = 0;
		/* m_memory_usage usage is constant */
	}

	base_buffer& base_buffer::operator=(base_buffer&& other) noexcept
	{
		LOG_ENGINE(warn, "move-assigning buffer");

		m_buffer = other.m_buffer;
		m_buffer_memory = other.m_buffer_memory;
		m_buffer_size = other.m_buffer_size;
		m_used_buffer_size = other.m_used_buffer_size;
		m_usage = other.m_usage;
		m_memory_usage = other.m_memory_usage;

		/* reset other's member variables */
		other.m_buffer = VK_NULL_HANDLE;
		other.m_buffer_memory = VK_NULL_HANDLE;
		other.m_buffer_size = 0;
		other.m_used_buffer_size = 0;
		other.m_usage = 0;
		/* m_memory_usage usage is constant */

		return *this;
	}

	base_device_only_buffer::base_device_only_buffer(size_t size, VkBufferUsageFlags flags, const void* data, size_t dataSize)
		: base_buffer(size, flags, VMA_MEMORY_USAGE_GPU_ONLY) /* device::integrated() ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY */
	{
		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = (uint32_t)m_buffer_size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | m_usage;
		m_buffer_memory = memory_manager::create_buffer(bufferCreateInfo, &m_buffer, VMA_MEMORY_USAGE_GPU_ONLY);

		if (data)
		{
			/* size can't be zero */
			assert(dataSize);

			VkBuffer stagingBuffer = VK_NULL_HANDLE;
			VkBufferCreateInfo stagingBufferCreateInfo{};
			stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferCreateInfo.size = (uint64_t)dataSize;
			stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocation stagingbufferMemory = memory_manager::create_buffer(stagingBufferCreateInfo, &stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
			void* dstData = nullptr;

			memory_manager::map(&dstData, stagingbufferMemory);
			memcpy(dstData, data, dataSize);
			memory_manager::unmap(stagingbufferMemory);

			VkBufferCopy bufferCopy{ 0, 0, (uint64_t)dataSize };
			transfer_buffer(stagingBuffer, m_buffer, &bufferCopy, queue_family::transfer);

			memory_manager::destroy_buffer(stagingBuffer, stagingbufferMemory);
			m_used_buffer_size = dataSize;
		}
	}

	void base_device_only_buffer::write(const void* srcData, size_t dataSize, size_t offset /*= 0*/)
	{
		/* make sure buffer is initialized */
		assert(m_buffer_size > 0);
		assert(srcData && dataSize);
		assert(offset < m_buffer_size && dataSize <= m_buffer_size - offset);

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkBufferCreateInfo stagingBufferCreateInfo{};
		stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferCreateInfo.size = dataSize;
		stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocation stagingbufferMemory = memory_manager::create_buffer(stagingBufferCreateInfo, &stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
		void* dstData = nullptr;

		memory_manager::map(&dstData, stagingbufferMemory);
		memcpy(dstData, srcData, dataSize);
		memory_manager::unmap(stagingbufferMemory);

		VkBufferCopy bufferCopy{ 0, (uint64_t)offset, (uint64_t)dataSize };
		transfer_buffer(stagingBuffer, m_buffer, &bufferCopy, queue_family::graphics);

		memory_manager::destroy_buffer(stagingBuffer, stagingbufferMemory);
	}

	void base_device_only_buffer::resize(size_t newSize, bool keepOldData)
	{
		LOG_ENGINE(trace, "resizing gpu only buffer | old size == %zu, new size == %zu", m_buffer_size, newSize);

		VkBuffer newBuffer = VK_NULL_HANDLE;
		VmaAllocation newBufferMemory = VK_NULL_HANDLE;

		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = (uint32_t)newSize;

		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | m_usage;
		newBufferMemory = memory_manager::create_buffer(bufferCreateInfo, &newBuffer, m_memory_usage);

		if (keepOldData && m_used_buffer_size)
		{
			VkBufferCopy bufferCopy{ 0, 0, (uint64_t)m_used_buffer_size }; // m_buffer_size
			transfer_buffer(m_buffer, newBuffer, &bufferCopy, queue_family::graphics);
		}
		else
		{
			/*  reset current location */
			m_used_buffer_size = 0;
		}

		if(m_buffer && m_buffer_memory)
			memory_manager::destroy_buffer(m_buffer, m_buffer_memory);

		m_buffer = newBuffer;
		m_buffer_memory = newBufferMemory;
		m_buffer_size = newSize;
	}

	void base_device_only_buffer::copy(const base_device_only_buffer& inBuffer, size_t size, size_t srcOffset, size_t dstOffset)
	{
		/* make sure buffer is initialized */
		assert(m_buffer_size > 0);
		assert(dstOffset < m_buffer_size);

		/* check if resize needed */
		if (m_buffer_size - dstOffset < size)
		{
			size_t newSize = (size_t)((float)(m_buffer_size - dstOffset + size) * 1.5f);
			resize(newSize, true);
		}

		VkBufferCopy bufferCopy{ srcOffset, dstOffset, (uint64_t)size };
		transfer_buffer(inBuffer.get(), m_buffer, &bufferCopy, queue_family::graphics);
	}

	base_host_visible_device_buffer::base_host_visible_device_buffer(size_t size, VkBufferUsageFlags flags, VmaMemoryUsage memUsage, const void* data, size_t dataSize)
		: base_buffer(size, flags, memUsage)
	{
		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = (uint32_t)m_buffer_size;
		bufferCreateInfo.usage = m_usage;
		m_buffer_memory = memory_manager::create_buffer(bufferCreateInfo, &m_buffer, m_memory_usage);

		memory_manager::map(&m_buffer_location, m_buffer_memory);

		if (data)
		{
			/* size can't be zero */
			assert(dataSize);

			memcpy(m_buffer_location, data, dataSize);

			m_used_buffer_size = dataSize;
		}

		/* better to leave mapped (for updates) and unmap only at destruction */
		//memory_manager::unmap(m_buffer_memory);
	}

	base_host_visible_device_buffer::~base_host_visible_device_buffer()
	{
		if(m_buffer_memory)
			memory_manager::unmap(m_buffer_memory);
	}

	base_host_visible_device_buffer::base_host_visible_device_buffer(base_host_visible_device_buffer&& other) noexcept
		: base_buffer(std::move(other)), m_buffer_location(other.m_buffer_location)
	{
		other.m_buffer_location = nullptr;
	}

	base_host_visible_device_buffer& base_host_visible_device_buffer::operator=(base_host_visible_device_buffer&& other) noexcept
	{
		base_buffer::operator=(std::move(other));
		m_buffer_location = other.m_buffer_location;

		other.m_buffer_location = nullptr;

		return *this;
	}

	void* base_host_visible_device_buffer::write(const void* srcData, size_t dataSize, size_t offset)
	{
		/* make sure buffer is initialized */
		assert(m_buffer_size > 0);
		assert(srcData && dataSize);
		assert(offset < m_buffer_size && dataSize <= m_buffer_size - offset);

		byte* loc = (byte*)m_buffer_location;
		loc += offset;
		memcpy((void*)loc, srcData, dataSize);

		return (void*)loc;
	}

	void* base_host_visible_device_buffer::resize(size_t newSize, bool keepOldData)
	{
		LOG_ENGINE(trace, "Resizing buffer");

		VkBuffer newBuffer = VK_NULL_HANDLE;
		VmaAllocation newBufferMemory = VK_NULL_HANDLE;

		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = (uint32_t)newSize;
		bufferCreateInfo.usage = m_usage;

		newBufferMemory = memory_manager::create_buffer(bufferCreateInfo, &newBuffer, m_memory_usage);
		void* newBufferLocation = nullptr;
		memory_manager::map(&newBufferLocation, newBufferMemory);

		if (keepOldData && m_used_buffer_size)
		{
			memcpy(newBufferLocation, m_buffer_location, m_buffer_size); //m_used_buffer_size
		}
		else
		{
			/* reset current location */
			m_used_buffer_size = 0;
		}

		m_buffer_location = newBufferLocation;

		if(m_buffer_memory)
		{
			memory_manager::unmap(m_buffer_memory);

			if(m_buffer)
				memory_manager::destroy_buffer(m_buffer, m_buffer_memory);
		}

		m_buffer = newBuffer;
		m_buffer_memory = newBufferMemory;
		m_buffer_size = newSize;

		return m_buffer_location;
	}

	void base_host_visible_device_buffer::copy(const base_host_visible_device_buffer& inBuffer, size_t size, size_t srcOffset, size_t dstOffset)
	{
		assert(m_buffer_size > 0); // make sure buffer is initialized
		assert(dstOffset < m_buffer_size);

		/* check if resize needed */
		if (m_buffer_size - dstOffset < size)
		{
			size_t newSize = (size_t)((float)(m_buffer_size - dstOffset + size) * 1.5f);
			resize(newSize, true);
		}

		byte* dstBuffer = (byte*)m_buffer_location;
		byte* srcBuffer = (byte*)inBuffer.data();
		dstBuffer += dstOffset;
		srcBuffer += srcOffset;
		memcpy((void*)dstBuffer, (void*)srcBuffer, size);
	}

#ifdef VULKAN_GLSL_1_2
	VkDeviceAddress buffer::GetDeviceAddress()
	{
		if (m_usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		{
			VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_buffer };
			return vkGetBufferDeviceAddress(device::get_logical(), &addressInfo);
		}

		LOG_ENGINE(warn, "Device memory requested from a buffer that was not created with the VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT flag, returning 0");
		return 0ULL;
	}
#endif

}