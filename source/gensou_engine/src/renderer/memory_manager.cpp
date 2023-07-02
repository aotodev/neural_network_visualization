#include "renderer/memory_manager.h"
#include "renderer/device.h"
#include "renderer/validation_layers.h"

#include "core/log.h"
#include "core/runtime.h"
#include "core/engine_events.h"

namespace gs {

	static constexpr uint32_t s_max_queue_submit_per_frame = 16;

	static VmaAllocator s_allocator;
	static uint64_t s_total_allocation_in_bytes = 0;
	static uint64_t s_current_allocated_bytes = 0;

	VmaAllocator& memory_manager::get_allocator() { return s_allocator; }

	uint64_t memory_manager::total_allocation_size() { return s_total_allocation_in_bytes; }
	uint64_t memory_manager::currently_allocated_memory_size() { return s_current_allocated_bytes; }

	VkDescriptorPool memory_manager::s_descriptor_pool;

	void memory_manager::init(uint64_t preferredLargeHeapBlockSize, uint32_t descriptorPoolCount)
	{
		VmaAllocatorCreateInfo allocatorCreateInfo = {};

		allocatorCreateInfo.vulkanApiVersion = device::get_device_api_version();
		allocatorCreateInfo.physicalDevice = device::get_physical();
		allocatorCreateInfo.device = device::get_logical();
		allocatorCreateInfo.instance = device::get_instance();
		allocatorCreateInfo.flags = device::supports_buffer_device_address() ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT : 0x0;

		/* if VMA_DYNAMIC_VULKAN_FUNCTIONS */
		//VmaVulkanFunctions vulkanFunctions = {};
		//vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
		//vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
		//allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

		if (preferredLargeHeapBlockSize)
			allocatorCreateInfo.preferredLargeHeapBlockSize = preferredLargeHeapBlockSize;

		VkResult createAllocatorResult = vmaCreateAllocator(&allocatorCreateInfo, &s_allocator);

		INTERNAL_ASSERT_VKRESULT(createAllocatorResult, "Failed to create vma allocator");
		LOG_ENGINE(trace, "Created vma allocator");


		/* DESCRIPTOR SET POOL */
		VkDescriptorPoolSize poolSize[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER,					descriptorPoolCount },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	descriptorPoolCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 			descriptorPoolCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 			descriptorPoolCount },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 		descriptorPoolCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 		descriptorPoolCount },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 			descriptorPoolCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 			descriptorPoolCount },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 	descriptorPoolCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 	descriptorPoolCount },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 			descriptorPoolCount }
		};

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

		/* this flag is far too slow on some mobile drivers
		 * descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		 */
		descriptorPoolCreateInfo.flags = 0x0;
		descriptorPoolCreateInfo.poolSizeCount = 11;
		descriptorPoolCreateInfo.pPoolSizes = poolSize;
		descriptorPoolCreateInfo.maxSets = descriptorPoolCount * 11;

		VkResult createDescriptorPool = vkCreateDescriptorPool(device::get_logical(), &descriptorPoolCreateInfo, nullptr, &s_descriptor_pool);

		if (createDescriptorPool != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(createDescriptorPool, "Could not create Descriptor Pool");

		LOG_ENGINE(trace, "created descriptor pool | maxSets == %u", descriptorPoolCreateInfo.maxSets);
	}

	void memory_manager::map(void** data, VmaAllocation allocation)
	{
		VkResult result = vmaMapMemory(s_allocator, allocation, data);
		INTERNAL_ASSERT_VKRESULT(result, "map memory failed");
	}

	void memory_manager::unmap(VmaAllocation allocation)
	{
		vmaUnmapMemory(s_allocator, allocation);
	}

	VmaAllocation memory_manager::create_buffer(const VkBufferCreateInfo& createInfo, VkBuffer* outBuffer, VmaMemoryUsage usage)
	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = usage;

		VmaAllocation allocation;
		VkResult result = vmaCreateBuffer(s_allocator, &createInfo, &allocCreateInfo, outBuffer, &allocation, nullptr);

		INTERNAL_ASSERT_VKRESULT(result, "failed to create bufffer");

		VmaAllocationInfo allocInfo;
		vmaGetAllocationInfo(s_allocator, allocation, &allocInfo);
		s_total_allocation_in_bytes += allocInfo.size;
		s_current_allocated_bytes += allocInfo.size;

		return allocation;
	}

	VmaAllocation memory_manager::create_image(const VkImageCreateInfo& createInfo, VkImage* outImage, VmaMemoryUsage usage)
	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = usage;

		VmaAllocation allocation;
		VkResult result = vmaCreateImage(s_allocator, &createInfo, &allocCreateInfo, outImage, &allocation, nullptr);

		INTERNAL_ASSERT_VKRESULT(result, "failed to create image");

		VmaAllocationInfo allocInfo;
		vmaGetAllocationInfo(s_allocator, allocation, &allocInfo);
		s_total_allocation_in_bytes += allocInfo.size;
		s_current_allocated_bytes += allocInfo.size;

		return allocation;
	}

	void memory_manager::destroy_buffer(VkBuffer buffer, VmaAllocation allocation)
	{
		if (buffer != VK_NULL_HANDLE)
		{
			VmaAllocationInfo allocInfo;
			vmaGetAllocationInfo(s_allocator, allocation, &allocInfo);
			s_current_allocated_bytes -= allocInfo.size;

			vmaDestroyBuffer(s_allocator, buffer, allocation);
		}
	}

	void memory_manager::destroy_image(VkImage image, VmaAllocation allocation)
	{
		if (image != VK_NULL_HANDLE)
		{
			VmaAllocationInfo allocInfo;
			vmaGetAllocationInfo(s_allocator, allocation, &allocInfo);
			s_current_allocated_bytes -= allocInfo.size;

			vmaDestroyImage(s_allocator, image, allocation);
		}
	}

	void memory_manager::free(VmaAllocation allocation)
	{
		if (allocation != VK_NULL_HANDLE)
			vmaFreeMemory(s_allocator, allocation);
	}

	void memory_manager::terminate()
	{
		vmaDestroyAllocator(s_allocator);
		LOG_ENGINE(trace, "Destroyed vma allocator");

		if (s_descriptor_pool != VK_NULL_HANDLE)
		{
			vkResetDescriptorPool(device::get_logical(), s_descriptor_pool, 0x0);
			vkDestroyDescriptorPool(device::get_logical(), s_descriptor_pool, nullptr);
		}
	}

	VkDescriptorSet memory_manager::allocate_descriptor_set(VkDescriptorSetLayout layout)
	{
		VkDescriptorSet outSet = VK_NULL_HANDLE;
		VkDescriptorSetAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, s_descriptor_pool, 1, &layout };
		VkResult descriptionSetAllocation = vkAllocateDescriptorSets(device::get_logical(), &allocateInfo, &outSet);

		if (descriptionSetAllocation != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(descriptionSetAllocation, "failed to allocate descriptor set");

		return outSet;
	}

	void memory_manager::allocate_descriptor_sets(VkDescriptorSet* outSets, uint32_t setsCount, VkDescriptorSetLayout* inLayouts)
	{
		VkDescriptorSetAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, s_descriptor_pool, setsCount, inLayouts };
		VkResult descriptionSetAllocation = vkAllocateDescriptorSets(device::get_logical(), &allocateInfo, outSets);

		if (descriptionSetAllocation != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(descriptionSetAllocation, "failed to allocate descriptor set");
	}

	void memory_manager::reset_descriptor_pool()
	{
		vkResetDescriptorPool(device::get_logical(), s_descriptor_pool, 0x0);
	}

}