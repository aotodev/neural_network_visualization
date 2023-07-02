#include "renderer/ui_renderer.h"
#include "renderer/memory_manager.h"
#include "renderer/texture.h"
#include "renderer/pipeline.h"
#include "renderer/renderer.h"

#include "core/system.h"
#include "core/runtime.h"

#include "core/input.h"
#include "core/engine_events.h"
#include "core/misc.h"

#include "scene/components.h"
#include "scene/scene.h"
#include "scene/sprite.h"

namespace {

	struct ui_vertex
	{
		ui_vertex(const glm::vec4& vertexColor, const glm::vec2& quadSize, float corner, float thickness, float fade)
			: color(vertexColor), size(quadSize), corner_radius(corner), frame_thickness(thickness), circle_fade(fade) {}

		glm::vec3 position{ 0.0f, 0.0f, 0.0f };
		glm::vec2 uv{ 0.0f, 0.0f };
		glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec2 size{ 1.0f, 1.0f };
		float corner_radius = 0.0f; /* 0.0f == no rounded edges */
		float frame_thickness = 0.0f; /* 0.0f == no wireframe */
		float circle_fade = 0.0f; /* 0.0f == not a circle */
	};
}

namespace gs {

	static sprite s_white_texture;

	ui_renderer* ui_renderer::s_instance = nullptr;

	void ui_renderer::override_white_texture(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride)
	{
		s_white_texture.tex = inTexture;
		s_white_texture.uv = uv;
		s_white_texture.stride = stride;
	}

	void ui_renderer::init(VkRenderPass renderpass, uint32_t subpassIndex)
	{
		s_white_texture.uv = glm::vec2(0.125f);
		s_white_texture.uv = glm::vec2(0.75f);

		s_instance = new ui_renderer(renderpass, subpassIndex);
	}

	void ui_renderer::terminate()
	{
		s_white_texture.reset();

		if (s_instance)
			delete s_instance;
	}

	ui_renderer::ui_renderer(VkRenderPass renderpass, uint32_t subpassIndex)
		: m_renderpass(renderpass),
		  m_subpass_index(subpassIndex),
		  m_vertices(sizeof(ui_vertex) * 4ULL * 256ULL),
		  m_camera_ubo(sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr, 0),
		  m_blur_area((float)rt::viewport().width * 0.5f, (float)rt::viewport().height * 0.5f, 0.0f, 0.0f)
	{
		const uint32_t frameCount = runtime::get_frames_in_flight_count();

		/* descriptors */
		{
			m_camera.set_orthographic(runtime::viewport().height, -1.0f, 1.0f);
			m_camera.set_viewport_size(runtime::viewport().width, runtime::viewport().height);

			VkDescriptorBufferInfo cameraDescriptorInfo{};
			cameraDescriptorInfo.offset = 0;
			cameraDescriptorInfo.range = sizeof(glm::mat4); // projection * view matrix
			cameraDescriptorInfo.buffer = m_camera_ubo.get();

			m_camera_descriptor.create(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
			m_camera_descriptor.update(0, &cameraDescriptorInfo, 1, 0);

			m_camera.update(m_camera_transform);

			auto projectionView = m_camera.get_projection_view();
			m_camera_ubo.write(&projectionView, sizeof(glm::mat4), 0);
		}

		// pipeline
		{
			m_pipeline = std::make_shared<graphics_pipeline>();

			#if defined(APP_DEBUG) && !defined(APP_ANDROID)
			m_pipeline->push_shader_src("ui.vert.glsl", true);
			m_pipeline->push_shader_src("ui.frag.glsl", true);
			#else
			m_pipeline->push_shader_spv("engine_res/shaders/spir-v/ui.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			m_pipeline->push_shader_spv("engine_res/shaders/spir-v/ui.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			#endif

			graphics_pipeline_properties pipelineProperties{};
			pipelineProperties.depthTest = false;
			pipelineProperties.width = runtime::viewport().width;
			pipelineProperties.height = runtime::viewport().height;
			pipelineProperties.culling = VK_CULL_MODE_NONE; //VK_FRONT_FACE_CLOCKWISE
			pipelineProperties.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //VK_FRONT_FACE_COUNTER_CLOCKWISE
			pipelineProperties.blending = true;
			pipelineProperties.renderPass = m_renderpass;
			pipelineProperties.subpassIndex = m_subpass_index;

			/* vertex input info */
			{
				static constexpr VkVertexInputAttributeDescription vertexDescription[] =
				{
					{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ui_vertex, position) },
					{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ui_vertex, uv) },
					{ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ui_vertex, color) },
					{ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ui_vertex, size) },
					{ 4, 0, VK_FORMAT_R32_SFLOAT, offsetof(ui_vertex, corner_radius) },
					{ 5, 0, VK_FORMAT_R32_SFLOAT, offsetof(ui_vertex, frame_thickness) },
					{ 6, 0, VK_FORMAT_R32_SFLOAT, offsetof(ui_vertex, circle_fade) }
				};

				static constexpr VkVertexInputBindingDescription vertexBindingDescription{ 0, sizeof(ui_vertex), VK_VERTEX_INPUT_RATE_VERTEX };
				
				static constexpr VkPipelineVertexInputStateCreateInfo quadVertexInputState
				{
					VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
					nullptr, 0x0,
					1,&vertexBindingDescription,
					7, vertexDescription
				};

				pipelineProperties.vertexInputInfo = quadVertexInputState;
			}

