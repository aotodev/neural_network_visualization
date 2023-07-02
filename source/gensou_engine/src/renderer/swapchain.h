#pragma once

#include "core/core.h"
#include "core/log.h"
#include "core/event.h"

#include "renderer/framebuffer.h"

#include <vulkan/vulkan.h>

namespace gs {

	class graphics_pipeline;

	struct swapchain_properties
	{
		extent2d extent;
		bool vsync = true;
		bool use_depth = false;

		/* may increase frame rate but also power consumption. Should be avoided on mobile devices */
		/* may not be supported (FIFO will be selected in that case) */
		bool prefer_mailbox_mode = false;

		/* The composite shader will perform tone mapping with gamma correction */
		VkFormat desired_surface_format = VK_FORMAT_R8G8B8A8_UNORM; 
		VkCompositeAlphaFlagBitsKHR desired_composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		/* change this to apply a rotation to the swapchain image (may not be supported) */
		VkSurfaceTransformFlagBitsKHR surface_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; 
	};

	class swapchain
	{
	public:
		swapchain();

		void create_surface(void* inWindow, const swapchain_properties& inProperties);
		void create(extent2d extent, bool vsync);
        void terminate();

		uint32_t acquire_next_image(VkFence inFence = VK_NULL_HANDLE);
		void present(uint32_t frame);
		void on_resize(uint32_t width, uint32_t height);

		/* waits for all fences from all threads and frames on which the swapchain may depend on */
		void wait_for_cmds();

		extent2d get_image_extent() const { return m_extent; }
		uint32_t get_image_width() const { return m_extent.width; }
		uint32_t get_image_height() const { return m_extent.height; }

		uint32_t get_image_count() const { return (uint32_t)m_swapchain_images.size(); }
		VkImage get_image(uint32_t index) { return m_swapchain_images[index]; }
		VkImage* get_ptr_to_images() { return m_swapchain_images.data(); }

		VkRenderPass get_renderpass() { return m_renderpass; }
		const framebuffer<1>& get_framebuffer(uint32_t imgIndex) const { return m_framebuffers[imgIndex]; }
		const framebuffer<1>& get_current_img_framebuffer() const { return m_framebuffers[m_current_image_index]; }
		std::shared_ptr<graphics_pipeline> get_pipeline() { return m_screen_pipeline; }

		VkFormat get_format() const { return m_surface_format.format; }
		VkColorSpaceKHR get_color_space() const { return m_surface_format.colorSpace; }
		VkSurfaceFormatKHR get_surface_format() const { return m_surface_format; }

		bool supports_nonvsync_mode() const { return m_nonvsync_mode != m_vsync_mode; }

	private:
		void create_renderpass();
		void create_framebuffers();
		void create_semaphores();

	private:
		VkSwapchainKHR						m_swapchain;
		VkSurfaceKHR						m_surface;
		extent2d							m_extent;

		std::vector<VkImage>				m_swapchain_images;
		uint32_t							m_current_image_index = 0;

		VkRenderPass						m_renderpass = VK_NULL_HANDLE;
		std::vector<framebuffer<1>>			m_framebuffers;
		std::shared_ptr<graphics_pipeline> 	m_screen_pipeline;

		VkPresentModeKHR					m_vsync_mode, m_nonvsync_mode;
		VkSurfaceFormatKHR					m_surface_format;
		VkCompositeAlphaFlagBitsKHR			m_alpha_composite;
		VkSurfaceTransformFlagBitsKHR 		m_surface_transform;
		bool								m_vsync = true;

		/* synchronization primitives */
		struct present_semaphores
		{
			VkSemaphore image_acquired = VK_NULL_HANDLE;
			VkSemaphore render_complete = VK_NULL_HANDLE;
		}
		m_semaphores;
	};

}