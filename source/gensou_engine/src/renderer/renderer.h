/*
* there is no way to make sure how many swapchain images we'll get from the OS,
* nor are there any garantees the acquire_swapcahin_image will return the images
* sequentially or that their indices will match the current frame in flight
* for that reason we'll render the scene to an offscreen buffer and copy its contents
* to the acquired swapchain image before presenting. that way we can keep the frame in flight
* system independant from the windowing system
*/

#pragma once

#include "core/core.h"
#include "core/cmd_queue.h"
#include "core/uuid.h"
#include "core/misc.h"

#include "renderer/memory_manager.h"
#include "renderer/command_manager.h"
#include "renderer/buffer.h"
#include "renderer/framebuffer.h"
#include "renderer/descriptor_set.h"
#include "renderer/geometry/cube.h"
#include "renderer/geometry/lines.h"

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>


namespace gs {

	class image2d;
	class texture;
	class graphics_pipeline;
	class compute_pipeline;
	class swapchain;
	class SpriteComponent;

	class renderer
	{
		friend class scene;

	public:
		~renderer();

		static renderer* get() { return s_instance; }

		static void init();
		static void terminate();

		static void submit_quad(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float squash, bool mirrorTexture);
		static void submit_quad(const glm::vec2& size, const glm::mat4& transform, const glm::vec4& color);

		static void submit_line(const glm::vec2& edgeRange, const glm::vec3& p1Pos, const glm::vec4& p1Color, const glm::vec3& p2Pos, const glm::vec4& p2color);
		static void submit_line_range(const line_vertex* start, size_t count, const glm::vec2& edgeRange);
		static void submit_cube(const glm::vec4& color, const glm::mat4& transform);
		
		static void render(std::shared_ptr<swapchain> swapchain) { s_instance->render_internal(swapchain); }
		static void reset_render_cmds() { s_instance->reset_render_cmds_internal(true); }

		static void set_clear_value(const glm::vec4& color);

		static void on_resize(uint32_t x, uint32_t y) { s_instance->on_resize_internal(x, y); }
		static void update_view_projection(const glm::mat4& viewProjection, uint32_t frame);

		static uint32_t get_quad_count() { return s_instance->m_quad_count; }
		static uint32_t get_total_quad_count();

		/* ----- post-process ----- */
		static std::shared_ptr<texture> get_blur_texture(uint32_t frame) { return s_instance->m_blur_textures[frame]; }

		static void enable_post_process(bool enable) { s_enable_post_process = enable; }
		static bool is_post_process_enabled() { return s_enable_post_process; }
		
		static void set_blur_downscale_factor(uint32_t factor) { s_blur_downscale_factor = factor; }
		static uint32_t get_blur_downscale_factor() { return s_blur_downscale_factor; }

		static void override_white_texture(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride);

		static void wait_render_cmds()
		{
			if(s_render_complete_future.valid())
				s_render_complete_future.wait();
		};

		static void set_future(std::future<void>&& future)
		{ 
			s_render_complete_future = std::forward<std::future<void>>(future);
		}

		/* will execute between the current frame in flight submissions (between waitForCmd and submitCmd) */
		/* it's the only way to safely update bound vulkan resources */
		template<typename Functor>
		static void submit_pre_render_cmd(Functor&& functor)
		{
			auto commandfn = [](void* functor_ptr)
			{
				auto pFunctorAsCmd = (Functor*)functor_ptr;
				(*pFunctorAsCmd)();
				pFunctorAsCmd->~Functor();
			};

			auto cmdBuffer = s_instance->m_pre_render_cmds.enqueue(commandfn, sizeof(Functor));
			new(cmdBuffer) Functor(std::forward<Functor>(functor));
		}

	private:
		renderer();

