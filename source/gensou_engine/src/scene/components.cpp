#include "scene/components.h"
#include "core/input.h"
#include "core/input_codes.h"
#include "glm/fwd.hpp"
#include "glm/trigonometric.hpp"
#include "scene/scene.h"
#include "scene/game_statics.h"

#include "core/runtime.h"
#include "core/engine_events.h"

#include "renderer/buffer.h"

namespace gs {

	sprite_component::sprite_component(const std::string& path, bool mips, bool flip, float u, float v, float strideX, float strideY)
		: m_texture(texture::create(path, mips, flip)), m_coords({ u, v }), m_stride({ strideX, strideY })
	{
		calculate_size(false);
	}

	sprite_component::sprite_component(const std::string& path, float u, float v, float strideX, float strideY)
		: m_texture(texture::create(path, false, false)), m_coords({ u, v }), m_stride({ strideX, strideY }) 
	{
		calculate_size(false);
	}

	sprite_component::sprite_component(std::shared_ptr<texture> inTexture, float u, float v, float strideX, float strideY)
		: m_texture(inTexture), m_coords({ u, v }), m_stride({ strideX, strideY })
	{
		calculate_size(false);
	}

	sprite_component::sprite_component(const std::string& path, const glm::vec4& coords, bool mips, bool flip)
		: m_texture(texture::create(path, mips, flip)), m_coords({ coords.x, coords.y }), m_stride({ coords.z, coords.w })
	{
		calculate_size(false);
	}


	sprite_component::sprite_component(const std::string& path, bool mips, bool flip, glm::vec2 uv, glm::vec2 stride, sampler_info samplerInfo)
		: m_texture(texture::create(path, mips, flip, samplerInfo)), m_coords({ uv.x, uv.y }), m_stride({ stride.x, stride.y })
	{
		calculate_size(false);
	}

	void sprite_component::calculate_size(bool keepScale)
	{
		if (!m_texture)
			return;

		float width = (float)m_texture->get_width() * m_stride.x;
		float height = (float)m_texture->get_height() * m_stride.y;

		if (keepScale)
		{
			/* update aspect ratio maintaining the current scale */
			float newRatio = width / height;
			m_size.y = m_size.x / newRatio;
		}
		else
		{
			float unit = game_statics::get_active_scene()->get_base_unit_in_pixels();
			m_size = { width / unit, height / unit };
		}

	}

	void sprite_component::set_texture(const std::string& path, bool mips, bool flip,  float u, float v, float strideX, float strideY)
	{
		m_texture = texture::create(path, mips, flip);

		m_coords = { u, v };
		m_stride = { strideX, strideY };

		calculate_size(false);
	}

	void sprite_component::set_texture(std::shared_ptr<texture> inTexture, float u, float v, float strideX, float strideY)
	{
		m_texture = inTexture;

		m_coords = { u, v };
		m_stride = { strideX, strideY };

		calculate_size(false);
	}

	void sprite_component::set_texture_ex(const std::string& path, bool mips, bool flip, float u, float v, float strideX, float strideY, sampler_info samplerInfo)
	{
		m_texture = texture::create(path, mips, flip, samplerInfo);

		m_coords = { u, v };
		m_stride = { strideX, strideY };

		calculate_size(false);
	}

