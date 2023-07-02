#include "core/input.h"

#include "core/core.h"
#include "core/runtime.h"
#include "core/log.h"
#include "core/engine_events.h"

namespace gs {

	input_type  input::s_active_input_type			= input_type::none;

	input_state input::s_right_ctrl_key				= input_state::released;
	input_state input::s_left_ctrl_key				= input_state::released;

	input_state input::s_right_shift_key			= input_state::released;
	input_state input::s_left_shift_key				= input_state::released;

	input_state input::s_left_alt_key				= input_state::released;
	input_state input::s_right_altgr_key			= input_state::released;

	input_state input::s_super_key					= input_state::released;

	input_state input::s_mouse_middle_button		= input_state::released;
	input_state input::s_mouse_right_button			= input_state::released;
	input_state input::s_mouse_left_button			= input_state::released;

	glm::vec2	input::s_mouse_position				= glm::vec2(0.0f);
	glm::vec2	input::s_mouse_position_last_click	= glm::vec2(0.0f);
	glm::vec2	input::s_touch_position				= glm::vec2(0.0f);
	glm::vec2	input::s_position_on_last_touchDown	= glm::vec2(0.0f);

	float		input::s_touch_overlap_radius		= 28.0f;

	uint32_t	input::s_held_key_count				= 0UL;
	bool		input::s_has_mouse_device_connected	= false;

	void input::init()
	{
		engine_events::key.subscribe(&input::key_callback);
		engine_events::mouse_button_action.subscribe(&input::mouse_button_callback);

		// TODO: actually check if a mouse is plugged in
		#ifndef APP_ANDROID
		s_has_mouse_device_connected = true;
		#endif
	}

	void input::key_callback(key_code key, input_state state)
	{
		switch (key)
		{
			case key_code::right_ctrl:	s_right_ctrl_key	= state; break;
			case key_code::left_ctrl:	s_left_ctrl_key		= state; break;
			case key_code::right_shift:	s_right_shift_key	= state; break;
			case key_code::left_shift:	s_left_shift_key	= state; break;
			case key_code::left_alt:	s_left_alt_key		= state; break;
			case key_code::right_alt:	s_right_altgr_key	= state; break;
			case key_code::left_super:	s_super_key			= state; break;
			
			default:
				break;
		}
	}

	void input::mouse_button_callback(mouse_button button, input_state state)
	{
		switch (button)
		{
			case mouse_button::left:	s_mouse_left_button		= state; break;
			case mouse_button::right:	s_mouse_right_button	= state; break;
			case mouse_button::middle:	s_mouse_middle_button	= state; break;

			default:
				break;
		}
	}

}