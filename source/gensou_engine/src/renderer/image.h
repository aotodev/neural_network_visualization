#pragma once

#include "core/core.h"
#include "core/uuid.h"

#include "renderer/memory_manager.h"

#include <vulkan/vulkan.h>

#include <ktx.h>
#include <ktxvulkan.h>

namespace gs {

	/* functions that have a VkCommandBuffer parameter should be executed inside a command buffer */
	/* the caller is responsible for synchronization and submition */

	class image2d
	{
		friend class texture;

	public:
		/* static helpers */

		struct image_info
		{
			VkImage image = VK_NULL_HANDLE;
			VkImageLayout oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			VkImageLayout newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			extent2d extent{};

			VkPipelineStageFlagBits srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			VkPipelineStageFlagBits dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			VkAccessFlagBits srcAccess = VK_ACCESS_SHADER_READ_BIT;
			VkAccessFlagBits dstAccess = VK_ACCESS_SHADER_READ_BIT;

			VkImageSubresourceRange subresources{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		};

		static void transition_layout(VkCommandBuffer cmd, image_info& imageInfo);
		static void transition_layout(image_info* pImageInfo, uint32_t count, bool waitForFences = true);

		static void buffer_to_image(VkCommandBuffer cmd, VkImage image, VkBuffer srcBuffer, const VkBufferImageCopy* bufferCopy, uint32_t copyCount, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount);

		/* uses blit internally */
		static void copy_image(VkCommandBuffer cmd, image_info& srcImage, image_info& dstImage);

	public:
		image2d() = default;

		image2d(const void* pData, size_t size, extent2d extent, VkFormat format, bool generateMips = true, uint32_t layerCount = 1);
		image2d(VkImageUsageFlags usage, extent2d extent, VkFormat format, uint32_t samples = 1, bool generateMips = false);
		image2d(VkImage image, extent2d extent, VkFormat format);
		image2d(ktxTexture* kTexture, VkFormat format, bool generateMips = false);

		~image2d(){ invalidate(); }

		/* for textures */
		void create(const void* pData, size_t size, extent2d extent, VkFormat format, bool generateMips = true, uint32_t layerCount = 1);

		/* general use, mostly for attachment/storage */
		void create(VkImageUsageFlags usage,extent2d extent, VkFormat format, uint32_t levels, uint32_t layers = 1, uint32_t samples = 1);
		void create(VkImageUsageFlags usage,extent2d extent, VkFormat format, bool generateMips = false, uint32_t layers = 1, uint32_t samples = 1);

		/* for swapchain targets */
		void create(VkImage image, extent2d extent, VkFormat format);

		/* will only generate mips in case the ktx file does not contain it already and if the format allows it */
		void create_from_ktx(ktxTexture* kTexture, VkFormat format, bool generateMips = false);

		void invalidate();

		/* offset.depth is the layer in the case of a texture array */
		void write(const void* src, size_t size, extent2d imgExtent, extent3d offset);

		/* for attachments and/or storage */
		void resize(uint32_t width, uint32_t height);

		/* for swapchain targets */
		void resize(VkImage image, uint32_t width, uint32_t height);

		void update_layout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t firstMip = 0, uint32_t levelCount = UINT32_MAX, uint32_t layer = UINT32_MAX);
		void update_layout(VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t firstMip = 0, uint32_t levelCount = UINT32_MAX, uint32_t layer = UINT32_MAX);

		/* defaults: maxLevels == 0 means maximum possible, layer == UINT32_MAX means all layers */
		void generate_mipmap_chain(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t maxLevels = 0, uint32_t layer = UINT32_MAX);
		void generate_mipmap_chain(VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t maxLevels = 0, uint32_t layer = UINT32_MAX);

		//////////////// getters ////////////////
		
		uuid get_id() const { return m_id; }

		VkImage get_image() const { return m_image; }
		VkImageView get_image_view() const { return m_image_view; }

		extent2d get_extent() const { return m_extent; }
		uint32_t get_width()  const { return m_extent.width; }
		uint32_t get_height() const { return m_extent.height; }
		uint32_t get_mip_level_count() const { return m_mip_levels; }
		uint32_t get_sample_count() const { return m_samples; }
		VkFormat get_format() const { return m_format; }
		VkImageUsageFlags get_usage_flags() const { return m_usage; }

		bool static_extent() 	const { return m_static_extent; }
		bool lazily_allocated() const { return m_lazily_allocated; }
		bool swapchain_target() const { return m_swapchain_target; }

	private:
		uuid m_id;

		VkImage m_image = VK_NULL_HANDLE;
		VkImageView m_image_view = VK_NULL_HANDLE;
		VmaAllocation m_image_allocation = VK_NULL_HANDLE;

		VkFormat m_format;
		VkImageUsageFlags m_usage;
		extent2d m_extent;
		uint32_t m_mip_levels = 1, m_layer_count = 1, m_channels = 4, m_samples = 1;

		bool m_swapchain_target = false;
		bool m_static_extent = false;
		bool m_lazily_allocated = false;

	};

	class image_cube
	{
	public:
		image_cube() = default;
		image_cube(VkBuffer imageBuffer, VkFormat format, uint32_t width, uint32_t height);
		~image_cube() { invalidate(); }

		void create(VkBuffer imageBuffer, VkFormat format, uint32_t width, uint32_t height);
		void invalidate();

		//getters
		VkImage get_image() const { return m_image; }
		VkImageView get_image_view() const { return m_image_view; }
	private:
		VkImage m_image = VK_NULL_HANDLE;
		VkImageView m_image_view = VK_NULL_HANDLE;
		VmaAllocation m_image_allocation = VK_NULL_HANDLE;

		VkFormat m_format;
		extent2d m_extent;
		uint32_t m_channels;

		friend class Texture;
	};

}