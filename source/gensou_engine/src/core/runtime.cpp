#include "core/runtime.h"

namespace gs {

	float		runtime::s_deltatime = 0.0f;

	extent2d	runtime::s_viewport;
	extent2d	runtime::s_old_viewport;

	uint32_t	runtime::s_frames_in_flight_count = 3;
	uint32_t	runtime::s_current_frame = 0;
	uint32_t	runtime::s_multisample_count = 1;
	uint32_t	runtime::s_desired_multisample_count = 8;

	bool		runtime::s_use_staging_buffer = true;
	bool		runtime::s_is_mute = false;
	bool		runtime::s_is_focused = true;

	static std::chrono::time_point<std::chrono::high_resolution_clock> s_dt_current_frame, s_dt_last_frame;

	void runtime::restart_counter()
	{
		s_dt_current_frame = std::chrono::high_resolution_clock::now();
		s_dt_last_frame = std::chrono::high_resolution_clock::now();
	}

	float runtime::set_delta_time()
	{
		s_dt_current_frame = std::chrono::high_resolution_clock::now();
		s_deltatime = std::chrono::duration<float, std::chrono::seconds::period>(s_dt_current_frame - s_dt_last_frame).count();
		s_dt_last_frame = std::chrono::high_resolution_clock::now();

		return s_deltatime;
	}

	void runtime::on_framebuffer_resize(uint32_t width, uint32_t height)
	{
		set_viewport(width, height);
		restart_counter();
	}

}