	void sprite_component::animate(float dt)
	{
		if (m_current_animation && m_current_animation->is_active)
		{
			assert(!m_current_animation->frames_uv_and_stride.empty());

			/* still animation */
			if (m_current_animation->frame_count == 1)
			{
				if (m_current_animation->has_just_started)
				{
					const auto& uv_and_stride = m_current_animation->frames_uv_and_stride[0];
					m_coords = glm::vec2(uv_and_stride.x, uv_and_stride.y);
					m_stride = glm::vec2(uv_and_stride.z, uv_and_stride.w);

					m_current_animation->has_just_started = false;
					return;
				}
				return;
			}

			/* regular case */

			/* start animation */
			if (m_current_animation->has_just_started)
			{
				m_current_animation->accumulator = 0.0;

				m_current_animation->current_frame = 0;
				m_current_animation->has_just_started = false;

				const auto& uv_and_stride = m_current_animation->frames_uv_and_stride[0];
				m_coords = glm::vec2(uv_and_stride.x, uv_and_stride.y);
				m_stride = glm::vec2(uv_and_stride.z, uv_and_stride.w);

				m_current_animation->next_frame();
				m_current_animation->accumulator += dt;
				return;
			}

			/* advance 1 frame and reset the accumulator */
			if (m_current_animation->accumulator >= m_current_animation->change_frame - (dt * 0.1))
			{
				const auto& uv_and_stride = m_current_animation->frames_uv_and_stride[m_current_animation->current_frame];
				m_coords = glm::vec2(uv_and_stride.x, uv_and_stride.y);
				m_stride = glm::vec2(uv_and_stride.z, uv_and_stride.w);

				m_current_animation->accumulator = 0.0;
				m_current_animation->next_frame();

				if (!m_current_animation->loop && m_current_animation->current_frame == 0)
				{
					m_current_animation->is_active = false;
					return;
				}
			}

			m_current_animation->accumulator += dt;
		}
	}

	void camera_component::update(transform_component& transform)
	{
		auto orientation = glm::quat(glm::vec3(transform.rotation.x, -transform.rotation.y, transform.rotation.z));
		glm::vec3 position = m_look_at - glm::rotate(orientation, forward_vector()) * transform.translation.z;
		m_view = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(orientation);
		m_view = glm::inverse(m_view);
	}

	void camera_component::set_orthographic(float size, float nearClip, float farClip)
	{
		m_projection_type = projection::orthographic;

		m_orthographic_size = size;
		m_orthographic_near = nearClip;
		m_orthographic_far = farClip;
	}

	void camera_component::set_perspective(float fov, float nearClip, float farClip)
	{
		m_projection_type = projection::perspective;

		m_perspective_fov = fov;
		m_perspective_near = nearClip;
		m_perspective_far = farClip;
	}

	void camera_component::set_viewport_size(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		m_rect2d.s = width;
		m_rect2d.t = height;

		switch (m_projection_type)
		{
			case projection::orthographic:
			{
				m_orthographic_size = (float)height;
				const float aspect = (float)width / (float)height;
				const float lwidth = m_orthographic_size * aspect * m_orthographic_zoom;
				const float lheight = m_orthographic_size * m_orthographic_zoom;

				m_projection = glm::ortho(-lwidth * 0.5f, lwidth * 0.5f, -lheight * 0.5f, lheight * 0.5f, m_orthographic_near, m_orthographic_far);
				break;
			}
			case projection::perspective:
			{
				m_projection = glm::perspective(m_perspective_fov, (float)width / (float)height, m_perspective_near, m_perspective_far);
				break;
			}
			default:
			{
				LOG_ENGINE(error, "invalid projection type");
				break;
			}
		}
	}

	void camera_component::set_projection_type(projection projectionType)
	{
		m_projection_type = projectionType;

		switch (m_projection_type)
		{
		case projection::orthographic:
		{
			set_orthographic(m_orthographic_size, m_orthographic_near, m_orthographic_far);
			set_viewport_size(m_rect2d.x, m_rect2d.y);
			break;
		}
		case projection::perspective:
		{
			set_perspective(m_perspective_fov, m_perspective_near, m_perspective_far);
			set_viewport_size(m_rect2d.x, m_rect2d.y);
			break;
		}
		default:
		{
			LOG_ENGINE(error, "Invalid Projection Type");
			break;
		}
		}
	}

