#include "core/engine_events.h"
#include "core/input.h"

namespace gs {

    event<std::function<void(uint32_t, uint32_t)>> engine_events::window_resize;
    event<std::function<void(uint32_t, uint32_t)>> engine_events::viewport_resize;
    event<std::function<void(void)>> engine_events::window_close;
    event<std::function<void(void)>> engine_events::terminate_renderer;
    event<std::function<void(void)>> engine_events::save_state;
    event<std::function<void(bool)>> engine_events::change_focus;
    event<std::function<void(bool)>> engine_events::window_minimize;

    event<std::function<void(VkResult, const std::string&)>> engine_events::vulkan_result_error;

    event<std::function<void(key_code, input_state)>> engine_events::key;
    event<std::function<void(key_code, uint16_t repeatCount)>> engine_events::key_pressed;
    event<std::function<void(key_code)>> engine_events::key_released;

    event<std::function<void(mouse_button, input_state)>> engine_events::mouse_button_action;
    event<std::function<void(mouse_button)>> engine_events::mouse_button_pressed;
    event<std::function<void(mouse_button)>> engine_events::mouse_button_released;
    event<std::function<void(const float x, const float y)>> engine_events::mouse_moved;
    event<std::function<void(const float delta)>> engine_events::mouse_scrolled;

    event<std::function<void(const float x, const float y)>> engine_events::touch_down;
    event<std::function<void(const float x, const float y)>> engine_events::touch_up;
    event<std::function<void(const float x, const float y)>> engine_events::touch_move;
    event<std::function<void(const float delta)>> engine_events::pinch_scale;

}