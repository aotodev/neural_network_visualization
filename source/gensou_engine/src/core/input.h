#pragma once

#include "core/input_codes.h"

#include <glm/glm.hpp>

namespace gs {

	class input
	{
		friend class android_window;
		friend class windows_window;
		friend class linux_window;

	public:
		static void init();

		static input_type active_input_type() { return s_active_input_type; }

		static input_state right_ctrl() { return s_right_ctrl_key; }
		static input_state left_ctrl() { return s_left_ctrl_key; }

		static input_state right_shift() { return s_right_shift_key; }
		static input_state left_shift() { return s_left_shift_key; }

		static input_state right_altgr() { return s_right_altgr_key; }
		static input_state left_alt() { return s_left_alt_key; }

		static input_state super() { return s_super_key; }

		static input_state mouse_middle_button() { return s_mouse_middle_button; }
		static input_state mouse_right_button() { return s_mouse_right_button; }
		static input_state mouse_left_button() { return s_mouse_left_button; }

		static float touch_overlap_radius() { return s_touch_overlap_radius; }
		static uint32_t key_held_count() { return s_held_key_count; }

		static glm::vec2 mouse_position() { return s_mouse_position; }
		static glm::vec2 last_clicked_mouse_position() { return s_mouse_position_last_click; }
		static glm::vec2 touch_position() { return s_touch_position; }
		static glm::vec2 last_touch_down_position() { return s_position_on_last_touchDown; }

		static bool has_mouse_device_connected() { return s_has_mouse_device_connected; }

		static bool is_pressed(key_code key);

	private:
		static void mouse_button_callback(mouse_button button, input_state state);
		static void key_callback(key_code key, input_state state);

	private:
		static input_type s_active_input_type;

		static input_state s_right_ctrl_key, s_left_ctrl_key, s_right_shift_key, s_left_shift_key, s_left_alt_key, s_right_altgr_key, s_super_key;
		static input_state s_mouse_middle_button, s_mouse_right_button, s_mouse_left_button;

		static glm::vec2 s_mouse_position;
		static glm::vec2 s_mouse_position_last_click;

		static glm::vec2 s_touch_position;
		static glm::vec2 s_position_on_last_touchDown;

		static float s_touch_overlap_radius;

		static uint32_t s_held_key_count;

		static bool s_has_mouse_device_connected;

	};

}