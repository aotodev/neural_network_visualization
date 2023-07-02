#pragma once

#include "core/core.h"
#include "renderer/device.h"

#include "core/system.h"
#include "core/runtime.h"
#include "core/engine_events.h"

#include <vulkan/vulkan.h>

namespace gs {

	class command_buffer
	{
		friend class command_pool;
		friend class command_manager;
		friend class swapchain;

		command_buffer() = default;
		command_buffer(VkCommandBuffer cmd, class command_pool* pool) : m_cmd_buffer(cmd), m_cmd_pool(pool) {}

	public:
		operator VkCommandBuffer() { return m_cmd_buffer; }

	private:
		VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
		class command_pool* m_cmd_pool = nullptr;

		operator bool() const { return m_cmd_buffer != VK_NULL_HANDLE && m_cmd_pool != nullptr; }
	};

	class command_pool
	{
		friend class command_manager;
		friend class swapchain;

		VkCommandPool pool = VK_NULL_HANDLE;
		VkQueue queue = VK_NULL_HANDLE;
		std::mutex* queue_mutex = nullptr;

		std::vector<VkCommandBuffer> cmd_buffers;

		/* resets every frame. cmds are recycled, not deallocated */
		uint32_t recorded_cmd_count = 0;

		/* keeps track of the # of allocated cmds */
		uint32_t created_cmd_buffer_count = 0;

		/* to signal when we'll be able reset the pool.*/
		std::vector<VkFence> fences;

		/* resets every frame. fences are also recycled, not deallocated */
		uint32_t fences_in_use_count = 0;

		/* keeps track of the # of created fences */
		uint32_t created_fences_count = 0;

		VkCommandBuffer next_cmd()
		{
			size_t size = cmd_buffers.size();

			if (size < recorded_cmd_count)
				cmd_buffers.resize(size * 2, VK_NULL_HANDLE);

			auto cmd = cmd_buffers[recorded_cmd_count];

			if (cmd == VK_NULL_HANDLE)
			{
				VkCommandBufferAllocateInfo info{};
				info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				info.commandPool = pool;
				info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				info.commandBufferCount = 1;

				VkResult result = vkAllocateCommandBuffers(device::get_logical(), &info, &cmd);
				if (result != VK_SUCCESS)
					engine_events::vulkan_result_error.broadcast(result, "Could not create commad buffer");

				cmd_buffers[recorded_cmd_count] = cmd;
				created_cmd_buffer_count++;
			}

			recorded_cmd_count++;

			return cmd;
		}

		VkFence next_fence()
		{
			size_t size = fences.size();
			if (size < fences_in_use_count)
				fences.resize(size * 2, VK_NULL_HANDLE);

			auto fence = fences[fences_in_use_count];

			if (fence == VK_NULL_HANDLE)
			{
				VkFenceCreateInfo info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };

				VkResult result = vkCreateFence(device::get_logical(), &info, nullptr, &fence);
				if (result != VK_SUCCESS)
					engine_events::vulkan_result_error.broadcast(result, "Could not create fence");

				fences[fences_in_use_count] = fence;
				created_fences_count++;
			}

			fences_in_use_count++;

			return fence;
		}

		void reset()
		{
			//std::unique_lock<std::mutex> lock(*queue_mutex);

			if (fences_in_use_count)
				vkWaitForFences(device::get_logical(), fences_in_use_count, fences.data(), VK_TRUE, UINT64_MAX);

			if (recorded_cmd_count)
				vkResetCommandPool(device::get_logical(), pool, 0x0);

			fences_in_use_count = 0;
			recorded_cmd_count = 0;
		}

		void clear()
		{
			reset();

			if (created_cmd_buffer_count)
				vkFreeCommandBuffers(device::get_logical(), pool, created_cmd_buffer_count, cmd_buffers.data());

			for (auto& fence : fences)
			{
				if (fence != VK_NULL_HANDLE)
				{
					vkDestroyFence(device::get_logical(), fence, nullptr);
					fence = VK_NULL_HANDLE;
				}
			}

			cmd_buffers.clear();
			fences.clear();

			if (pool != VK_NULL_HANDLE)
			{
				vkDestroyCommandPool(device::get_logical(), pool, nullptr);
				pool = VK_NULL_HANDLE;
			}

			queue = VK_NULL_HANDLE;
			queue_mutex = nullptr;
			created_cmd_buffer_count = 0;
			created_fences_count = 0;
		}
	};

	class command_manager
	{
		friend class renderer;
		friend class swapchain;

	public:
		static void init();
		static void terminate();

		static command_buffer get_cmd_buffer(queue_family family, std::thread::id threadId);
		static command_buffer get_loading_cmd_buffer(queue_family type);
		static command_buffer get_render_cmd_buffer(uint32_t frame);

		static std::mutex* get_graphics_queue_mutex();
		static std::mutex* get_compute_queue_mutex();
		static std::mutex* get_transfer_queue_mutex();

		static VkResult submit(command_buffer& cmdBuffer, bool waitOnCmds = false);

		/* submits all recorded commands from all threads for a specific frame */
		static VkResult submit_all_render_cmds(uint32_t frame, bool waitOnCmds = false, VkSemaphore* waitSemaphores = nullptr, uint32_t waitCount = 0, VkSemaphore* signalSemaphores = nullptr, uint32_t signalCount = 0);

		/* resets poll after waiting on all fences for the current frame in fligth */
		static void reset_cmd_pool(queue_family family, std::thread::id threadId);

		static void reset_general_pools();
		static void reset_loading_pools();

		/* resets poll after waiting on all fences for a specific frame in flight */
		static void reset_render_pool(uint32_t frame);

		/* resets all polls after waiting on all fences for all threads and all frames in flight*/
		static void reset_all_pools();

		/* resets polls after waiting on all fences for all frames in flight */
		static void reset_all_render_pools();

		/* waits for fences but does not reset the pools */
		static void wait_all_render_cmds();

		static void reset_general_pool(queue_family family);

	private:
		static command_pool* get_cmd_pool(queue_family family, std::thread::id threadId);

		/* for loading thread, only one "frame in flight" since no presenting is done from this thread */
		static command_pool s_loading_graphics_pool, s_loading_transfer_pool;

		/* for submiting to the same VkQueue from different threads */
		static std::mutex s_graphics_queue_mutex, s_compute_queue_mutex, s_transfer_queue_mutex;

		/* for main thread */
		static command_pool s_graphics_pool, s_compute_pool, s_transfer_pool;

		/* for render thread (main thread) */
		static std::array<command_pool, MAX_FRAMES_IN_FLIGHT> s_render_graphics_pools;

	};
}