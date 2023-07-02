#include "core/gensou_app.h"

#include "core/system.h"
#include "core/log.h"
#include "core/runtime.h"
#include "core/time.h"

#include "core/window.h"
#include "renderer/renderer.h"
#include "renderer/swapchain.h"
#include "renderer/device.h"
#include "renderer/memory_manager.h"
#include "renderer/command_manager.h"
#include "renderer/validation_layers.h"

#include "core/engine_events.h"
#include "core/input.h"

#include "scene/game_instance.h"
#include "scene/scene.h"
#include <type_traits>


#if defined(APP_ANDROID)
#include "platform/android/android_platform.h"
#endif

	
namespace gs {

	gensou_app* gensou_app::s_instance = nullptr;

	gensou_app* gensou_app::get() { return s_instance; }

	gensou_app* gensou_app::create()
	{
		if (s_instance == nullptr)
			s_instance = new gensou_app();

		return s_instance;
	}

	void gensou_app::destroy()
	{
		if (s_instance)
		{
			delete s_instance;
			s_instance = nullptr;
		}
	}

	gensou_app::gensou_app()
	{
		BENCHMARK("gensou_app constructor");

		log::init(GAME_NAME);
		system::init();

		auto settings = system::get_settings();
		auto windowProperties = window_properties::get_default(settings->width, settings->height);

		m_window = std::unique_ptr<window>(window::create(windowProperties));

		/* desktop windowing system and android's surface system are quite different and need to be 
		 * initialized at different stages
		 */
		#ifndef APP_ANDROID
		
		m_window->init();
		auto viewport = m_window->get_extent();
		runtime::set_viewport(viewport.width, viewport.height);

        #else
		runtime::set_viewport(windowProperties.width, windowProperties.height);
        #endif

		device::init();
		device::set_multisample_count(1);
		memory_manager::init();
		command_manager::init();
		input::init();

		//renderer::enable_post_process(settings->use_postprocess);
		renderer::enable_post_process(true);
		renderer::set_blur_downscale_factor(4);
		renderer::init();

		engine_events::vulkan_result_error.subscribe(BIND_MEMBER_FUNCTION(gensou_app::handle_vulkan_error));

		m_game_instance = game_instance::create();
	}

	void gensou_app::init()
	{
		LOG_ENGINE(trace, "calling gensou_app::init");
		BENCHMARK("gensou_app::init");

		#ifdef APP_ANDROID
		m_window->init();
		#endif
		m_window->create_swapchain(system::get_settings()->vsync);
		m_game_instance->init();
	}

	void gensou_app::start()
	{
		LOG_ENGINE(trace, "calling gensou_app::start");
		BENCHMARK("gensou_app::start");

		m_game_instance->start();

		runtime::restart_counter();
	}

	void gensou_app::update()
	{
		runtime::restart_counter();

		while (!m_window->should_close_window())
		{
			float dt = runtime::set_delta_time();

			if (m_window->focused())
			{
				BENCHMARK("game loop");
				{
					BENCHMARK_VERBOSE("game_instance::update");
					m_game_instance->update(dt);
				}

				{
					BENCHMARK_VERBOSE("window::update");
					m_window->update();
				}

				{
					BENCHMARK_VERBOSE("renderer::render");
					renderer::render(m_window->get_swapchain());
				}
			}
			else
			{
				/* to avoid huge cpu usage spikes */
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}

			m_window->poll_events();
		}
	}

	void gensou_app::terminate()
	{
		renderer::wait_render_cmds();
		command_manager::reset_all_pools();

		if(m_game_instance)
		{
			m_game_instance->terminate();
			delete m_game_instance;
			m_game_instance = nullptr;
		}

		vkDeviceWaitIdle(device::get_logical());

		m_window.reset();

		renderer::terminate();
		command_manager::terminate();
		memory_manager::terminate();
		device::terminate();

		system::terminate();
	}

	void gensou_app::run()
	{
		update();
		terminate();
	}

	void gensou_app::show_msg(const std::string& msg)
	{
		system::error_msg(msg);
	}

	void gensou_app::handle_vulkan_error(VkResult result, const std::string& message)
	{
		LOG_ENGINE(critical, message.c_str());
		show_msg(message);
		exit(-1);
	}

}

