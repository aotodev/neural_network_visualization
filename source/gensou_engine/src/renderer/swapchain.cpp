#include "renderer/swapchain.h"

#include "renderer/device.h"
#include "renderer/renderer.h"
#include "renderer/pipeline.h"
#include "renderer/memory_manager.h"
#include "renderer/command_manager.h"
#include "renderer/validation_layers.h"

#include "core/core.h"
#include "core/system.h"
#include "core/log.h"
#include "core/runtime.h"
#include "core/time.h"
#include "core/engine_events.h"
#include "core/gensou_app.h"
#include <queue>
#include <vulkan/vulkan_core.h>


namespace {

	struct surface_priority_format
	{
		VkSurfaceFormatKHR surface_format;
		uint32_t priority = 0;
	};
}

namespace std {

	template<>
	struct less<surface_priority_format>
	{
		bool operator()(const surface_priority_format& s1, const surface_priority_format& s2) const
		{
			return s1.priority > s2.priority;
		}			
	};
}

extern VkSurfaceKHR create_vulkan_surface(void* window);

namespace gs {

	swapchain::swapchain()
		: m_swapchain(VK_NULL_HANDLE), m_surface(VK_NULL_HANDLE)
	{
		create_semaphores();
	}

	void swapchain::create_surface(void* inWindow, const swapchain_properties& props)
	{
		wait_for_cmds();
		
		/* uses swapchain image as attachments */
		for (auto& framebuffer : m_framebuffers)
			framebuffer.clear();

		/* uses surface that may be destroyed */
		if (m_swapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device::get_logical(), m_swapchain, nullptr);
			m_swapchain = VK_NULL_HANDLE;
		}

		/* clean up before reseting it */
		if (m_surface != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(device::get_instance(), m_surface, nullptr);
			m_surface = VK_NULL_HANDLE;
		}

		m_surface = create_vulkan_surface(inWindow);
		m_extent = props.extent;
		m_vsync = props.vsync;
		m_surface_transform = props.surface_transform;

		/*------------------------------------------------------------------*/

