#include "renderer/command_manager.h"

#include "renderer/device.h"

namespace gs {

	////////////////////////////////////////////////////////////////////////////////////
	// STATICS
	////////////////////////////////////////////////////////////////////////////////////

	command_pool command_manager::s_loading_graphics_pool;
	command_pool command_manager::s_loading_transfer_pool;

	std::mutex command_manager::s_graphics_queue_mutex;
	std::mutex command_manager::s_compute_queue_mutex;
	std::mutex command_manager::s_transfer_queue_mutex;

	/* for main thread */
	command_pool command_manager::s_graphics_pool;
	command_pool command_manager::s_compute_pool;
	command_pool command_manager::s_transfer_pool;

	/* for render thread (main thread) */
	std::array<command_pool, MAX_FRAMES_IN_FLIGHT> command_manager::s_render_graphics_pools;

	////////////////////////////////////////////////////////////////////////////////////



	void command_manager::init()
	{
		VkCommandPoolCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

		/* VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT for vkResetCommandBuffer;
		 * better avoid it since this flag prevents it from using a single large
		 * allocator for all buffers in the pool thus increasing memory overhead */
		info.flags = 0x0;

		info.queueFamilyIndex = device::get_graphics_family_index();

		/* loading */
		{
			/* graphics */
			{
				VkResult result = vkCreateCommandPool(device::get_logical(), &info, nullptr, &s_loading_graphics_pool.pool);

				if (result != VK_SUCCESS)
					engine_events::vulkan_result_error.broadcast(result, "Could not create loading graphics command pool");

				s_loading_graphics_pool.queue = device::get_graphics_queue();
				s_loading_graphics_pool.cmd_buffers.resize(8, VK_NULL_HANDLE);
				s_loading_graphics_pool.fences.resize(8, VK_NULL_HANDLE);
				s_loading_graphics_pool.queue_mutex = get_graphics_queue_mutex();
			}

			/* transfer */
			{
				info.queueFamilyIndex = device::get_transfer_family_index();

				VkResult result = vkCreateCommandPool(device::get_logical(), &info, nullptr, &s_loading_transfer_pool.pool);

				if (result != VK_SUCCESS)
					engine_events::vulkan_result_error.broadcast(result, "Could not create loading transfer command pool");

				s_loading_transfer_pool.queue = device::get_transfer_queue();
				s_loading_transfer_pool.cmd_buffers.resize(8, VK_NULL_HANDLE);
				s_loading_transfer_pool.fences.resize(8, VK_NULL_HANDLE);
				s_loading_transfer_pool.queue_mutex = get_transfer_queue_mutex();
			}
		}

		/* rendering only pools (one per frame) */

		info.queueFamilyIndex = device::get_graphics_family_index();

		for (auto& pool : s_render_graphics_pools)
		{
			VkResult result = vkCreateCommandPool(device::get_logical(), &info, nullptr, &pool.pool);

			if (result != VK_SUCCESS)
				engine_events::vulkan_result_error.broadcast(result, "Could not create renderer graphics command pool");

			pool.queue = device::get_graphics_queue();
			pool.cmd_buffers.resize(8, VK_NULL_HANDLE);
			pool.fences.resize(8, VK_NULL_HANDLE);
			pool.queue_mutex = get_graphics_queue_mutex();
		}

		/* general pools */
		{
			/* graphics */
			{
				VkResult result = vkCreateCommandPool(device::get_logical(), &info, nullptr, &s_graphics_pool.pool);

				if (result != VK_SUCCESS)
					engine_events::vulkan_result_error.broadcast(result, "Could not create general graphics command pool");

				s_graphics_pool.queue = device::get_graphics_queue();
				s_graphics_pool.cmd_buffers.resize(8, VK_NULL_HANDLE);
				s_graphics_pool.fences.resize(8, VK_NULL_HANDLE);
				s_graphics_pool.queue_mutex = get_graphics_queue_mutex();
			}

			/* compute */
			{
				info.queueFamilyIndex = device::get_compute_family_index();

				VkResult result = vkCreateCommandPool(device::get_logical(), &info, nullptr, &s_compute_pool.pool);

				if (result != VK_SUCCESS)
					engine_events::vulkan_result_error.broadcast(result, "Could not create general compute command pool");

				s_compute_pool.queue = device::get_compute_queue();
				s_compute_pool.cmd_buffers.resize(8, VK_NULL_HANDLE);
				s_compute_pool.fences.resize(8, VK_NULL_HANDLE);
				s_compute_pool.queue_mutex = get_compute_queue_mutex();
			}

			/* transfer */
			{
				info.queueFamilyIndex = device::get_transfer_family_index();

				VkResult result = vkCreateCommandPool(device::get_logical(), &info, nullptr, &s_transfer_pool.pool);

				if (result != VK_SUCCESS)
					engine_events::vulkan_result_error.broadcast(result, "Could not create general transfer command pool");

				s_transfer_pool.queue = device::get_transfer_queue();
				s_transfer_pool.cmd_buffers.resize(8, VK_NULL_HANDLE);
				s_transfer_pool.fences.resize(8, VK_NULL_HANDLE);
				s_transfer_pool.queue_mutex = get_transfer_queue_mutex();
			}
		}
	}

