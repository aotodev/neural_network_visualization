#pragma once

#include "core/event.h"
#include "core/input_codes.h"

#include <vulkan/vulkan.h>

namespace gs {

	class engine_events
	{
	public:
		static event<std::function<void(uint32_t, uint32_t)>> window_resize;
		static event<std::function<void(uint32_t, uint32_t)>> viewport_resize;
		static event<std::function<void(void)>> window_close;
		static event<std::function<void(void)>> terminate_renderer;
		static event<std::function<void(void)>> save_state;
		static event<std::function<void(bool)>> change_focus;
		static event<std::function<void(bool)>> window_minimize;

		static event<std::function<void(VkResult, const std::string&)>> vulkan_result_error;

		static event<std::function<void(key_code, input_state)>> key;
		static event<std::function<void(key_code, uint16_t repeatCount)>> key_pressed;
		static event<std::function<void(key_code)>> key_released;

		static event<std::function<void(mouse_button, input_state)>> mouse_button_action;
		static event<std::function<void(mouse_button)>> mouse_button_pressed;
		static event<std::function<void(mouse_button)>> mouse_button_released;
		static event<std::function<void(const float x, const float y)>> mouse_moved;
		static event<std::function<void(const float delta)>> mouse_scrolled;

		static event<std::function<void(const float x, const float y)>> touch_down;
		static event<std::function<void(const float x, const float y)>> touch_up;
		static event<std::function<void(const float x, const float y)>> touch_move;
		static event<std::function<void(const float scale)>> pinch_scale;
	};

}