		/* check surface support */
		{
			VkBool32 hasSurfaceSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device::get_physical(), device::get_graphics_family_index(), m_surface, &hasSurfaceSupport);
			if (hasSurfaceSupport == VK_TRUE)
			{
				LOG_ENGINE(trace, "device has surface support");
			}
			else
			{
				LOG_ENGINE(error, "This device's vulkan driver does not have surface support, impossible to present");
				system::error_msg("This device's vulkan driver does not have surface support, impossible to present");
				exit(-1);
			}
		}

		device::select_present_queue(m_surface);

		/* Alpha composite */
		{
			VkSurfaceCapabilitiesKHR surfaceCapabilities{};
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device::get_physical(), m_surface, &surfaceCapabilities);

			if (surfaceCapabilities.supportedCompositeAlpha & props.desired_composite_alpha)
			{
				m_alpha_composite = props.desired_composite_alpha;
			}
			else if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
			{
				/* most common on desktop and garanteed on windows */
				m_alpha_composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
				LOG_ENGINE(trace, "using VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR");
			}
			else
			{
				/* the only one on android */
				m_alpha_composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
				LOG_ENGINE(trace, "using VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR");
			}
		}

		/* Present mode */
		{
			/* set to the only mode garanteed to be supported */
			m_vsync_mode = VK_PRESENT_MODE_FIFO_KHR;
			m_nonvsync_mode = VK_PRESENT_MODE_FIFO_KHR;

			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device::get_physical(), m_surface, &presentModeCount, nullptr);
			std::vector<VkPresentModeKHR> presentModes(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device::get_physical(), m_surface, &presentModeCount, presentModes.data());

			for (VkPresentModeKHR pMode : presentModes)
			{
				if (pMode == VK_PRESENT_MODE_MAILBOX_KHR && props.prefer_mailbox_mode)
					m_vsync_mode = VK_PRESENT_MODE_MAILBOX_KHR;

				if (pMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
					m_nonvsync_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

				if (m_nonvsync_mode != VK_PRESENT_MODE_IMMEDIATE_KHR && pMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
					m_nonvsync_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;

#if 0 /* this 2 modes effectively allow writes to an image that is still being presented, resulting in very visible tearing and should not be used */
				if (m_nonvsync_mode != VK_PRESENT_MODE_IMMEDIATE_KHR && pMode == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR)
					m_nonvsync_mode = VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR;

				if (m_nonvsync_mode != VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR && pMode == VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR)
					m_nonvsync_mode = VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR;
#endif
			}

			if (m_nonvsync_mode == VK_PRESENT_MODE_FIFO_KHR)
			{
				LOG_ENGINE(info, "This device does not support a non-VSync present mode");
			}

			m_vsync_mode = VK_PRESENT_MODE_FIFO_KHR;
		}

		if (!m_vsync)
		{
			/* no vsync support */
			if (m_nonvsync_mode == m_vsync_mode)
			{
				m_vsync = true;
				LOG_ENGINE(warn, "non vsync mode asked but not supported");
			}
		}

		bool recreateRenderpass = false;

		/* Surface format & color space */
		{
			uint32_t surfaceFormatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device::get_physical(), m_surface, &surfaceFormatCount, nullptr);

			std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device::get_physical(), m_surface, &surfaceFormatCount, surfaceFormats.data());

			/* we need a sRGB format and color space MUST be SRGB_NONLINEAR */
			VkSurfaceFormatKHR selectedFormat = surfaceFormats[0];
			std::priority_queue<surface_priority_format> formatsQueue;

			for (const VkSurfaceFormatKHR& f : surfaceFormats)
			{
				if (f.format == props.desired_surface_format && f.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
				{
					LOG_ENGINE(info, "Found desired surface format");
					selectedFormat = f;
					break;
				}

				/* garanteed on android devices */
				if(f.format == VK_FORMAT_R8G8B8A8_SRGB && f.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
				{
					formatsQueue.push(surface_priority_format{f, 1UL});
					break;
				}

				/* garanteed on windows and over 90% on linux */
				if(f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
				{
					formatsQueue.push(surface_priority_format{f, 2UL});
					break;
				}
			}

			if(selectedFormat.format != props.desired_surface_format)
			{
				if(!formatsQueue.empty())
				{
					selectedFormat = formatsQueue.top().surface_format;
				}
				else
				{
					//TODO: if a sRGB format could not be found we should create the screen pipeline
					//with a different fragment shader that performs gamma correction explicitly
					//alternatively we could alter the shader to take a const boolean value through a push constant

					LOG_ENGINE(critical, "This device does not offer a suitable surface format for the swapchain images");
					system::error_msg("This device does not offer a suitable surface format for the swapchain images");
					exit(-1);
				}
			}

			/* check if there is a need to recreate the renderpass */
			if (selectedFormat.colorSpace != m_surface_format.colorSpace || selectedFormat.format != m_surface_format.format)
				recreateRenderpass = true;

			m_surface_format = selectedFormat;
		}

		/*----------------------------------------------------------------------------*/

		if (m_renderpass == VK_NULL_HANDLE || recreateRenderpass)
		{
			create_renderpass();
			m_screen_pipeline.reset();
		}

		/* create pipeline */
		if(!m_screen_pipeline)
		{
			m_screen_pipeline = std::make_shared<graphics_pipeline>();

			#if defined(APP_DEBUG) && !defined(APP_ANDROID)
			m_screen_pipeline->push_shader_src("screen_quad.vert.glsl", true);
			m_screen_pipeline->push_shader_src("screen_quad.frag.glsl", true);
			#else
			m_screen_pipeline->push_shader_spv("engine_res/shaders/spir-v/screen_quad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			m_screen_pipeline->push_shader_spv("engine_res/shaders/spir-v/screen_quad.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			#endif

			graphics_pipeline_properties pipelineProperties{};
			pipelineProperties.depthTest = false;
			pipelineProperties.width = m_extent.width;
			pipelineProperties.height = m_extent.height;
			pipelineProperties.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			pipelineProperties.culling = INVERT_VIEWPORT ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT;
			pipelineProperties.blending = false;
			pipelineProperties.renderPass = m_renderpass;
			pipelineProperties.subpassIndex = 0;

			/* layout */
			{
				VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
				VkDescriptorSetLayoutBinding binding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
				VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0x0, 1, &binding };
				VkResult result = vkCreateDescriptorSetLayout(device::get_logical(), &info, nullptr, &descriptorLayout);
				if (result != VK_SUCCESS) engine_events::vulkan_result_error.broadcast(result, "Could not create descriptor set layout");

				m_screen_pipeline->create_pipeline_layout({ descriptorLayout });
				vkDestroyDescriptorSetLayout(device::get_logical(), descriptorLayout, nullptr);
			}
			m_screen_pipeline->create_pipeline(pipelineProperties);
		}
	}

	void swapchain::create(extent2d extent, bool vsync)
	{
		m_vsync = vsync;

		if (!m_vsync)
		{
			if (!supports_nonvsync_mode())
			{
				m_vsync = true;
				LOG_ENGINE(warn, "non vsync mode asked but not supported");
			}
		}

		VkSwapchainKHR oldSwapChain = m_swapchain;

		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device::get_physical(), m_surface, &surfaceCapabilities);

		if(rt::get_frames_in_flight_count() < surfaceCapabilities.minImageCount)
			rt::set_frames_in_flight_count(surfaceCapabilities.minImageCount + 1);

		uint32_t minImageCount = rt::get_frames_in_flight_count();
		if(surfaceCapabilities.maxImageCount > 0) /* 0 means no limit */
		{
			if(minImageCount > surfaceCapabilities.maxImageCount)
			{
				minImageCount = surfaceCapabilities.maxImageCount;
				rt::set_frames_in_flight_count(minImageCount);

				if(minImageCount < 3)
					LOG_ENGINE(warn, "swapchain can not hold 3 or more images, triple-buffering not possible with this device");

				if(minImageCount < 2)
				{
					LOG_ENGINE(error, "This device's vulkan driver supports only 1 swapchain image. This application requires at least 2 to function properly");
					system::error_msg("This device's vulkan driver supports only 1 swapchain image\nthis application requires at least 2 to function properly");
					exit(-1);
				}
			}
		}

		VkSwapchainCreateInfoKHR swapchainCreateInfo{};
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.pNext = nullptr;
		swapchainCreateInfo.flags = 0x0;
		swapchainCreateInfo.surface = m_surface;
		swapchainCreateInfo.minImageCount = minImageCount;
		swapchainCreateInfo.imageFormat = m_surface_format.format;
		swapchainCreateInfo.imageColorSpace = m_surface_format.colorSpace;

		if (surfaceCapabilities.currentExtent.width == UINT32_MAX)
		{
			m_extent.width = std::clamp(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);;
			m_extent.height = std::clamp(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
		}
		else
		{
			m_extent.width = surfaceCapabilities.currentExtent.width;
			m_extent.height = surfaceCapabilities.currentExtent.height;
		}

		swapchainCreateInfo.imageExtent = VkExtent2D{ m_extent.width , m_extent.height };
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

		/* family indices and index count are only needed for VK_SHARING_MODE_CONCURRENT which we're not using here */
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = nullptr;

		swapchainCreateInfo.preTransform = m_surface_transform;
		swapchainCreateInfo.compositeAlpha = m_alpha_composite;
		swapchainCreateInfo.presentMode = m_vsync ? m_vsync_mode : m_nonvsync_mode;

		/* unless we need to sample from the swapchain image, this should always be true */
		swapchainCreateInfo.clipped = VK_TRUE;

		/* from the docs: oldSwapchain may aid in the resource reuse, and also allows the application to still present any images that are already acquired from it. */
		swapchainCreateInfo.oldSwapchain = oldSwapChain;

		VkResult createSwapchainResult = vkCreateSwapchainKHR(device::get_logical(), &swapchainCreateInfo, nullptr, &m_swapchain);

		if (createSwapchainResult != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(createSwapchainResult, "Could not create Swapchain");

		LOG_ENGINE(trace, "Created swapchain");

		/* if we are recreating the swapchain i.e. resizing or minimizing */
		if (oldSwapChain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(device::get_logical(), oldSwapChain, nullptr);


		//-------GET SWAPCHAIN IMAGES------------------------------------------------------------
		{
			uint32_t swapchainImageCount = 0;

			vkGetSwapchainImagesKHR(device::get_logical(), m_swapchain, &swapchainImageCount, nullptr);
			m_swapchain_images.resize(swapchainImageCount);

			VkResult getSwapchainImagesResult = vkGetSwapchainImagesKHR(device::get_logical(), m_swapchain, &swapchainImageCount, m_swapchain_images.data());
			if (getSwapchainImagesResult != VK_SUCCESS)
				engine_events::vulkan_result_error.broadcast(getSwapchainImagesResult, "Could not get swapchain images");

			LOG_ENGINE(trace, "got %u swapchain images", swapchainImageCount);
		}

		create_framebuffers();

		auto appWindow = gensou_app::get()->get_window();
		if(m_extent != appWindow->get_extent())
			appWindow->resize(m_extent.width, m_extent.height);
	}

	/* resets all graphics commands */
	void swapchain::wait_for_cmds()
	{
		renderer::wait_render_cmds();

		command_manager::reset_general_pools();
		command_manager::reset_all_render_pools();
	}

	uint32_t swapchain::acquire_next_image(VkFence inFence /*= VK_NULL_HANDLE*/)
	{
		vkAcquireNextImageKHR(device::get_logical(), m_swapchain, UINT64_MAX, m_semaphores.image_acquired, inFence, &m_current_image_index);
		return m_current_image_index;
	}

	void swapchain::present(uint32_t frame)
	{
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapchain;
		presentInfo.pImageIndices = &m_current_image_index;
		presentInfo.pWaitSemaphores = &m_semaphores.render_complete;
		presentInfo.waitSemaphoreCount = 1;

		auto& pool = command_manager::s_render_graphics_pools[frame];

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = pool.recorded_cmd_count;
		submitInfo.pCommandBuffers = pool.cmd_buffers.data();

		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		submitInfo.pWaitDstStageMask = &waitStageMask;
		submitInfo.pWaitSemaphores = &m_semaphores.image_acquired;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_semaphores.render_complete;
		submitInfo.signalSemaphoreCount = 1;

		auto fence = pool.next_fence();

		vkResetFences(device::get_logical(), 1, &fence);

		VkResult presentResult;
		{
			BENCHMARK("submit & present");

			std::unique_lock<std::mutex> lock(*(pool.queue_mutex));
			vkQueueSubmit(pool.queue, 1, &submitInfo, fence);

			presentResult = vkQueuePresentKHR(device::get_present_queue(), &presentInfo);
		}

		if (presentResult != VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR)
		{
			LOG_ENGINE(warn, "swapchain present result was '%s'", get_vulkan_result_as_string(presentResult));

			command_manager::reset_general_pools();
			command_manager::reset_all_render_pools();

			if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
			{
				LOG_ENGINE(warn, "swapchain out of date");
				create(m_extent, m_vsync);
			}

			return;
		}

		frame = (frame + 1) % runtime::get_frames_in_flight_count();
		BENCHMARK_VERBOSE("swapchain wait for fences");

		/* reset_all_render_pools waits on all its submited commands (fences) for the current main frame (next frame for the render thread) */
		BENCHMARK("reset_render_pool(nextFrame)");
		command_manager::reset_render_pool(frame);
	}

	void swapchain::on_resize(uint32_t width, uint32_t height)
	{
		/* check if not minimized */
		if (width + height == 0 || (m_extent == extent2d(width, height)))
			return;

		wait_for_cmds();
		create(extent2d(width, height), m_vsync);

		LOG_ENGINE(trace, "swapchain attachments updated with size [%u x %u]", width, height);
	}

	void swapchain::create_renderpass()
	{
		if(m_renderpass != VK_NULL_HANDLE)
			vkDestroyRenderPass(device::get_logical(), m_renderpass, nullptr);

		VkAttachmentDescription attachment{};
		attachment.flags = 0x0;
		attachment.format = m_surface_format.format;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; /* needs to be stored in order to be presented */
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; /*  dont't care about past components as we clear the attachment*/
		attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference attachmentReference{};
		attachmentReference.attachment = 0;
		attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &attachmentReference;
		subpass.pDepthStencilAttachment = nullptr;

		VkSubpassDependency dependencies[2] = { VkSubpassDependency{} };

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = 0;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = 0x0; /* VK_DEPENDENCY_BY_REGION_BIT for subpass input */

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments = &attachment;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;
		renderPassCreateInfo.dependencyCount = 2;
		renderPassCreateInfo.pDependencies = dependencies;

		VkResult result = vkCreateRenderPass(device::get_logical(), &renderPassCreateInfo, nullptr, &m_renderpass);
			if (result != VK_SUCCESS)
				engine_events::vulkan_result_error.broadcast(result, "Could not create screen renderpass");
	}

	void swapchain::create_framebuffers()
	{
		if (m_renderpass == VK_NULL_HANDLE)
			create_renderpass();

		for (auto& framebuffer : m_framebuffers)
			framebuffer.clear();

		m_framebuffers.resize(m_swapchain_images.size());

		for (size_t i = 0; i < m_framebuffers.size(); i++)
		{
			m_framebuffers[i].set_attachment(0, m_swapchain_images[i], m_surface_format.format, m_extent);
			m_framebuffers[i].set_clear_value(0, { 0.0f, 0.0f, 0.0f, 1.0f });
			m_framebuffers[i].set_clear_value_count(1);
			m_framebuffers[i].create(m_renderpass);
		}
	}

	void swapchain::create_semaphores()
	{
		//keep reference for cleaning if necessary
		VkSemaphore oldAcquiredImage = m_semaphores.image_acquired;
		VkSemaphore oldRenderComplete = m_semaphores.render_complete;

		VkSemaphoreCreateInfo semaphoreCreateInfo{};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkResult imageAcquiredSmaphoreCreation = vkCreateSemaphore(device::get_logical(), &semaphoreCreateInfo, nullptr, &m_semaphores.image_acquired);
		if (imageAcquiredSmaphoreCreation != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(imageAcquiredSmaphoreCreation, "Could not create image acquired semaphore");

		VkResult renderCompleteSmaphoreCreation = vkCreateSemaphore(device::get_logical(), &semaphoreCreateInfo, nullptr, &m_semaphores.render_complete);
		if (renderCompleteSmaphoreCreation != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(imageAcquiredSmaphoreCreation, "Could not create render complete semaphore");

		//-----------------------------------------------------------------
		if (oldAcquiredImage != VK_NULL_HANDLE)
			vkDestroySemaphore(device::get_logical(), oldAcquiredImage, nullptr);

		if (oldRenderComplete != VK_NULL_HANDLE)
			vkDestroySemaphore(device::get_logical(), oldRenderComplete, nullptr);

		LOG_ENGINE(trace, "Created present semaphores");
	}

	void swapchain::terminate()
	{
		if(m_semaphores.image_acquired != VK_NULL_HANDLE)
			vkDestroySemaphore(device::get_logical(), m_semaphores.image_acquired, nullptr);

		if(m_semaphores.render_complete  != VK_NULL_HANDLE)
			vkDestroySemaphore(device::get_logical(), m_semaphores.render_complete, nullptr);

		if (m_renderpass != VK_NULL_HANDLE)
			vkDestroyRenderPass(device::get_logical(), m_renderpass, nullptr);

		for (auto& framebuffer : m_framebuffers)
			framebuffer.clear();

		if (m_swapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device::get_logical(), m_swapchain, nullptr);
			LOG_ENGINE(trace, "Destroyed swapchain");
		}

		if(m_surface != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(device::get_instance(), m_surface, nullptr);
			LOG_ENGINE(trace, "destroyed surface");
		}
	}

}