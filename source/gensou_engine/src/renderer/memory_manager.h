#pragma once

#include "core/core.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace gs {

	class device;

	enum memory_type
	{ 
		no_vma_cpu = 0,
		gpu_only = VMA_MEMORY_USAGE_GPU_ONLY,
		cpu_only = VMA_MEMORY_USAGE_CPU_ONLY,
		cpu_to_gpu = VMA_MEMORY_USAGE_CPU_TO_GPU,
		gpu_to_cpu = VMA_MEMORY_USAGE_GPU_TO_CPU,
		cpu_copy = VMA_MEMORY_USAGE_CPU_COPY,
		gpu_lazy = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED
	};

	/* on lazily allocated memory (VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
	 * A memory type with this flag set is only allowed to be bound to a 
	 * VkImage whose usage flags include VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT.
	*/

	class memory_manager
	{
	public:
		/* 0 = default of 256MiB */
		static void init(uint64_t preferredLargeHeapBlockSize = 0, uint32_t descriptorPoolCount = 1000);
		static VmaAllocator& get_allocator();

		/* VmaAllocation can be seen as VkDeviceMemory. it can be used to map allocated memory */
		[[nodiscard]] static VmaAllocation create_buffer(const VkBufferCreateInfo& createInfo, VkBuffer* outBuffer, VmaMemoryUsage usage);
		[[nodiscard]] static VmaAllocation create_image(const VkImageCreateInfo& createInfo, VkImage* outImage, VmaMemoryUsage usage);

		static void map(void** data, VmaAllocation allocation);
		static void unmap(VmaAllocation allocation);
		static void free(VmaAllocation allocation);
		static void destroy_image(VkImage image, VmaAllocation allocation);
		static void destroy_buffer(VkBuffer buffer, VmaAllocation allocation);

		static void terminate();

		static uint64_t total_allocation_size();
		static uint64_t currently_allocated_memory_size();

		[[nodiscard]] static VkDescriptorSet allocate_descriptor_set(VkDescriptorSetLayout layout);
		static void allocate_descriptor_sets(VkDescriptorSet* outSets, uint32_t setsCount, VkDescriptorSetLayout* inLayouts);
		static void reset_descriptor_pool();
		static VkDescriptorPool get_descriptor_pool() { return s_descriptor_pool; }

	private:		
		static VkDescriptorPool s_descriptor_pool;
	};

}