			auto cameraLayout = m_camera_descriptor.get_layout();
			auto textureLayout = m_texture_descriptors[0].get_descriptor().get_layout();

			VkPushConstantRange pushRange{};
			pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			pushRange.offset = 0ULL;
			pushRange.size = sizeof(uint32_t);

			m_pipeline->create_pipeline_layout({ cameraLayout, textureLayout }, { pushRange });
			m_pipeline->create_pipeline(pipelineProperties);
		}

		push_font_internal("engine_res/fonts/opensans/opensans_bold_ttf.gsasset", 100.0f, "default");

		engine_events::window_resize.subscribe(&ui_renderer::on_resize);
	}

	void ui_renderer::submit_quad(uint32_t textureId, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float corner, float thickness, float fade, bool mirrorTexture)
	{
		const float right	= size.x / 2;
		const float left	= -right;
		const float up		= size.y / 2;
		const float down	= -up;

		const glm::mat4 baseQuad =
		{
			left,	down,	0.0f, 1.0f,
			right,	down,	0.0f, 1.0f,
			right,	up,		0.0f, 1.0f,
			left,	up,		0.0f, 1.0f
		};

		glm::mat4 position = transform * baseQuad;

		auto newColor = glm::vec4(revert_gamma_correction(glm::vec3(color)), color.a);

		uint32_t offset = m_quad_count * sizeof(ui_vertex) * 4ull;

		auto vertex0 = m_vertices.emplace<ui_vertex>(offset + sizeof(ui_vertex) * 0ull, newColor, size, corner, thickness, fade);
		auto vertex1 = m_vertices.emplace<ui_vertex>(offset + sizeof(ui_vertex) * 1ull, newColor, size, corner, thickness, fade);
		auto vertex2 = m_vertices.emplace<ui_vertex>(offset + sizeof(ui_vertex) * 2ull, newColor, size, corner, thickness, fade);
		auto vertex3 = m_vertices.emplace<ui_vertex>(offset + sizeof(ui_vertex) * 3ull, newColor, size, corner, thickness, fade);

		vertex0->position = glm::vec3(position[0]);	// top left
		vertex1->position = glm::vec3(position[1]);	// bottom right
		vertex2->position = glm::vec3(position[2]);	// top right
		vertex3->position = glm::vec3(position[3]);	// bottom left

		const float uvX0 = mirrorTexture ? uv.x + stride.x : uv.x;
		const float uvX1 = mirrorTexture ? uv.x : uv.x + stride.x;

		vertex0->uv = { uvX0, uv.y };	// top left 
		vertex1->uv = { uvX1, uv.y };	// bottom right
		vertex2->uv = { uvX1, uv.y + stride.y };	// top right
		vertex3->uv = { uvX0, uv.y + stride.y };	// bottom left

		if (m_working_draw_calls.empty())
		{
			m_working_draw_calls.emplace_back(std::make_pair(1ul, textureId));
		}
		else if (textureId == m_working_draw_calls.back().second)
		{
			m_working_draw_calls.back().first++;
		}
		else
		{
			m_working_draw_calls.emplace_back(std::make_pair(1ul, textureId));
		}

		m_quad_count++;
	}

	void ui_renderer::submit_quad(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float corner, float thickness,  float fade, bool mirrorTexture)
	{
		uint32_t texId = m_texture_descriptors[rt::current_frame()].get_texture_id(inTexture);
		submit_quad(texId, uv, stride, size, color, transform, corner, thickness, fade, mirrorTexture);
	}

	void ui_renderer::submit_quad(const glm::vec2& size, const glm::mat4& transform, const glm::vec4& color, float corner, float thickness, float fade)
	{
		submit_quad(s_white_texture.tex, s_white_texture.uv, s_white_texture.stride, size, color, transform, corner, thickness, fade, false);
	}

	void ui_renderer::submit_glyph(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& leftTop, const glm::vec2& rightBotton, const glm::vec4& color)
	{
		const float right = rightBotton.x;
		const float left = leftTop.x;
		const float up = leftTop.y;
		const float down = rightBotton.y;

		const glm::mat4 baseQuad =
		{
			left,	down,	0.0f, 1.0f,
			right,	down,	0.0f, 1.0f,
			right,	up,		0.0f, 1.0f,
			left,	up,		0.0f, 1.0f
		};

		const glm::vec2 size{ leftTop.x - rightBotton.x,  leftTop.y - rightBotton.y };
		const glm::vec4 newColor = glm::vec4(revert_gamma_correction(glm::vec3(color)), color.a);
		uint32_t offset = m_quad_count * sizeof(ui_vertex) * 4ull;

		auto vertex0 = m_vertices.emplace<ui_vertex>(offset + sizeof(ui_vertex) * 0ull, newColor, size, 0.0f, 0.0f, 0.0f);
		auto vertex1 = m_vertices.emplace<ui_vertex>(offset + sizeof(ui_vertex) * 1ull, newColor, size, 0.0f, 0.0f, 0.0f);
		auto vertex2 = m_vertices.emplace<ui_vertex>(offset + sizeof(ui_vertex) * 2ull, newColor, size, 0.0f, 0.0f, 0.0f);
		auto vertex3 = m_vertices.emplace<ui_vertex>(offset + sizeof(ui_vertex) * 3ull, newColor, size, 0.0f, 0.0f, 0.0f);

		vertex0->position = glm::vec3(baseQuad[0]);	// top left
		vertex1->position = glm::vec3(baseQuad[1]);	// bottom right
		vertex2->position = glm::vec3(baseQuad[2]);	// top right
		vertex3->position = glm::vec3(baseQuad[3]);	// bottom left

		vertex0->uv = { uv.x,				uv.y };				// top left 
		vertex1->uv = { uv.x + stride.x,	uv.y, };			// bottom right
		vertex2->uv = { uv.x + stride.x,	uv.y + stride.y };	// top right
		vertex3->uv = { uv.x,				uv.y + stride.y };	// bottom left

		uint32_t textureId = s_instance->m_texture_descriptors[rt::current_frame()].get_texture_id(inTexture);

		if (m_working_draw_calls.empty())
		{
			m_working_draw_calls.emplace_back(std::make_pair(1ul, textureId));
		}
		else if (textureId == m_working_draw_calls.back().second)
		{
			m_working_draw_calls.back().first++;
		}
		else
		{
			m_working_draw_calls.emplace_back(std::make_pair(1ul, textureId));
		}

		m_quad_count++;
	}

	void ui_renderer::reset_cmds_internal(bool resetWhiteTexture)
	{
		m_vertices.reset();
		m_quad_count = 0;
		m_use_blur = false;

		m_working_draw_calls.clear();

		for(auto& drawCall : m_draw_calls)
			drawCall.clear();

		for (auto& texDescriptor : m_texture_descriptors)
			texDescriptor.clear();

		if(resetWhiteTexture)
		{
			s_white_texture.reset();
			s_white_texture.uv = glm::vec2(0.125f);
			s_white_texture.stride = glm::vec2(0.75f);
		}

			
	}

	void ui_renderer::end_frame_internal(uint32_t frame)
	{
		m_vertices.reset();
		m_quad_count = 0;
		m_working_draw_calls.clear();

		m_blur_area.set_quad((float)rt::viewport().width * 0.5f, (float)rt::viewport().height * 0.5f, 0.0f, 0.0f);
		m_use_blur = false;
	}

	void ui_renderer::on_resize_internal(uint32_t width, uint32_t height)
	{
		m_camera.set_viewport_size(width, height);

		m_camera.update(m_camera_transform);
		auto projectionView = m_camera.get_projection_view();
		m_camera_ubo.write(&projectionView, sizeof(glm::mat4), 0);

		m_blur_area.set_quad((float)width * 0.5f, (float)height * 0.5f, 0.0f, 0.0f);
	}

	size_t ui_renderer::get_vertices_size()
	{
		return (size_t)s_instance->m_quad_count * 4ULL * sizeof(ui_vertex);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	static inline button_component::state operator|(button_component::state left, button_component::state right)
	{ 
		return (button_component::state)((uint32_t)left | (uint32_t)right);
	}

	static inline button_component::state operator&(button_component::state left, button_component::state right)
	{
		return (button_component::state)((uint32_t)left & (uint32_t)right);
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ui_renderer::submit_button(button_component* button, const glm::vec2& size, glm::mat4& transform, scene* pScene)
	{
		/*------------determine-layout--------------------------------------------------------*/
		bool enabled = button->m_game_object.is_active();
		bool selected = button->m_game_object == get_selected_object();
		bool hovered = false;

		if(enabled && !selected)
		{
			glm::vec2 scale = { glm::length(glm::vec3(transform[0])), glm::length(glm::vec3(transform[1])) };
			glm::vec2 pixelSize = { size.x * scale.x, size.y * scale.y };

			hovered = input::has_mouse_device_connected() && 
				overlaps_rect_point(glm::vec2(transform[3]), pixelSize, input::mouse_position());
		}

		/* submit renderables */
		{
			/*-------------set-layout---------------------------------------------------------*/
			glm::vec4& mainColor = 
				selected ? button->pressed_color :
					hovered ? button->hovered_color :
						button->default_color;

			glm::vec4& backgroundColor = 
				selected ? button->pressed_background_color :
					hovered ? button->hovered_background_color :
						button->default_background_color;

			glm::vec4 colorMul = enabled ? glm::vec4(1.0f) : button->disabled_color;

			float thickness, cornerRadius;

			if(size.x >= size.y)
			{
				thickness = size.y * button->border_thickness * 0.02f;
				cornerRadius = size.x * button->corner_radius;
			}
			else
			{
				thickness = size.x * button->border_thickness * 0.02f;
				cornerRadius = size.y * button->corner_radius;
			}

			/*----------------------------------------------------------------------------*/

			/* background quad */
			if (backgroundColor.a > 0.0f)
				s_instance->submit_quad(size, transform, backgroundColor * colorMul, cornerRadius);

			/* main quad */
			if(button->m_texture)
			{
				glm::vec2 textureSize = size;

				if(button->m_texture_mode != ui_texture_mode::fit_both)
				{
					float aspect = button->get_texture_aspect_ratio();

					button->m_texture_mode == ui_texture_mode::fit_height ?
						textureSize.x = size.y * aspect : textureSize.y = size.x / aspect;
				}

				textureSize *= button->texture_scale;

				s_instance->submit_quad(button->m_texture, button->m_texture_uv, button->m_texture_uv_stride,
					textureSize, mainColor * colorMul, transform, 0.0f, 0.0f, 0.0f, false);
			}
			else if (!button->label.empty())
			{
				float fontSize = std::clamp((size.y * 0.85f) * 0.01f, 0.04f, 1.0f);
				s_instance->render_text(button->label, fontSize, button->label_font, mainColor * colorMul, transform, true);
			}

			// border quad
			if((thickness > 0.0f) && (button->border_color.a > 0.0f))
			{
				s_instance->submit_border(size, button->border_color * colorMul, transform, cornerRadius, thickness);
			}
		}

		/*----button-action-------------------------------------------------------------------*/

		if (!enabled)
			return;

		if (selected)
		{
			if (!(button->m_state & button_component::pressed))
			{
				button->m_state = button->m_state | button_component::pressed;

				if (auto action = button->on_pressed_action)
					action(button, pScene, button->user_data);
			}

			return;
		}

		if (button->m_state & button_component::pressed)
		{
			/* button has just been released */

			button->m_state = (button_component::state)(button->m_state & ~button_component::pressed);

			if(hovered)
				button->m_state = button->m_state | button_component::hovered;

			if (auto action = button->on_released_action)
				action(button, pScene, button->user_data);
		}

		if (hovered)
		{
			if (!(button->m_state & button_component::hovered))
			{
				button->m_state = button->m_state | button_component::hovered;

				if (auto action = button->on_hover_started_action)
					action(button, pScene, button->user_data);
			}
		}
	}

	void ui_renderer::submit_border(const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float cornerRadius, float thickness)
	{
		/* the SDF algorithm that generates the frame needs a bit more space to generate the shape properly
		 * a factor of 1.1 is taken from the size when calculating it. multiply the size here to offset this difference
		 */
		s_instance->submit_quad(size * 1.1f, transform, color, cornerRadius, thickness, 0.0f);
	}
	
	void ui_renderer::submit_panel(const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float cornerRadius)
	{
		s_instance->submit_quad(size, transform, color, cornerRadius, 0.0f, 0.0f);
	}

	void ui_renderer::submit_image(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& uvStride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float cornerRadius)
	{
		s_instance->submit_quad(inTexture, uv, uvStride, size, color, transform, cornerRadius, 0.0f, 0.0f, false);
	}

	void ui_renderer::submit_blurred_background_image(const glm::vec2& size, const glm::mat4& transform, const glm::vec4& color, float cornerRadius)
	{
		auto frame = runtime::current_frame();

		auto&& [uv, stride] = world_position_to_uv(glm::vec2(transform[3].x, transform[3].y), size);

		float corner = size.x >= size.y ? size.x * cornerRadius : size.y * cornerRadius;

		if (renderer::is_post_process_enabled() && color.a < 1.0f)
		{
			s_instance->submit_quad(renderer::get_blur_texture(frame), uv, stride, size, glm::vec4(1.0f), transform, corner, 0.0f, 0.0f, false);

			float x0 = uv.x * (float)runtime::viewport().width;
			float y0 = uv.y * (float)runtime::viewport().height;

			auto& area = s_instance->m_blur_area;

			float x1 = x0 + size.x;
			float y1 = y0 + size.y;

			float area_x1 = area.x + area.size_x;
			float area_y1 = area.y + area.size_y;

			area.x = std::min(area.x, x0);
			area.y = std::min(area.y, y0);

			area_x1 = std::max(area_x1, x1);
			area_y1 = std::max(area_y1, y1);

			area.size_x = area_x1 - area.x;
			area.size_y = area_y1 - area.y;

			s_instance->m_use_blur = true;
		}

		if (color.a > 0.0f)
			s_instance->submit_quad(size, transform, color, corner);
	}

	void ui_renderer::push_font(const std::string& path, float height, const std::string& fontName)
	{
		s_instance->push_font_internal(path, height, fontName);
	}

	void ui_renderer::push_font_internal(const std::string& path, float height, const std::string& fontName)
	{
		if (m_fonts_map.find(fontName) != m_fonts_map.end())
		{
			LOG_ENGINE(warn, "Font with name %s already exists", fontName.c_str());
			return;
		}

		m_fonts_map.emplace(fontName, font{});
		auto& info = m_fonts_map[fontName];

		info.font_data = system::load_file(path);

		if (!info.font_data)
		{
			LOG_ENGINE(error, "Error while loading font from path '%s", path.c_str());
			m_fonts_map.erase(fontName);
			return;
		}

		if (!stbtt_InitFont(&info.info, info.font_data->data(), 0))
		{
			LOG_ENGINE(error, "Font from path '%s' was invalid", path.c_str());
			m_fonts_map.erase(fontName);
			return;
		}

		float bitmapSize = height <= 100.0f ? 512.0f : 1024.0f;
		info.bitmap_size = glm::vec2(bitmapSize, bitmapSize);

		info.baked_chars.resize(96);
		info.height = height;

		std::vector<byte> bitmap(bitmapSize * bitmapSize, 0u);
		constexpr int spaceChar = (int)' ';
		stbtt_BakeFontBitmap(info.font_data->data(), 0, height, bitmap.data(), info.bitmap_size.x, info.bitmap_size.y, spaceChar, info.baked_chars.size(), info.baked_chars.data());

		std::vector<byte> bitmapRGBA(bitmapSize * bitmapSize * 4ull, 0u);
		for (size_t i = 0; i < bitmap.size(); i++)
		{
			const size_t j = i * 4;

			bitmapRGBA[j + 0] = bitmap[i]; // r
			bitmapRGBA[j + 1] = bitmap[i]; // g
			bitmapRGBA[j + 2] = bitmap[i]; // b
			bitmapRGBA[j + 3] = bitmap[i]; // a
		}

		info.font_texture = texture::create_from_pixels(bitmapRGBA.data(), bitmapRGBA.size() * sizeof(uint8_t), extent2d(uint32_t(info.bitmap_size.x), uint32_t(info.bitmap_size.y)), false, VK_FORMAT_R8G8B8A8_UNORM);
	}

	void ui_renderer::submit_text(const std::string& text, float fontSize, const glm::vec4& color, const glm::mat4& transform, bool center, const std::string& fontName, float lineWidth)
	{
		s_instance->render_text(text, fontSize, fontName, color, transform, center);
	}

	void ui_renderer::submit_text(text_component* pText, const glm::mat4& transform, float quadBaseSize)
	{
		float fontSize = pText->font_size;
		float lineWidth = pText->line_width;

		if (pText->text_size_dynamic)
		{
			fontSize = std::clamp(fontSize * quadBaseSize * 0.01f, 0.04f, 1.0f);
			lineWidth *= quadBaseSize;
		}

		s_instance->render_text(pText->text, fontSize, pText->font, pText->color, transform, pText->center_text, lineWidth);
	}

	ui_renderer::font* ui_renderer::get_font(const std::string& fontName)
	{
		font* pFont = nullptr;
		const auto mapIterator = m_fonts_map.find(fontName);
		if(mapIterator != m_fonts_map.end())
		{
			pFont = &mapIterator->second;
		}
		else
		{
			const auto defaultFontIterator = m_fonts_map.find("default");
			if(defaultFontIterator != m_fonts_map.end())
			{
				LOG_ENGINE(warn, "FONT '%s' not found, using default one", fontName.c_str());
				pFont = &defaultFontIterator->second;
			}
			else
			{
				LOG_ENGINE(error, "NO FONT AVAILABLE, can't render text");
				return nullptr;
			}
		}

		return pFont;
	}

	/* returns the size of the text in pixel space */
	static inline float get_text_width(const std::string& text, ui_renderer::font* pFont, int32_t firstCharacterindex = 0, int32_t lastCharacterindex = 0)
	{
		assert(firstCharacterindex < text.size());

		float heightScale = stbtt_ScaleForPixelHeight(&pFont->info, pFont->height);

			float posX = 0.0f;
			float posY = 0.0f;

			int32_t breakIndex = lastCharacterindex > 0 ? lastCharacterindex : text.size();

			for (int32_t i = firstCharacterindex; i < breakIndex; i++)
			{
				char c = text[i];

				stbtt_aligned_quad quadInfo;

				/* handle unavailable characters the same way we render them */
				{
					uint8_t uc = (uint8_t)c;
					if (uc < 32 || uc >= 128)
						c = 32;
				}

				int advanceX = 0;
				int leftSideBearing = 0;
				stbtt_GetCodepointHMetrics(&pFont->info, c, &advanceX, &leftSideBearing);
				posX += (float)leftSideBearing * heightScale; // scale
				stbtt_GetBakedQuad(pFont->baked_chars.data(), pFont->bitmap_size.x, pFont->bitmap_size.y, c - 32, &posX, &posY, &quadInfo, 1);

				if (i < text.size() - 1)
				{
					char nextChar = text[i + 1];
					int kern;
					kern = stbtt_GetCodepointKernAdvance(&pFont->info, c, nextChar);
					posX += (float)kern * heightScale; // scale
				}
			}

		return posX;
	}

	void ui_renderer::render_text(const std::string& text, float fontSize, const std::string& fontName, const glm::vec4& color, const glm::mat4& transform, bool center, float maxLineSize)
	{
		font* pFont = get_font(fontName);

		if(!pFont)
			return;

		float xPos = transform[3].x;
		float yPos = transform[3].y;
		#if INVERT_VIEWPORT
		yPos *= -1.0f;
		#endif

		float heightScale = stbtt_ScaleForPixelHeight(&pFont->info, pFont->height);
		float scale = heightScale * fontSize;

		int ascent_int = 0, descent_int = 0, lineGap_int = 0;
		stbtt_GetFontVMetrics(&pFont->info, &ascent_int, &descent_int, &lineGap_int);
		float ascent = (float)ascent_int * scale;
		float descent = (float)descent_int * scale;
		float lineGap = (float)lineGap_int * scale;
		yPos -= descent;

		float nextLineOffset = ascent - descent + lineGap;

		#if INVERT_VIEWPORT
		nextLineOffset *= -1.0f;
		#endif

		nextLineOffset /= fontSize;
		xPos /= fontSize;
		yPos /= fontSize;

		if(maxLineSize > 0.5f)
			maxLineSize /= fontSize;

		if (center)
		{
			float textWidth = get_text_width(text, pFont, 0);
			xPos -= textWidth * 0.5f;
		}
		else
		{
			/* remove half ot the first character width */

			float textWidth = get_text_width(text, pFont, 0, 1);
			xPos -= textWidth * 0.5f;
		}

		float start_xPos = xPos, lineWidth = 0.0f;
		int32_t lastSpaceIndex = 0;

		for (int32_t i = 0; i < text.size(); i++)
		{
			char c = text[i];

			if(c == '\n')
			{
				yPos += nextLineOffset;
				xPos = start_xPos;

				lineWidth = 0.0f;
				lastSpaceIndex = i;

				continue;
			}

			if(c == 32)
				lastSpaceIndex = i;

			stbtt_aligned_quad quadInfo;

			/* handle unavailable characters */
			{
				uint8_t uc = (uint8_t)c;
				if (uc < 32 || uc >= 128)
					c = 32;
			}

			int advanceX = 0;
			int leftSideBearing = 0;
			stbtt_GetCodepointHMetrics(&pFont->info, c, &advanceX, &leftSideBearing);

			stbtt_GetBakedQuad(pFont->baked_chars.data(), pFont->bitmap_size.x, pFont->bitmap_size.y, c - 32, &xPos, &yPos, &quadInfo, 1);

			glm::vec2 uv{ quadInfo.s0, quadInfo.t0 };
			glm::vec2 uvStride(0.0f);
			uvStride.x = quadInfo.s1 - quadInfo.s0;
			uvStride.y = quadInfo.t1 - quadInfo.t0;

			xPos += (float)leftSideBearing * heightScale;

			/* if not-inverted viewport y0 & y1 should be positive */
			#if INVERT_VIEWPORT
			glm::vec2 topLeft = { quadInfo.x0, -quadInfo.y1 };
			glm::vec2 bottomRight = { quadInfo.x1, -quadInfo.y0 };
			#else
			glm::vec2 topLeft = { quadInfo.x0, quadInfo.y1 };
			glm::vec2 bottomRight = { quadInfo.x1, quadInfo.y0 };
			#endif

			topLeft *= fontSize;
			bottomRight *= fontSize;

			s_instance->submit_glyph(pFont->font_texture, uv, uvStride, topLeft, bottomRight, color);

			if (i < text.size() - 1) 
			{
				char nextChar = text[i + 1];
				int kern;
				kern = stbtt_GetCodepointKernAdvance(&pFont->info, c, nextChar);
				xPos += (float)kern * heightScale; // scale
			}

			if(maxLineSize > 0.5f)
			{
				lineWidth = xPos - start_xPos;
				if(lineWidth > maxLineSize)
				{
					/* override the part of the word that was already submited to the vertex buffer */
					/* -1 to remove to trailling space */
					uint32_t removeQuadCount = uint32_t(i - lastSpaceIndex + 1);
					m_quad_count -= removeQuadCount;
					m_working_draw_calls.back().first -= removeQuadCount;

					/* go to next line */
					yPos += nextLineOffset;
					xPos = start_xPos;

					/* roll back the for loop to the beginning of the current word */
					i = lastSpaceIndex;

					lineWidth = 0.0f;

					// TODO: truncate words that do not fit on a line
				}
			}
		}
	}

	void ui_renderer::submit_slider(slider_component* slider, const glm::vec2& size, const glm::mat4& transform, class scene* pScene)
	{
		const auto& range = slider->range;
		auto& value = slider->value;
		bool enabled = slider->m_game_object.is_active();

		float normalizedValue = value / range.y;
		glm::vec2 fillSize = size; fillSize.x *= normalizedValue;
		glm::mat4 fillTransform = transform; fillTransform[3].x -= (size.x - fillSize.x) * 0.5f;

		glm::vec4 color = enabled ? glm::vec4(1.0f) : slider->disabled_color;

		/* outer quad */
		if(slider->background_color.a > 0.0f)
			s_instance->submit_quad(size, transform, slider->background_color * color, size.y);

		/* inner/fill quad */
		if (value > 0.0f)
			s_instance->submit_quad(fillSize, fillTransform, slider->fill_color * color, size.y);

		if(slider->border_color.a > 0.0f)
		{
			float thickness = (size.y / size.x * 100.0f) * slider->border_thickness;
			s_instance->submit_quad(size * 1.1f, transform, slider->border_color * color, size.y, thickness);
		}

		/* handle */
		{
			auto handleTransform = fillTransform; handleTransform[3].x += fillSize.x * 0.5f;
			glm::vec2 handleSize = glm::vec2(fillSize.y * 2.64f);

			if (auto texture = slider->m_handle_texture)
			{
				s_instance->submit_quad(texture, slider->m_handle_texture_uv, slider->m_handle_texture_uv_stride,
					handleSize, slider->handle_color * color, handleTransform, 0.0f, 0.0f, 0.0f, false);
			}
			else
			{
				s_instance->submit_quad(handleSize, handleTransform, slider->handle_color * color, 0.0f, 0.0f, 0.1f); /* 0.005f */
			}
		}

		if (!slider->m_game_object.get_component<state_component>().is_active)
			return;

		if (slider->m_game_object == s_instance->m_currently_selected_object)
		{
			slider->m_is_pressed = true;

			float x = input::active_input_type() == input_type::mouse_button ?
				input::mouse_position().x : input::touch_position().x;

			float addValue = x - (transform[3].x - (size.x * 0.5f));
			addValue /= size.x;
			addValue *= range.y - range.x;
			value = std::clamp(addValue, range.x, range.y);

			if (auto action = slider->on_value_changed_action)
				action(slider, pScene, value, slider->user_data);
		}
		else
		{
			if(slider->m_is_pressed)
			{
				slider->m_is_pressed = false;
				if(auto action = slider->on_release_action)
					action(slider, pScene, value, slider->user_data);
			}
		}
	}

	void ui_renderer::submit_toggle_switch(toggle_switch_component* toggle, const glm::vec2& size, const glm::mat4& transform, class  scene* pScene)
	{
		bool enabled = toggle->m_game_object.is_active();
		glm::vec4 color = enabled ? glm::vec4(1.0f) : toggle->disabled_color;

		bool& on = toggle->m_is_on;
		glm::mat4 handleTransform = transform;
		glm::vec2 handleSize = size * toggle->handle_scale;

		float thickness = 0.0f;

		if(size.x >= size.y)
		{
			thickness = size.y * toggle->border_thickness * 0.02f;
		}
		else
		{
			thickness = size.x * toggle->border_thickness * 0.02f;
		}

		if (on)
		{
			handleTransform[3].x += size.x * 0.5f - size.y * 0.5f;

			/* toggle */
			s_instance->submit_quad(size, transform, toggle->on_color * color, size.y);

			/* border */
			if(toggle->border_color.a > 0.0f && toggle->border_thickness > 0.0f)
				submit_border(size, toggle->border_color, transform, size.y, thickness);

			/* handle */
			if (auto texture = toggle->m_handle_texture)
			{
				s_instance->submit_quad(texture, toggle->m_handle_texture_uv, toggle->m_handle_texture_uv_stride,
					glm::vec2(handleSize.y), toggle->handle_on_color * color, handleTransform, 0.0f, 0.0f, 0.1f, false);
			}
			else
			{
				s_instance->submit_quad(glm::vec2(handleSize.y), handleTransform, toggle->handle_on_color * color, 0.0f, 0.0f, 0.1f);
			}

		}
		else
		{
			handleTransform[3].x -= size.x * 0.5f - size.y * 0.5f;

			/* toggle */
			s_instance->submit_quad(size, transform, toggle->off_color * color, size.y);

			/* border */
			if(toggle->border_color.a > 0.0f && toggle->border_thickness > 0.0f)
				submit_border(size, toggle->border_color, transform, size.y, thickness);

			/* handle */
			if (auto texture = toggle->m_handle_texture)
			{
				s_instance->submit_quad(texture, toggle->m_handle_texture_uv, toggle->m_handle_texture_uv_stride,
					glm::vec2(handleSize.y), toggle->handle_off_color * color, handleTransform, 0.0f, 0.0f, 0.1f, false);
			}
			else
			{
				s_instance->submit_quad(glm::vec2(handleSize.y), handleTransform, toggle->handle_off_color * color, 0.0f, 0.0f, 0.1f); // 0.005f
			}
		}

		if (!toggle->m_game_object.get_component<state_component>().is_active)
			return;

		if (toggle->m_game_object != s_instance->m_currently_selected_object)
		{
			if (toggle->m_is_pressed)
			{
				/* has just been released. fire toggle action */

				auto bOn = toggle->toggle();
				if (auto action = toggle->on_toggle_action)
					action(toggle, pScene, bOn, toggle->user_data);

				toggle->m_is_pressed = false;
			}

			return;
		}

		if (!toggle->m_is_pressed)
		{
			toggle->m_is_pressed = true;
		}
	}

	void ui_renderer::submit_bar(bar_component* bar, const glm::vec2& size, const glm::mat4& transform)
	{
		const auto& range = bar->range;
		auto& value = bar->value;

		float normalizedValue = value / range.y;
		glm::vec2 fillSize = size;
		glm::mat4 fillTransform = transform;

		float corner = 1.0f, fillCorner = 1.0f;

		if (bar->m_orientation == bar_component::horizontal)
		{
			fillSize.x *= normalizedValue;
			fillTransform[3].x -= (size.x - fillSize.x) * 0.5f;

			corner = size.y;
			fillCorner = fillSize.y;
		}
		else
		{
			fillSize.y *= normalizedValue;
			#if INVERT_VIEWPORT
			fillTransform[3].y -= (size.y - fillSize.y) * 0.5f;
			#else
			fillTransform[3].y += (size.y - fillSize.y) * 0.5f;
			#endif

			corner = size.x;
			fillCorner = fillSize.x;
		}

		/* background quad */
		if (bar->background_color.a > 0.0f)
			s_instance->submit_quad(size, transform, bar->background_color, corner);

		/* inner/fill quad */
		if (value > 0.0f)
			s_instance->submit_quad(fillSize, fillTransform, bar->fill_color, fillCorner);

		/* outer/border quad */
		if (bar->border_color.a > 0.0f)
		{
			/* the SDF algorithm that generates the frame needs a bit more space to generate the shape properly
			 * a factor of 1.1 is taken from the size when calculating it. multiply the size here to offset this difference
			 */
			float thickness = (size.y / size.x * 100.0f) * bar->border_thickness;
			s_instance->submit_quad(size * 1.1f, transform, bar->border_color, corner, thickness);
		}
	}

	void ui_renderer::submit_dialog_box(dialog_box_component* dialog, glm::mat4& transform, float baseQuadSize)
	{
		float fontSize = dialog->font_size;
		float lineWidth = dialog->line_width;

		fontSize = std::clamp(fontSize * baseQuadSize * 0.01f, 0.04f, 1.0f);
		lineWidth *= baseQuadSize;

		glm::vec2 size = dialog->m_custom_rect ? 
			dialog->get_rect_size() * baseQuadSize :
				glm::vec2(lineWidth * 1.2f, fontSize * 140.0f * dialog->max_lines);

		float thickness, corner;

		if(size.x >= size.y)
		{
			thickness = size.y * dialog->border_thickness * 0.02f;
			corner = size.x * dialog->round_corners_radius;
		}
		else
		{
			thickness = size.x * dialog->border_thickness * 0.02f;
			corner = size.y * dialog->round_corners_radius;
		}

		/* submit box */
		{
			if (dialog->blur_box)
			{
				submit_blurred_background_image(size, transform, dialog->box_color, dialog->round_corners_radius);
			}
			else
			{
				submit_panel(size, dialog->box_color, transform, corner);
			}

			if(dialog->border_thickness > 0.0f && dialog->box_border_color.a > 0.0f)
			{
				submit_border(size, dialog->box_border_color, transform, corner, thickness);
			}
		}

		/* submit text */
		{
			auto& dLine = dialog->dialogs_list[dialog->m_current_dialog_index];

			if(lineWidth)
				transform[3].x -= lineWidth * 0.5f;
			else
			 transform[3].x -= size.x * 0.40f;

			if(dialog->m_custom_rect)
				transform[3].y -= size.y * 0.40f;
			else
				transform[3].y -= size.y * 0.2f;

			s_instance->render_text(dLine.substr(0, dialog->m_current_char_count), fontSize, dialog->font, dialog->text_color, transform, false, lineWidth);
		}
	}


}