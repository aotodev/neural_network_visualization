#pragma once

#include "core/core.h"
#include "core/log.h"
#include "core/engine_events.h"

#include "renderer/image.h"
#include "renderer/device.h"

#include <vulkan/vulkan.h>

namespace gs {

	template<int8_t n>
	class framebuffer
	{
	public:
		framebuffer() = default;
		~framebuffer() { clear(); }

		void create(VkRenderPass renderpass);
		void recreate() { assert(m_renderpass); create(m_renderpass); }
		void resize(uint32_t width, uint32_t height, VkImage swapchainImage = VK_NULL_HANDLE);
		void clear();

		/* invalidates the frambuffer if one already exists. needs to call create/recreate to create a new valid one */
		void set_attachment(uint32_t index, std::shared_ptr<image2d> image) { m_attachments[index] = image; }

		std::shared_ptr<image2d> set_attachment(uint32_t index, VkImageUsageFlags usage, VkFormat format, extent2d size, bool generateMips = false, uint32_t samples = 1);
		std::shared_ptr<image2d> set_attachment(uint32_t index, VkImage image, VkFormat format, extent2d size);
		std::shared_ptr<image2d> get_attachment(uint32_t index) { return m_attachments[index]; }

		void set_clear_value(uint32_t index, VkClearValue&& value) { m_clear_values[index] = value; }
		void set_clear_value_count(int32_t count = -1) { m_clear_value_count = count >= 0 ? count : m_clear_values.size(); }

		VkClearValue get_clear_value(uint32_t index) const { return m_clear_values[index]; }
		VkClearValue* get_clear_value_data() const { return m_clear_values.data(); }
		uint32_t get_clear_value_count() const { return m_clear_value_count; }

		uint32_t get_attachment_count() const { return (uint32_t)m_attachments.size(); }

		VkFramebuffer get() const { return m_framebuffer; }

		extent2d get_extent() const { return m_size; }
		uint32_t get_width() const { return m_size.width; }
		uint32_t get_height() const { return m_size.height; }

	private:
		VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
		VkRenderPass m_renderpass = VK_NULL_HANDLE;

		std::array<std::shared_ptr<image2d>, n> m_attachments;

		mutable std::array<VkClearValue, n> m_clear_values;
		
		extent2d m_size;
		uint32_t m_clear_value_count = n;

	};

	template<int8_t n>
	inline void framebuffer<n>::create(VkRenderPass renderpass)
	{
		m_renderpass = renderpass;
		assert(m_renderpass != VK_NULL_HANDLE);

		std::array<VkImageView, n> imageViews;

		/* holds the largest width and largest height from all attachemnts (if more than 1) */
		m_size = { 0, 0 };

		/* get the bigger width|height from all attachments */
		for (uint32_t i = 0; i < n; i++)
		{
			imageViews[i] = m_attachments[i]->get_image_view();

			if (m_attachments[i]->get_width() > m_size.width)
				m_size.width = m_attachments[i]->get_width();

			if (m_attachments[i]->get_height() > m_size.height)
				m_size.height = m_attachments[i]->get_height();
		}

		if (m_framebuffer != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(device::get_logical(), m_framebuffer, nullptr);
			m_framebuffer = VK_NULL_HANDLE;
		}

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_renderpass;
		framebufferInfo.attachmentCount = (uint32_t)n;
		framebufferInfo.pAttachments = imageViews.data();
		framebufferInfo.width = m_size.width;
		framebufferInfo.height = m_size.height;
		framebufferInfo.layers = 1;

		VkResult createFramebufferResult = vkCreateFramebuffer(device::get_logical(), &framebufferInfo, nullptr, &m_framebuffer);
		if (createFramebufferResult != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(createFramebufferResult, "could not create framebuffer");
	}

	template<int8_t n>
	inline void framebuffer<n>::resize(uint32_t width, uint32_t height, VkImage swapchainImage /*= VK_NULL_HANDLE*/)
	{
		if (width + height == 0)
			return;

		bool bRecreate = false;

		for (size_t i = 0; i < m_attachments.size(); i++)
		{
			if (m_attachments[i]->swapchain_target())
			{
				assert(swapchainImage != VK_NULL_HANDLE);
				m_attachments[i]->resize(swapchainImage, width, height);
				bRecreate = true;
			}
			else
			{
				if ((m_size.width == width && m_size.height == height))
					continue;

				m_attachments[i]->resize(width, height);
				bRecreate = true;
			}
		}

		if(bRecreate)
			recreate();
	}

	template<int8_t n>
	inline void framebuffer<n>::clear()
	{
		LOG_ENGINE(trace, "clearing framebuffer object");

		if (m_framebuffer != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(device::get_logical(), m_framebuffer, nullptr);
			m_framebuffer = VK_NULL_HANDLE;
		}

		for (size_t i = 0; i < m_attachments.size(); i++)
			m_attachments[i].reset();

		m_renderpass = VK_NULL_HANDLE;
		m_clear_value_count = 0;
		m_size = { 0, 0 };
	}

	template<int8_t n>
	inline std::shared_ptr<image2d> framebuffer<n>::set_attachment(uint32_t index, VkImageUsageFlags usage, VkFormat format, extent2d size, bool generateMips, uint32_t samples)
	{
		m_attachments[index] = std::make_shared<image2d>(usage, size, format, samples, generateMips);

		if (m_framebuffer != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(device::get_logical(), m_framebuffer, nullptr);
			m_framebuffer = VK_NULL_HANDLE;

			LOG_ENGINE(warn, "setting attachment %u of an existing framebuffer - current framebuffer will be destroyed", index);
		}

		return m_attachments[index];
	}

	template<int8_t n>
	inline std::shared_ptr<image2d> framebuffer<n>::set_attachment(uint32_t index, VkImage image, VkFormat format, extent2d size)
	{
		m_attachments[index] = std::make_shared<image2d>(image, size, format);
		
		if (m_framebuffer != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(device::get_logical(), m_framebuffer, nullptr);
			m_framebuffer = VK_NULL_HANDLE;

			LOG_ENGINE(warn, "setting attachment %u of an existing framebuffer - current framebuffer will be destroyed", index);
		}

		return m_attachments[index];
	}

}