		void submit_quad_internal(uint32_t textureId, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float squash, bool mirrorTexture);
		void submit_cube_internal(const glm::vec4& color, const glm::mat4& transform);
		void reset_render_cmds_internal(bool resetWhiteTexture);
		void render_internal(std::shared_ptr<swapchain> sc);
		void on_resize_internal(uint32_t x, uint32_t y);
		void blur(VkCommandBuffer cmd, uint32_t frame, std::shared_ptr<image2d> attachment, quad_area blurArea, uint32_t blurCount = 1);

	private:
		static renderer* s_instance;
		static bool s_enable_post_process;

		/* for gaussian blur only */
		static uint32_t s_blur_downscale_factor;

		static std::future<void> s_render_complete_future;

	private:
		/* ----- geometry ----- */
		cube_geometry m_cubes;
		// lines are optional and customizable
		line_geometry m_lines;

		/* ----- gpu data ----- */
		/* single vertex buffer with offsets per frame */
		size_t m_vertex_buffer_capacity = 0;
		buffer<cpu_to_gpu> m_vertex_buffer;
		buffer<gpu_only> m_index_buffer;

		/* ----- working buffer ----- */
		buffer<no_vma_cpu> m_vertices;
		uint32_t m_quad_count = 0; /* number of quads submitted to the m_vertices buffer */

		cmd_queue m_pre_render_cmds;

		/* one for the app/main thread and 3 for the render thread(one per frame in flight) */
		draw_call m_working_draw_calls;
		std::array< draw_call, MAX_FRAMES_IN_FLIGHT> m_draw_calls;

		/* internal sampler to sample the scene's framebuffer color attachment in the screen
		 * and gaussian blur passes as well as sample the blurred image in the ui pass */
		VkSampler m_sampler = VK_NULL_HANDLE;

		/* scene pass & attachments */
		VkRenderPass m_renderpass = VK_NULL_HANDLE;
		std::array<framebuffer<2>, MAX_FRAMES_IN_FLIGHT> m_framebuffers;

		std::shared_ptr<graphics_pipeline> m_texture_pipeline, m_line_pipeline, m_cube_pipeline;
		std::array<texture_batch_descriptor, MAX_FRAMES_IN_FLIGHT> m_texture_descriptors;

		/* use offsets for frame in flight */
		buffer<cpu_to_gpu> m_camera_ubo; 
		std::array<descriptor_set, MAX_FRAMES_IN_FLIGHT> m_camera_descriptors;

		/* ----- screen/swapchain pass ----- */
		/* for copying(sampling) the rendered scene into the swapchain image for presenting.
		 * Framebuffers, renderpass and pipeline will be managed by the swapchain itself
		 * as they all rely on the swapchain image and its format(renderpass compatibility)
		 * which are retrieved at runtime and can be destroyed and reacreated many times
		 * throughout the application's lifetime */
		
		std::array<descriptor_set, MAX_FRAMES_IN_FLIGHT> m_screen_texture_descriptors;
		
		/* ----- ui ----- */
		/* uses the same color attachment as the scene pass but without a depth attachment */
		VkRenderPass m_ui_renderpass = VK_NULL_HANDLE;
		std::array<framebuffer<1>, MAX_FRAMES_IN_FLIGHT> m_ui_framebuffers;

		/* ----- blur ----- */
		std::shared_ptr<compute_pipeline> m_blur_pipeline;

		/* 2 per frame in flight */
		std::array<std::shared_ptr<image2d>, MAX_FRAMES_IN_FLIGHT * 2UL> m_blur_images; 
		std::array<descriptor_set, MAX_FRAMES_IN_FLIGHT * 2UL> m_blur_descriptors;

		/* for sampling the final blur in the ui pass */
		std::array<std::shared_ptr<texture>, MAX_FRAMES_IN_FLIGHT> m_blur_textures; 

		struct blur_push_constant
		{
			uint32_t x_offset = 0, y_offset = 0;
			uint32_t horizontal_pass = 1; /* to be passed as a bool */
		};
	};

}