	void command_manager::terminate()
	{
		s_loading_graphics_pool.clear();
		s_loading_transfer_pool.clear();

		for (auto& pool : s_render_graphics_pools)
			pool.clear();

		s_graphics_pool.clear();
		s_compute_pool.clear();
		s_transfer_pool.clear();
	}

	command_pool* command_manager::get_cmd_pool(queue_family family, std::thread::id threadId)
	{
		if (threadId == system::get_loading_thread_id())
		{
			if (family == queue_family::transfer)
			{
				LOG_ENGINE(trace, "requesting loading transfer queue");
				return &s_loading_transfer_pool;
			}
			else if (family == queue_family::graphics || family == queue_family::compute)
			{
				LOG_ENGINE(trace, "requesting loading graphics queue");
				return &s_loading_graphics_pool;
			}
		}

		/* these are the only threads that should be abble to record and submit vulkan cmds */
		if (threadId != system::get_main_thread_id())
		{
			LOG_ENGINE(error, "submiting vulkan commands to a wrong thread (id %xll), main thread id = %xll", threadId, system::get_main_thread_id());
			assert(false);
			return nullptr;
		}

		switch (family)
		{
		case queue_family::graphics:
		{
			LOG_ENGINE(trace, "requesting general graphics queue");
			return &s_graphics_pool;
		}
		case queue_family::compute:
		{
			LOG_ENGINE(trace, "requesting general compute queue");
			return &s_compute_pool;
		}
		case queue_family::transfer:
		{
			LOG_ENGINE(trace, "requesting general transfer queue");
			return &s_transfer_pool;
		}

		default: return nullptr;
		}
	}

	command_buffer command_manager::get_cmd_buffer(queue_family family, std::thread::id threadId)
	{
		command_buffer outCmd{};

		if (auto pool = get_cmd_pool(family, threadId))
		{
			LOG_ENGINE(info, "new cmd buffer");

			outCmd.m_cmd_pool = pool;
			outCmd.m_cmd_buffer = pool->next_cmd();
		}

		return outCmd;
	}

	command_buffer command_manager::get_loading_cmd_buffer(queue_family family)
	{
		command_buffer outCmd{};

		if (family == queue_family::transfer)
		{
			outCmd.m_cmd_pool = &s_loading_transfer_pool;
			outCmd.m_cmd_buffer = outCmd.m_cmd_pool->next_cmd();
		}
		else if (family == queue_family::graphics || family == queue_family::compute)
		{
			outCmd.m_cmd_pool = &s_loading_graphics_pool;
			outCmd.m_cmd_buffer = outCmd.m_cmd_pool->next_cmd();
		}

		return outCmd;
	}

	command_buffer command_manager::get_render_cmd_buffer(uint32_t frame)
	{
		assert(std::this_thread::get_id() == system::get_render_thread_id());

		command_buffer outCmd{};
		outCmd.m_cmd_pool = &s_render_graphics_pools[frame];
		outCmd.m_cmd_buffer = outCmd.m_cmd_pool->next_cmd();
		return outCmd;
	}

