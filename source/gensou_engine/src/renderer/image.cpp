#include "renderer/image.h"
#include "renderer/device.h"
#include "renderer/command_manager.h"

#include "core/log.h"
#include "core/engine_events.h"
#include "core/misc.h"

FORCEINLINE static VkAccessFlags get_access_by_layout(VkImageLayout layout)
{
	switch(layout)
	{
		case VK_IMAGE_LAYOUT_UNDEFINED: return 0x0;
		case VK_IMAGE_LAYOUT_PREINITIALIZED: return VK_ACCESS_HOST_WRITE_BIT; /* Only valid as initial layout for linear images, preserves memory contents */
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_ACCESS_TRANSFER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_ACCESS_TRANSFER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_ACCESS_SHADER_READ_BIT;
		default: return 0x0;
	}
} 

namespace gs {

	void image2d::transition_layout(VkCommandBuffer cmd, image_info& imageInfo)
	{
		VkImageMemoryBarrier imageBarrier{};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.oldLayout = imageInfo.oldLayout;
		imageBarrier.newLayout = imageInfo.newLayout;
		imageBarrier.image = imageInfo.image;
		imageBarrier.subresourceRange = imageInfo.subresources;
		imageBarrier.srcAccessMask = imageInfo.srcAccess;
		imageBarrier.dstAccessMask = imageInfo.dstAccess;

		vkCmdPipelineBarrier(
			cmd,
			imageInfo.srcStage,
			imageInfo.dstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrier);
	}

	void image2d::transition_layout(image_info* pImageInfo, uint32_t count, bool waitForFences)
	{
		auto cmd = command_manager::get_cmd_buffer(queue_family::graphics, std::this_thread::get_id());
		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0x0, nullptr };

		vkBeginCommandBuffer(cmd, &beginInfo);

		for(uint32_t i = 0; i < count; i++)
			transition_layout(cmd, pImageInfo[i]);

		vkEndCommandBuffer(cmd);