	void camera_component::zoom(float dy)
	{
		switch (m_projection_type)
		{
			case projection::orthographic:
			{
				m_orthographic_zoom -= m_zoom_speed * dy;

				if (m_orthographic_zoom < 0.01f)
					m_orthographic_zoom = 0.0f;

				const float width = m_rect2d.s * m_orthographic_zoom;
				const float height = m_rect2d.t * m_orthographic_zoom;

				m_projection = glm::ortho(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f);
				break;
			}
			case projection::perspective:
			{
				m_perspective_fov -= m_zoom_speed * dy;

				if (m_perspective_fov < 0.01f)
					m_perspective_fov = 0.01;

				m_projection = glm::perspective(m_perspective_fov, m_rect2d.x / m_rect2d.y, m_perspective_near, m_perspective_far);
				break;
			}
			default:
			{
				LOG_ENGINE(error, "Invalid Projection Type");
				break;
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////////
	// UI COMPONENTS
	//////////////////////////////////////////////////////////////////////////////////
	static inline bool operator&(anchor lhs, anchor rhs) { return (uint32_t)lhs & (uint32_t)rhs; }

	glm::vec2 anchor_component::get_center()
	{
		glm::vec3 parentScale = glm::vec3(1.0f, 1.0f, 1.0f);

		if (auto parent = m_game_object.get_component<relationship_component>().parent)
		{
			parentScale = parent.get_world_scale();
		}

		return get_center(parentScale);
	}

	glm::vec2 anchor_component::get_center(const glm::vec3& parentScale)
	{
		glm::vec2 center(0.0f, 0.0f);

		if (m_anchor == anchor::center)
			return center;

		bool horizontalStretch = m_anchor & anchor::horizontal_stretch;
		bool verticalStretch = m_anchor & anchor::vertical_stretch;

		auto extent = m_game_object.get_scene()->get_scene_viewport();

		bool hasUiParent = false;

		auto& rect = m_game_object.get_component<rect2d_component>();
		auto& transform = m_game_object.get_component<transform_component>();

		if (auto parent = m_game_object.get_component<relationship_component>().parent)
		{
			if (parent.has_components<ui_component>())
			{
				hasUiParent = true;
				auto& parentRect = parent.get_component<rect2d_component>();

				if (horizontalStretch)
				{
					rect.width = parentRect.width;
					transform.scale.x = 1.0f;
				}
				else
					extent.x = parentRect.get_rect().x * parentScale.x;

				if (verticalStretch)
				{
					rect.height = parentRect.height;
					transform.scale.y = 1.0f;
				}
				else
					extent.y = parentRect.get_rect().y * parentScale.y;
			}
		}

		if(!hasUiParent)
		{
			if (horizontalStretch)
			{
				rect.width = extent.x;
				transform.scale.x = 1.0f;
			}

			if (verticalStretch)
			{
				rect.height = extent.y;
				transform.scale.y = 1.0f;
			}
		}
		
		if(!horizontalStretch)
			center.x = m_anchor & anchor::horizontal_center ? 0.0f : m_anchor & anchor::left ? extent.x * -0.5f : extent.x * 0.5f;

		if(!verticalStretch)
			center.y = m_anchor & anchor::vertical_center ? 0.0f : m_anchor & anchor::top ? extent.y * -0.5f : extent.y * 0.5f;

		return center;
	}

	void button_component::set_rect_to_texture()
	{
			if (!m_texture)
				return;

			float width = m_texture->get_width() * m_texture_uv_stride.x;
			float height = m_texture->get_height() * m_texture_uv_stride.y;

			float unit = m_game_object.get_scene()->get_base_unit_in_pixels();

			set_rect(width / unit, height / unit);

			m_texture_mode = ui_texture_mode::fit_both;
	}

	void button_component::set_stride(float x, float y)
	{
		m_texture_uv_stride.x = x; m_texture_uv_stride.y = y;

		/* update aspect ratio maintaining the current scale */
		float newWidth = m_texture->get_width() * m_texture_uv_stride.x;
		float newHeight = m_texture->get_height() * m_texture_uv_stride.y;
		float newRatio = newWidth / newHeight;

		auto& rect = m_game_object.get_component<rect2d_component>();
		rect.height = rect.width / newRatio;
	}

	void button_component::set_texture(std::shared_ptr<texture> texture, float u, float v, float strideX, float strideY, bool overrideCurrentSize)
	{
		m_texture = texture;
		m_texture_uv = glm::vec2(u, v);
		m_texture_uv_stride= glm::vec2(strideX, strideY);
	}

	void button_component::set_texture(const std::string& path, float u, float v, float strideX, float strideY, bool overrideCurrentSize)
	{
		m_texture = texture::create(path, true, false);
		m_texture_uv = glm::vec2(u, v);
		m_texture_uv_stride = glm::vec2(strideX, strideY);
	}

	void slider_component::set_handle_texture(std::shared_ptr<texture> texture, float u, float v, float strideX, float strideY)
	{
		m_handle_texture = texture;
		m_handle_texture_uv = glm::vec2(u, v);
		m_handle_texture_uv_stride = glm::vec2(strideX, strideY);
	}

	void slider_component::set_handle_texture(const std::string& path, float u, float v, float strideX, float strideY)
	{
		m_handle_texture = texture::create(path);
		m_handle_texture_uv = glm::vec2(u, v);
		m_handle_texture_uv_stride = glm::vec2(strideX, strideY);	
	}

	void toggle_switch_component::set_handle_texture(std::shared_ptr<texture> texture, float u, float v, float strideX, float strideY)
	{
		m_handle_texture = texture;
		m_handle_texture_uv = glm::vec2(u, v);
		m_handle_texture_uv_stride = glm::vec2(strideX, strideY);
	}

	void toggle_switch_component::set_handle_texture(const std::string& path, float u, float v, float strideX, float strideY)
	{
		m_handle_texture = texture::create(path);
		m_handle_texture_uv = glm::vec2(u, v);
		m_handle_texture_uv_stride = glm::vec2(strideX, strideY);	
	}

	void image_component::set_rect_to_texture()
	{
		if (!m_texture)
			return;

		float width = m_texture->get_width() * m_texture_uv_stride.x;
		float height = m_texture->get_height() * m_texture_uv_stride.y;

		float unit = m_game_object.get_scene()->get_base_unit_in_pixels();

		set_rect(width / unit, height / unit);
	}

	void image_component::set_texture(std::shared_ptr<texture> texture, float u, float v, float strideX, float strideY, bool overrideCurrentSize)
	{
		m_texture = texture;
		m_texture_uv = glm::vec2(u, v);
		m_texture_uv_stride = glm::vec2(strideX, strideY);

		set_rect_to_texture();
	}

	void image_component::set_texture(const std::string& path, float u, float v, float strideX, float strideY, bool overrideCurrentSize)
	{
		m_texture = texture::create(path, true, false);
		m_texture_uv = glm::vec2(u, v);
		m_texture_uv_stride = glm::vec2(strideX, strideY);

		set_rect_to_texture();
	}

	uint32_t dialog_box_component::next_dialog()
	{
		if(!m_finished)
		{
			m_updating = true;
			m_finished_line = false;
	
			m_current_char_count = 1;
			m_current_dialog_index = (m_current_dialog_index + 1) % dialogs_list.size();
		}

		return m_current_dialog_index;
	}

	void dialog_box_component::update(float dt)
	{
		m_counter += dt * text_speed;

		if(m_counter >= 1.0f)
		{
			m_counter = 0.0f;

			if(m_current_char_count >= dialogs_list[m_current_dialog_index].size())
			{
				m_updating = false;
				m_finished_line = true;

				if(finished_line_action)
					finished_line_action(this, m_current_dialog_index, m_game_object.get_scene(), user_data);

				if(m_current_dialog_index >= (dialogs_list.size() - 1))
					m_finished = true;

				return;
			}

			m_current_char_count++;
		}
	}

}