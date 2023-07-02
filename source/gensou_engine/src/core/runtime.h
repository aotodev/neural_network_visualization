#pragma once

#include "core/core.h"

namespace gs {

	/* class to hold all important runtime states */
	class runtime
	{
	public:
		static float delta_time() { return s_deltatime; }

		static extent2d viewport() { return s_viewport; }
		static extent2d old_viewport() { return s_old_viewport; }

		static uint32_t current_frame() { return s_current_frame; }
		static uint32_t get_frames_in_flight_count() { return s_frames_in_flight_count; }

		static uint32_t multisample_count() { return s_multisample_count; }

		static bool is_mute() { return s_is_mute; }
		static void set_mute(bool mute) { s_is_mute = mute; }

		static bool focused() { return s_is_focused; }
		static void set_focused(bool focused) { s_is_focused = focused; }

	private:
		static float set_delta_time();
		static void restart_counter();
		static void on_framebuffer_resize(uint32_t width, uint32_t height);
		static void set_frames_in_flight_count(uint32_t count) { s_frames_in_flight_count = count; }

		/* advances one frame, to be called by the swapchain after presenting */
		static uint32_t next_frame()
		{
			s_current_frame = (s_current_frame + 1) % s_frames_in_flight_count;
			return s_current_frame;
		}

		static extent2d set_viewport(extent2d extent)
		{ 
			s_old_viewport = s_viewport;
			s_viewport = extent;

			return s_viewport;
		}

		static extent2d set_viewport(uint32_t width, uint32_t height)
		{ 
			s_old_viewport = s_viewport;
			s_viewport = { width, height };

			return s_viewport;
		}

	private:
		static float s_deltatime;

		static extent2d s_viewport, s_old_viewport;

		static uint32_t s_frames_in_flight_count, s_current_frame;
		static uint32_t s_desired_multisample_count, s_multisample_count;

		static bool s_use_staging_buffer, s_is_mute, s_is_focused;

		//--friends---------------------------->>
		friend class gensou_app;
		friend class device;
		friend class renderer;
		friend class swapchain;

		friend class android_window;
		friend class windows_window;
		friend class linux_window;
	};

	using rt = runtime;

	/* usefull to convert input space to viewport space */
	#if INVERT_VIEWPORT
	#define CONVERT_TO_VIEWPORT(x, y) x = x - (float)rt::viewport().width * 0.5f; y = (float)rt::viewport().height * 0.5f - y;
	#else
	#define CONVERT_TO_VIEWPORT(x, y) x = x - (float)rt::viewport().width * 0.5f; y = y - (float)rt::viewport().height * 0.5f;
	#endif
}