		command_manager::submit(cmd, waitForFences);
	}

	void image2d::buffer_to_image(VkCommandBuffer cmd, VkImage image, VkBuffer srcBuffer, const VkBufferImageCopy* bufferCopy, uint32_t copyCount, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount)
	{
		VkImageMemoryBarrier imageBarrier{};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.oldLayout = oldLayout;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier.image = image;
		imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, layerCount };
		imageBarrier.srcAccessMask = get_access_by_layout(oldLayout);
		imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrier);

		vkCmdCopyBufferToImage(cmd, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyCount, bufferCopy);

		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier.newLayout = newLayout;
		imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		//imageBarrier.dstAccessMask = get_access_by_layout(newLayout);
		imageBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, //VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrier);
	}

	void image2d::copy_image(VkCommandBuffer cmd, image_info& srcImage, image_info& dstImage)
	{
		VkImageMemoryBarrier barriers[2];
		barriers[0].pNext = nullptr;
		barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[0].image = dstImage.image;
		barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[0].subresourceRange = dstImage.subresources;

		barriers[0].oldLayout = dstImage.oldLayout;
		barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barriers[0].srcAccessMask = dstImage.srcAccess;
		barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		barriers[1] = barriers[0];
		barriers[1].image = srcImage.image;
		barriers[1].oldLayout = srcImage.oldLayout;
		barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barriers[1].srcAccessMask = srcImage.srcAccess;
		barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		barriers[1].subresourceRange = srcImage.subresources;

		vkCmdPipelineBarrier(
			cmd,
			srcImage.srcStage | dstImage.srcStage,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			2, barriers);

		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { (int32_t)srcImage.extent.width, (int32_t)srcImage.extent.height, 1 };

		blit.srcSubresource.aspectMask = srcImage.subresources.aspectMask;
		blit.srcSubresource.mipLevel = srcImage.subresources.baseMipLevel;
		blit.srcSubresource.baseArrayLayer = srcImage.subresources.baseArrayLayer;
		blit.srcSubresource.layerCount = srcImage.subresources.layerCount;

		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { (int32_t)dstImage.extent.width, (int32_t)dstImage.extent.height, 1 };

		blit.dstSubresource.aspectMask = dstImage.subresources.aspectMask;
		blit.dstSubresource.mipLevel = dstImage.subresources.baseMipLevel;
		blit.dstSubresource.baseArrayLayer = dstImage.subresources.baseArrayLayer;
		blit.dstSubresource.layerCount = dstImage.subresources.layerCount;

		vkCmdBlitImage(cmd,
			srcImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barriers[0].newLayout = dstImage.newLayout;
		barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barriers[0].dstAccessMask = dstImage.dstAccess;

		barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barriers[1].newLayout = srcImage.newLayout;
		barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barriers[1].dstAccessMask = srcImage.dstAccess;

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			srcImage.dstStage | dstImage.dstStage,
			0,
			0, nullptr,
			0, nullptr,
			2, barriers);
	}

	//////////////////////////////////////////////////
	// MEMBER FUNCTIONS
	//////////////////////////////////////////////////

	image2d::image2d(const void* pData, size_t size, extent2d extent, VkFormat format, bool generateMips, uint32_t layerCount)
	{
		create(pData, size, extent, format, generateMips, layerCount);
	}

	image2d::image2d(VkImageUsageFlags usage, extent2d extent, VkFormat format, uint32_t samples, bool generateMips)
	{
		create(usage, extent, format, generateMips, 1, samples);
	}

	image2d::image2d(VkImage image, extent2d extent, VkFormat format)
	{
		create(image, extent, format);
	}

	image2d::image2d(ktxTexture* kTexture, VkFormat format, bool generateMips)
	{
		create_from_ktx(kTexture, format, generateMips);
	}

	void image2d::create(const void* pData, size_t size, extent2d extent, VkFormat format, bool generateMips, uint32_t layerCount)
	{
		assert(pData && size);
		assert(extent.width && extent.height);

		invalidate();
		m_swapchain_target = false;
		m_static_extent = true;

		m_extent = extent;
		m_format = format;
		m_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		m_mip_levels = generateMips ? calculate_mip_count(m_extent.width, m_extent.height) : 1;
		m_layer_count = layerCount;

		if (generateMips)
		{
			if (!device::format_supports_blitt(m_format))
			{
				LOG_ENGINE(error, "mips requested but the chosen format's optimal tilling does not support blitting. No mips were generated");
				m_mip_levels = 1;
				generateMips = false;
			}
		}

		/* create image */
		{
			VkImageCreateInfo createImageInfo{};
			createImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			createImageInfo.imageType = VK_IMAGE_TYPE_2D;
			createImageInfo.extent = { extent.width, extent.height, 1 };
			createImageInfo.mipLevels = m_mip_levels;
			createImageInfo.arrayLayers = m_layer_count;
			createImageInfo.format = format;
			createImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			createImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			createImageInfo.usage = m_usage;
			createImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			createImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			m_image_allocation = memory_manager::create_image(createImageInfo, &m_image, VMA_MEMORY_USAGE_GPU_ONLY);
			assert(m_image != VK_NULL_HANDLE);
		}

		/* create image view */
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_image;
			viewInfo.viewType = m_layer_count > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			viewInfo.format = format;
			viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, m_mip_levels, 0, m_layer_count };

			VkResult imageViewCreation = vkCreateImageView(device::get_logical(), &viewInfo, nullptr, &m_image_view);
			if (imageViewCreation != VK_SUCCESS)
				engine_events::vulkan_result_error.broadcast(imageViewCreation, "Could not create texture imageview");
		}

		/* copy pixel data into the image and generate mips if needed */
		{
			VkBuffer stagingBuffer = VK_NULL_HANDLE;
			VkBufferCreateInfo stagingBufferCreateInfo{};
			stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferCreateInfo.size = size;
			stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocation stagingbufferMemory = memory_manager::create_buffer(stagingBufferCreateInfo, &stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);

			void* dstData = nullptr;
			memory_manager::map(&dstData, stagingbufferMemory);

			if (dstData)
				memcpy(dstData, pData, (size_t)size);

			memory_manager::unmap(stagingbufferMemory);

			/* copy buffer to image */
			VkBufferImageCopy copy{};
			copy.bufferOffset = 0ULL;
			copy.imageOffset = { 0, 0, 0 };
			copy.imageExtent = { extent.width, extent.height, 1 };
			copy.imageSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

			/* it's more efficient to send all commands on a single submit to a single thread, as vkQueueSubmit is an expensive command */
			/* since blitt requires a graphics queue, we'll use it for the buffer to image operation as well if mip generation is requested */
			auto queueFamily = generateMips ? queue_family::graphics : queue_family::transfer;
			auto cmd = command_manager::get_cmd_buffer(queueFamily, std::this_thread::get_id());

			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0x0, nullptr };
			vkBeginCommandBuffer(cmd, &beginInfo);

			VkImageLayout finalLayout = generateMips ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			buffer_to_image(cmd, m_image, stagingBuffer, &copy, 1, VK_IMAGE_LAYOUT_UNDEFINED, finalLayout, m_mip_levels, m_layer_count);

			if (generateMips)
				generate_mipmap_chain(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_mip_levels);

			vkEndCommandBuffer(cmd);

			command_manager::submit(cmd, true);
			memory_manager::destroy_buffer(stagingBuffer, stagingbufferMemory);
		}
	}

	void image2d::create(VkImageUsageFlags usage, extent2d extent, VkFormat format, bool generateMips, uint32_t layers, uint32_t samples)
	{
		uint32_t levels = generateMips ? calculate_mip_count(m_extent.width, m_extent.height) : 1;
		create(usage, extent, format, levels, layers, samples);
	}

	void image2d::create(VkImageUsageFlags usage, extent2d extent, VkFormat format, uint32_t levels, uint32_t layers, uint32_t samples)
	{
		invalidate();
		m_swapchain_target = false;

		bool lazy = false;
		if (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
		{
			if (!device::supports_lazy_allocation())
				m_usage = usage & ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
			else
				lazy = true;
		}

		m_extent = extent;
		m_format = format;
		m_usage = usage;
		m_samples = samples;
		m_mip_levels = levels;
		m_layer_count = layers;

		if(usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
			m_lazily_allocated = true;

		if (m_mip_levels > 1)
		{
			if (!device::format_supports_blitt(m_format))
			{
				LOG_ENGINE(error, "mips requested but the chosen format's optimal tilling does not support blitting");
				m_mip_levels = 1;
			}
		}

		VkImageCreateInfo createImageInfo{};
		createImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		createImageInfo.imageType = VK_IMAGE_TYPE_2D;
		createImageInfo.extent = { m_extent.width, m_extent.height, 1 };
		createImageInfo.mipLevels = m_mip_levels;
		createImageInfo.arrayLayers = m_layer_count;
		createImageInfo.format = format;
		createImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		createImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		createImageInfo.usage = m_usage;
		createImageInfo.samples = (VkSampleCountFlagBits)m_samples;
		createImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		m_image_allocation = memory_manager::create_image(createImageInfo, &m_image, lazy ? VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED : VMA_MEMORY_USAGE_GPU_ONLY);

		assert(m_image != VK_NULL_HANDLE);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_image;
		viewInfo.viewType = m_layer_count > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		viewInfo.format = m_format;

		viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, m_mip_levels, 0, m_layer_count };
		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		VkResult imageViewCreation = vkCreateImageView(device::get_logical(), &viewInfo, nullptr, &m_image_view);
		if (imageViewCreation != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(imageViewCreation, "Could not create texture imageview");
	}

	void image2d::create(VkImage image, extent2d extent, VkFormat format)
	{
		assert(image != VK_NULL_HANDLE);
		invalidate();

		m_swapchain_target = true;
		m_extent.width = extent.width;
		m_extent.height = extent.height;
		m_format = format;
		m_mip_levels = 1;
		m_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		m_layer_count = 1;

		m_image = image;

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		viewInfo.format = m_format;

		viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, m_mip_levels, 0, m_layer_count };

		VkResult imageViewCreation = vkCreateImageView(device::get_logical(), &viewInfo, nullptr, &m_image_view);
		if (imageViewCreation != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(imageViewCreation, "Could not create texture imageview");
	}

	void image2d::create_from_ktx(ktxTexture* kTexture, VkFormat format, bool generateMips)
	{
		assert(kTexture);

		invalidate();

		LOG_ENGINE(info, "createing image2d from ktx with format %u", (uint32_t)format);

		ktx_uint8_t* ktxTextureData = kTexture->pData;
		ktx_size_t ktxTextureSize = kTexture->dataSize;
	
		m_extent.width = kTexture->baseWidth;
		m_extent.height = kTexture->baseHeight;
		m_layer_count = kTexture->numLayers;
		m_format = format;
		m_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		if (generateMips)
			m_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		LOG_ENGINE(info, "ktx texture size == [%u, %u], total area == %u", m_extent.width, m_extent.height, m_extent.width * m_extent.height);

		if (kTexture->numLevels == 1 && generateMips)
		{
			if (!device::format_supports_blitt(m_format))
			{
				LOG_ENGINE(error, "mips requested but the chosen format's optimal tilling does not support blitting. No mips were generated");
				m_mip_levels = 1;
				generateMips = false;
			}
			else
			{
				m_mip_levels = calculate_mip_count(m_extent.width, m_extent.height);
			}
		}
		else
		{
			m_mip_levels = kTexture->numLevels;
            LOG_ENGINE(trace, "KTX | setting mip level count to %u", m_mip_levels);
			generateMips = false;
		}

		/* create image */
		{
			VkImageCreateInfo createImageInfo{};
			createImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			createImageInfo.imageType = VK_IMAGE_TYPE_2D;
			createImageInfo.extent = { m_extent.width, m_extent.height, 1 };
			createImageInfo.mipLevels = m_mip_levels;
			createImageInfo.arrayLayers = m_layer_count;
			createImageInfo.format = format;
			createImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			createImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			createImageInfo.usage = m_usage;
			createImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			createImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			m_image_allocation = memory_manager::create_image(createImageInfo, &m_image, VMA_MEMORY_USAGE_GPU_ONLY);

			assert(m_image != VK_NULL_HANDLE);
		}

		/* create image view */
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_image;
			viewInfo.viewType = m_layer_count > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			viewInfo.format = format;
			viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, m_mip_levels, 0, m_layer_count };

			VkResult imageViewCreation = vkCreateImageView(device::get_logical(), &viewInfo, nullptr, &m_image_view);
			if (imageViewCreation != VK_SUCCESS)
				engine_events::vulkan_result_error.broadcast(imageViewCreation, "Could not create texture imageview");
		}

		/* copy pixel data into the image and generate mips if needed */	
		{
			VkBuffer stagingBuffer = VK_NULL_HANDLE;
			VkBufferCreateInfo stagingBufferCreateInfo{};
			stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferCreateInfo.size = ktxTextureSize;
			stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocation stagingbufferMemory = memory_manager::create_buffer(stagingBufferCreateInfo, &stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);

			void* dstData = nullptr;
			memory_manager::map(&dstData, stagingbufferMemory);

			if (dstData)
				memcpy(dstData, ktxTextureData, (size_t)ktxTextureSize);

			memory_manager::unmap(stagingbufferMemory);

			/* Setup buffer copy regions for each layer and its respective mip levels */
			std::vector<VkBufferImageCopy> copies;
			copies.reserve(m_layer_count * m_mip_levels);

			for(uint32_t layer = 0; layer < kTexture->numLayers; layer++)
			{
				for (uint32_t mip = 0; mip < kTexture->numLevels; mip++)
				{
					ktx_size_t offset;
					KTX_error_code result = ktxTexture_GetImageOffset(kTexture, mip, layer, 0, &offset);
					assert(result == KTX_SUCCESS);

					VkBufferImageCopy bufferCopyRegion = {};
					bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					bufferCopyRegion.imageSubresource.mipLevel = mip;
					bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
					bufferCopyRegion.imageSubresource.layerCount = 1;
					bufferCopyRegion.imageExtent.width = std::max(uint32_t(1), (uint32_t)kTexture->baseWidth >> mip);
					bufferCopyRegion.imageExtent.height = std::max(uint32_t(1), (uint32_t)kTexture->baseHeight >> mip);
					bufferCopyRegion.imageExtent.depth = 1;
					bufferCopyRegion.bufferOffset = offset;

					copies.push_back(bufferCopyRegion);
				}	
			}

			/* it's more efficient to send all commands on a single queueSubmit to a single thread, as vkQueueSubmit is an expensive command */
			/* since blitt requires a graphics queue, we'll use it for the buffer to image operation as well if mip generation is requested */
			auto queueFamily = generateMips ? queue_family::graphics : queue_family::transfer;
			auto cmd = command_manager::get_cmd_buffer(queueFamily, std::this_thread::get_id());

			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0x0, nullptr };
			vkBeginCommandBuffer(cmd, &beginInfo);

			VkImageLayout finalLayout = generateMips ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			buffer_to_image(cmd, m_image, stagingBuffer, copies.data(), copies.size(), VK_IMAGE_LAYOUT_UNDEFINED, finalLayout, m_mip_levels, m_layer_count);

			if (generateMips)
				generate_mipmap_chain(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_mip_levels);

			vkEndCommandBuffer(cmd);

			command_manager::submit(cmd, true);

			memory_manager::destroy_buffer(stagingBuffer, stagingbufferMemory);
		}
	}

	void image2d::write(const void* src, size_t size, extent2d imgExtent, extent3d offset)
	{
		assert(m_image != VK_NULL_HANDLE);

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkBufferCreateInfo stagingBufferCreateInfo{};
		stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferCreateInfo.size = size;
		stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocation stagingbufferMemory = memory_manager::create_buffer(stagingBufferCreateInfo, &stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);

		void* dstData = nullptr;
		memory_manager::map(&dstData, stagingbufferMemory);

		if (dstData)
			memcpy(dstData, src, size);

		memory_manager::unmap(stagingbufferMemory);

		/* copy buffer to image */
		VkBufferImageCopy copy{};
		copy.bufferOffset = 0ULL;
		copy.imageOffset = { (int32_t)offset.width, (int32_t)offset.height, 1 };
		copy.imageExtent = { imgExtent.width, imgExtent.height, 1 };

		VkImageSubresourceLayers subRes{};
		subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subRes.mipLevel = 0;
		subRes.baseArrayLayer = offset.depth;
		subRes.layerCount = 1;

		copy.imageSubresource = subRes;

	}

	void image2d::resize(uint32_t width, uint32_t height)
	{
		if (m_swapchain_target)
		{
			LOG_ENGINE(error, "Swapchain image cannot be resized, only updated");
			return;
		}

		if (extent2d{ width, height} == m_extent)
			return;

		create(m_usage, extent2d{ width, height }, m_format, m_mip_levels, m_layer_count, m_samples);
	}

	void image2d::resize(VkImage image, uint32_t width, uint32_t height)
	{
		if (!m_swapchain_target)
		{
			LOG_ENGINE(error, "swapchain image passed to resize a non-swapchain image2D");
			return;
		}

		create(image, extent2d(width, height), m_format);
	}

	void image2d::invalidate()
	{
		if (m_image_view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device::get_logical(), m_image_view, nullptr);
			m_image_view = VK_NULL_HANDLE;
		}

		if (m_image != VK_NULL_HANDLE && m_image_allocation != VK_NULL_HANDLE && !m_swapchain_target)
		{
			/* also frees the allocation */
			memory_manager::destroy_image(m_image, m_image_allocation); 
			m_image = VK_NULL_HANDLE;
			m_image_allocation = VK_NULL_HANDLE;
		}
	}

	void image2d::generate_mipmap_chain(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t maxLevels, uint32_t layer)
	{
		uint32_t levels = m_mip_levels;
		
		if(maxLevels)
		{
			if(maxLevels > levels)
			{
				LOG_ENGINE(error, "requested maxLevels of %u for mipmap generation is higher than the number of mips requested (%u) when creating this image", maxLevels, levels);
			}
			else
			{
				levels = maxLevels;
			}
		}

		uint32_t layerCount = 1;
		uint32_t baseArrayLayer = 0;

		if(layer == UINT32_MAX) /* all layers */
		{
			baseArrayLayer = 0;
			layerCount = m_layer_count;
		}
		else if (layer >= m_layer_count)
		{
			LOG_ENGINE(error, "requested generation of mips for a layer (%u) that this image does not have (num of layers == %u)", layer, m_layer_count);
			baseArrayLayer = 0;
		}
		else
		{
			baseArrayLayer = layer;
		}

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = m_image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
		barrier.subresourceRange.layerCount = layerCount;
		barrier.subresourceRange.baseMipLevel = 0;

		/* if needed, transition the layout of all mips in the chain that will be generated */
		if(oldLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.subresourceRange.levelCount = levels; 
			barrier.oldLayout = oldLayout;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; 
			barrier.srcAccessMask = get_access_by_layout(oldLayout);
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);
		}

		barrier.subresourceRange.levelCount = 1; /* mips will be generated one at a time */

		int32_t mipWidth = m_extent.width;
		int32_t mipHeight = m_extent.height;

		for (uint32_t i = 1; i < levels; i++)
		{
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = baseArrayLayer;
			blit.srcSubresource.layerCount = layerCount;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			//blit.dstOffsets[1] = { std::max(1, mipWidth >> i), std::max(1, mipHeight >> i), 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(cmd,
				m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = newLayout;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = get_access_by_layout(newLayout);

			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
			//mipWidth = std::max(1, mipWidth >> i);
			//mipHeight = std::max(1, mipHeight >> i);

			barrier.subresourceRange.baseMipLevel++;
		}

		barrier.subresourceRange.baseMipLevel = levels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = newLayout;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = get_access_by_layout(newLayout);

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	}

	////////////
	// IMAGE CUBE
	//////////////

	image_cube::image_cube(VkBuffer imageBuffer, VkFormat format, uint32_t width, uint32_t height)
	{
		create(imageBuffer, format, width, height);
	}

	void image_cube::create(VkBuffer imageBuffer, VkFormat format, uint32_t width, uint32_t height)
	{
		invalidate();

		m_extent.width = width;
		m_extent.height = height;
		m_format = format;

		VkImageCreateInfo createImageInfo{};
		createImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		createImageInfo.imageType = VK_IMAGE_TYPE_2D;
		createImageInfo.extent = { m_extent.width, m_extent.height, 1 };
		createImageInfo.mipLevels = 1;
		createImageInfo.arrayLayers = 6;
		createImageInfo.format = format;
		createImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		createImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		createImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		createImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		createImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		m_image_allocation = memory_manager::create_image(createImageInfo, &m_image, VMA_MEMORY_USAGE_GPU_ONLY);

		assert(m_image != VK_NULL_HANDLE);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		viewInfo.format = format;
		viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };

		VkResult imageViewCreation = vkCreateImageView(device::get_logical(), &viewInfo, nullptr, &m_image_view);
		if (imageViewCreation != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(imageViewCreation, "Could not create texture imageview");

		LOG_ENGINE(info, "Created Image views for ImageCube");
	}

	void image_cube::invalidate()
	{
		if (m_image_view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device::get_logical(), m_image_view, nullptr);
			m_image_view = VK_NULL_HANDLE;
		}

		if (m_image != VK_NULL_HANDLE && m_image_allocation != VK_NULL_HANDLE)
		{
			/* also frees the allocation */
			memory_manager::destroy_image(m_image, m_image_allocation); 
			m_image = VK_NULL_HANDLE;
			m_image_allocation = VK_NULL_HANDLE;
		}
	}

}
