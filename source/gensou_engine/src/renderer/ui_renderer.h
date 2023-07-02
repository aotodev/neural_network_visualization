#pragma once

#include "core/core.h"
#include "core/system.h"
#include "core/misc.h"

#include "scene/components.h"
#include "scene/game_object.h"
#include "scene/sprite.h"

#include "renderer/descriptor_set.h"
#include "renderer/buffer.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <stb_truetype.h>

namespace gs {

	class graphics_pipeline;
	class texture;
	class renderpass;

	class ui_renderer
	{
		friend class scene;

	public:
		static void init(VkRenderPass renderpass, uint32_t subpassIndex);
		static void terminate();

		static ui_renderer* get() { return s_instance; }

		static void reset_cmds(bool resetWhiteTexture = true) { s_instance->reset_cmds_internal(resetWhiteTexture); }
		static void end_frame(uint32_t frame) { s_instance->end_frame_internal(frame); }
		static void on_resize(uint32_t width, uint32_t height) { s_instance->on_resize_internal(width, height); }

		static void submit_button(button_component* button, const glm::vec2& size, glm::mat4& transform, class scene* scene);

		static void submit_panel(const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float cornerRadius);
		static void submit_border(const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float cornerRadius, float thickness);
		static void submit_image(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& uvStride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float cornerRadius);
		static void submit_blurred_background_image(const glm::vec2& size, const glm::mat4& transform, const glm::vec4& color, float cornerRadius = 0.0f);

		static void submit_quad(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, bool mirrorTexture)
		{
			s_instance->submit_quad(inTexture, uv, stride, size, color, transform, 0.0f, 0.0f, 0.0f, mirrorTexture);
		}

		static void push_font(const std::string& path, float height, const std::string& fontName);
		static void submit_text(text_component* pText, const glm::mat4& transform, float quadBaseSize);
		static void submit_text(const std::string& text, float fontSize, const glm::vec4& color, const glm::mat4& transform, bool center, const std::string& fontName, float lineWidth);

		static void submit_slider(slider_component* slider, const glm::vec2& size, const glm::mat4& transform, class scene* scene);

		static void submit_toggle_switch(toggle_switch_component* toggle, const glm::vec2& size, const glm::mat4& transform, class scene* scene);

		/* health/progress bar */
		static void submit_bar(bar_component* bar, const glm::vec2& size, const glm::mat4& transform);
		static void submit_dialog_box(dialog_box_component* dialogBox, glm::mat4& transform, float baseQuadSize);

		static quad_area blur_area() { return s_instance->m_blur_area; }
		static bool using_blur() { return s_instance->m_use_blur; }

		static void set_selected_object(game_object gObj) { s_instance->m_currently_selected_object = gObj; }
		static void diselect_entity() { s_instance->m_currently_selected_object.reset(); }
		static game_object get_selected_object() { return s_instance->m_currently_selected_object; }

		/* renderer getters */

		static std::shared_ptr<graphics_pipeline> get_pipeline()
		{ 
			return s_instance->m_pipeline;
		}

		static const descriptor_set& get_texture_descriptor_set(uint32_t frame)
		{ 
			return s_instance->m_texture_descriptors[frame].get_descriptor();
		}

		static const descriptor_set& get_camera_descriptor_set()
		{ 
			return s_instance->m_camera_descriptor;
		}

		static uint32_t quad_count()
		{ 
			return s_instance->m_quad_count;
		}

		static draw_call& get_draw_calls(uint32_t frame)
		{ 
			s_instance->m_draw_calls[frame] = s_instance->m_working_draw_calls;
			return s_instance->m_draw_calls[frame];
		}

		static const void* get_vertices() { return s_instance->m_vertices.data(); }
		static size_t get_vertices_size();

		static void override_white_texture(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride);

	private:
		ui_renderer(VkRenderPass renderpass, uint32_t subpassIndex);

		void submit_quad(uint32_t textureId, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float corner, float thickness, float fade, bool mirrorTexture);
		void submit_quad(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float corner, float thickness, float fade, bool mirrorTexture);
		void submit_quad(const glm::vec2& size, const glm::mat4& transform, const glm::vec4& color, float corner = 0.0f, float thickness = 0.0f, float fade = 0.0f);
		void submit_glyph(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& leftTop, const glm::vec2& rightBotton, const glm::vec4& color);

		void reset_cmds_internal(bool resetWhiteTexture);
		void end_frame_internal(uint32_t frame);
		void on_resize_internal(uint32_t width, uint32_t height);
		void push_font_internal(const std::string& path, float height, const std::string& fontName);

		/* maxLineSize == 0.0f means no size limit */
		void render_text(const std::string& text, float fontSize, const std::string& fontName, const glm::vec4& color, const glm::mat4& transform, bool center, float maxLineSize = 0.0f);
		
	private:
		static ui_renderer* s_instance;

		VkRenderPass m_renderpass;
		uint32_t m_subpass_index = 0;

		std::shared_ptr<graphics_pipeline> m_pipeline;
		std::array<texture_batch_descriptor, MAX_FRAMES_IN_FLIGHT> m_texture_descriptors;

		/* CPU memory to serve as staging buffer, layers will be offset inside this contiguos buffer */
		buffer<no_vma_cpu> m_vertices;
		uint32_t m_quad_count = 0;

		/* one for the app/main thread and 3 for the render thread(one per frame in flight) */
		draw_call m_working_draw_calls;
		std::array<draw_call, MAX_FRAMES_IN_FLIGHT> m_draw_calls;

		/* only one set of resources for the ui camera as it is supposed to be immutable */
		camera_component m_camera;
		transform_component m_camera_transform;
		descriptor_set m_camera_descriptor;
		buffer<cpu_to_gpu> m_camera_ubo;

		game_object m_currently_selected_object;

public:
		struct font
		{
			stbtt_fontinfo info{};
			std::shared_ptr<gensou_file> font_data;
			std::vector<stbtt_bakedchar> baked_chars;
			glm::ivec2 bitmap_size{ 1024, 1024 }; //512
			std::shared_ptr<texture> font_texture;
			float height = 100.0f;
		};

private:

		font* get_font(const std::string& fontName);

		std::unordered_map<std::string, font> m_fonts_map;

		/* updated every frame */
		quad_area m_blur_area;
		bool m_use_blur = false;

	};
}