	VkResult command_manager::submit(command_buffer& cmdBuffer, bool waitOnCmds)
	{
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer.m_cmd_buffer;

		auto fence = cmdBuffer.m_cmd_pool->next_fence();

		VkResult result;

		vkResetFences(device::get_logical(), 1, &fence);

		std::unique_lock<std::mutex> lock(*(cmdBuffer.m_cmd_pool->queue_mutex));
		result = vkQueueSubmit(cmdBuffer.m_cmd_pool->queue, 1, &submitInfo, fence);

		if (waitOnCmds)
			vkWaitForFences(device::get_logical(), 1, &fence, VK_TRUE, UINT64_MAX);

		return result;
	}

	std::mutex* command_manager::get_graphics_queue_mutex() { return &s_graphics_queue_mutex; }
	std::mutex* command_manager::get_compute_queue_mutex()
	{
		return device::is_compute_queue_same_as_graphics() ?
			&s_graphics_queue_mutex : &s_compute_queue_mutex;
	}

	std::mutex* command_manager::get_transfer_queue_mutex()
	{
		if (device::is_transfer_queue_same_as_graphics()) return &s_graphics_queue_mutex;
		if (device::is_transfer_queue_same_as_compute()) return get_compute_queue_mutex();

		return &s_transfer_queue_mutex;
	}

	VkResult command_manager::submit_all_render_cmds(uint32_t frame, bool waitOnCmds, VkSemaphore* waitSemaphores, uint32_t waitCount, VkSemaphore* signalSemaphores, uint32_t signalCount)
	{
		//assert(std::this_thread::get_id() == system::get_main_thread_id());
		assert(std::this_thread::get_id() == system::get_render_thread_id());

		uint32_t cmdCount = s_render_graphics_pools[frame].recorded_cmd_count;

		if (!cmdCount)
			return VK_SUCCESS;

		auto& pool = s_render_graphics_pools[frame];

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = cmdCount;
		submitInfo.pCommandBuffers = pool.cmd_buffers.data();

		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		submitInfo.pWaitDstStageMask = waitCount ? &waitStageMask : nullptr;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.waitSemaphoreCount = waitCount;
		submitInfo.pSignalSemaphores = signalSemaphores;
		submitInfo.signalSemaphoreCount = signalCount;

		auto fence = s_render_graphics_pools[frame].next_fence();

		VkResult result;

		vkResetFences(device::get_logical(), 1, &fence);

		std::unique_lock<std::mutex> lock(*(s_render_graphics_pools[frame].queue_mutex));
		result = vkQueueSubmit(s_render_graphics_pools[frame].queue, 1, &submitInfo, fence);

		if (waitOnCmds)
			vkWaitForFences(device::get_logical(), 1, &fence, VK_TRUE, UINT64_MAX);

		return result;
	}

	void command_manager::reset_cmd_pool(queue_family family, std::thread::id threadId)
	{
		if (auto pool = get_cmd_pool(family, threadId))
			pool->reset();
	}

	void command_manager::reset_loading_pools()
	{
		s_loading_graphics_pool.reset();
		s_loading_transfer_pool.reset();
	}

	void command_manager::reset_general_pools()
	{
		s_graphics_pool.reset();
		s_compute_pool.reset();
		s_transfer_pool.reset();
	}

	void command_manager::reset_all_pools()
	{
		s_loading_graphics_pool.reset();
		s_loading_transfer_pool.reset();

		for (auto& pool : s_render_graphics_pools)
			pool.reset();

		s_graphics_pool.reset();
		s_compute_pool.reset();
		s_transfer_pool.reset();
	}

	void command_manager::reset_render_pool(uint32_t frame)
	{
		s_render_graphics_pools[frame].reset();
	}

	void command_manager::reset_all_render_pools()
	{
		for (auto& pool : s_render_graphics_pools)
			pool.reset();
	}

	void command_manager::wait_all_render_cmds()
	{
		for (auto& pool : s_render_graphics_pools)
		{
			if (pool.fences_in_use_count)
				vkWaitForFences(device::get_logical(), pool.fences_in_use_count, pool.fences.data(), VK_TRUE, UINT64_MAX);
		}
	}

	void command_manager::reset_general_pool(queue_family family)
	{
		switch (family)
		{
			case queue_family::graphics: s_graphics_pool.reset(); break;
			case queue_family::compute: s_compute_pool.reset(); break;
			case queue_family::transfer: s_transfer_pool.reset(); break;

			default: break;
		}
	}

}