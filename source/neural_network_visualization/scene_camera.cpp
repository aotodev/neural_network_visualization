#include "scene_camera.h"

#include <gensou/components.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

void scene_camera::on_init()
{
	/*----------------------------------------------------------------------------------------*/
    {
		auto viewport_pixels = gs::runtime::viewport();

		auto& camera = add_component<gs::camera_component>();
		camera.set_perspective(0.785398f);
		camera.set_viewport_size(viewport_pixels.width, viewport_pixels.height);

		set_camera(get_game_object());
	}
	/*----------------------------------------------------------------------------------------*/

    auto& pos = get_component<gs::transform_component>().translation;
	pos.z = m_base_z;
}

void scene_camera::on_update(float dt)
{
    if(m_orbit)
    {
        orbit(dt);
        return;
    }

    if(m_middle_mouse_button || m_touching)
        rotate();  
}

bool scene_camera::on_mouse_scrolled(const float delta)
{
    auto& position = get_component<gs::transform_component>().translation;

    float z = position.z - (delta * m_zoom_speed);

    position.z = std::clamp(z, -400.0f, 500.0f);

    return true;
}

bool scene_camera::on_mouse_button_action(mouse_button key, input_state state)
{
    switch(state)
    {
        case input_state::repeating: [[fallthrough]];
        case input_state::pressed:
        {
            if(key == mouse_button::middle)
            {
                if(!m_middle_mouse_button)
                    m_last_input = gs::input::mouse_position();

                m_middle_mouse_button = true;
            }

            return true;
        }

        case input_state::released:
        {
            if(key == mouse_button::middle)
                m_middle_mouse_button = false;
                
            return false;
        }
        default:
            break;
    }

    return false;
}

bool scene_camera::on_touch_down(float x, float y)
{
    m_last_input = gs::input::touch_position();
    m_touching = true;
    return true;
}

bool scene_camera::on_touch_up(float x, float y)
{
    m_touching = false;
    return false;
}

bool scene_camera::on_pinch_scale(const float movingSpan)
{
    auto& position = get_component<gs::transform_component>().translation;
    float z = position.z - (movingSpan * 0.2f);
    position.z = std::clamp(z, -500.0f, 500.0f);

    return true;
}

void scene_camera::reset_camera_transform()
{
    auto& transform = get_component<gs::transform_component>();
    transform.translation = { 0.0f, 0.0f, m_base_z };
    transform.rotation = glm::vec3(0.0f);
}

void scene_camera::rotate()
{
    float x, y;

    if(gs::input::has_mouse_device_connected())
    {
        x = gs::input::mouse_position().x;
        y = gs::input::mouse_position().y;
    }
    else
    {
        x = gs::input::touch_position().x;
        y = gs::input::touch_position().y;
    }

    float xoffset = x - m_last_input.x;
    float yoffset = y - m_last_input.y;
    m_last_input.x = x;
    m_last_input.y = y;

	constexpr float sensitivity = 0.005f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

    auto& rotation = get_component<gs::transform_component>().rotation;

    /* check yaw sign */
    {
        auto orientation = glm::quat(glm::vec3(rotation.x, -rotation.y, 0.0f));
        auto up = glm::rotate(orientation, gs::camera_component::up_vector());

        if(up.y < 0.0f)
            xoffset *= -1.0f;
    }

    rotation.y += xoffset; /* yaw */
    rotation.x += yoffset; /* pitch */
}

void scene_camera::orbit(float dt)
{
    auto& rotation = get_component<gs::transform_component>().rotation;

    float sign = -1.0f;
    {
        auto orientation = glm::quat(glm::vec3(rotation.x, -rotation.y, 0.0f));
        auto up = glm::rotate(orientation, gs::camera_component::up_vector());

        if(up.y < 0.0f)
            sign *= -1.0f;
    }

    rotation.y += dt * m_orbit_speed * sign;

    if(rotation.y > glm::radians(360.0f))
        rotation.y = 0.0f;

    if(rotation.y < glm::radians(-360.0f))
        rotation.y = 0.0f;
}