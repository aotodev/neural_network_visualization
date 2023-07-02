#include "scene/game_instance.h"

#include "core/system.h"
#include "core/runtime.h"
#include "core/engine_events.h"

#include "scene/scene.h"
#include "renderer/renderer.h"
#include "renderer/command_manager.h"

namespace gs {

    game_instance* game_instance::s_instance = nullptr;

	game_instance::game_instance()
	{
		engine_events::viewport_resize.subscribe(BIND_MEMBER_FUNCTION(game_instance::on_resize));
		engine_events::key.subscribe(BIND_MEMBER_FUNCTION(game_instance::on_key_action));
		engine_events::mouse_button_action.subscribe(BIND_MEMBER_FUNCTION(game_instance::on_mouse_button_action));
		engine_events::touch_down.subscribe(BIND_MEMBER_FUNCTION(game_instance::on_touch_down));
		engine_events::touch_up.subscribe(BIND_MEMBER_FUNCTION(game_instance::on_touch_up));
		engine_events::touch_move.subscribe(BIND_MEMBER_FUNCTION(game_instance::on_touch_move));
		engine_events::pinch_scale.subscribe(BIND_MEMBER_FUNCTION(game_instance::on_pinch_scale));
		engine_events::mouse_scrolled.subscribe(BIND_MEMBER_FUNCTION(game_instance::on_mouse_scrolled));
		engine_events::mouse_moved.subscribe(BIND_MEMBER_FUNCTION(game_instance::on_mouse_moved));
		engine_events::save_state.subscribe(BIND_MEMBER_FUNCTION(game_instance::on_save_state));

		system::run_on_loading_thread([this]()
		{ 
			on_create();
			command_manager::reset_loading_pools();
		});
	}

    void game_instance::init()
    {
        on_init();
    }

	void game_instance::start()
	{
		on_start();

		if(!m_current_scene)
		{
			set_current_scene(create_first_scene(), true, false);
		}
	}

    void game_instance::update(float dt)
	{
		m_current_scene->update(dt);
		on_update(dt);
	}

    void game_instance::terminate()
	{
		on_save_state();

		if (m_current_scene)
		{
			m_current_scene->terminate();
			m_current_scene.reset();
		}

		on_terminate();
	}

	void game_instance::set_current_scene(std::shared_ptr<scene> inScene, bool startScene, bool keepOldSceneAlive /*= false*/)
	{
		assert(inScene);

		command_manager::reset_all_pools();
		renderer::reset_render_cmds();

		if(m_current_scene && !keepOldSceneAlive)
		{
			m_current_scene->on_terminate();
		}

		m_current_scene = inScene;
		m_current_scene->create_loading_scene();

		system::run_on_loading_thread([this]()
		{ 
			m_current_scene->init();
			command_manager::reset_loading_pools();
		});
	}

    void game_instance::on_save_state()
    {
        if (m_current_scene)
            m_current_scene->on_save_state();

		auto settings = system::get_settings();
		settings->width = runtime::viewport().width;
		settings->height = runtime::viewport().height;
		settings->vsync = system::vsync();;
        
		settings->use_postprocess = renderer::is_post_process_enabled();

		system::serialize_settings(settings);
    }

	void game_instance::on_resize(uint32_t width, uint32_t height)
	{
		if (m_current_scene)
		{
			m_current_scene->on_window_resize(width, height);

			if (m_current_scene->finished_loading())
			{
				m_current_scene->on_viewport_resize(width, height);
			}
			else if (m_current_scene->m_loading_scene)
			{
				m_current_scene->m_loading_scene->on_viewport_resize(width, height);
			}
		}
	}

	void game_instance::on_key_action(key_code key, input_state state)
	{
		if (m_current_scene && m_current_scene->finished_loading())
			m_current_scene->on_key_action(key, state);
	}

	void game_instance::on_touch_down(float x, float y)
	{
		if (m_current_scene && m_current_scene->finished_loading())
			m_current_scene->on_touch_down(x, y);
	}

	void game_instance::on_touch_up(float x, float y)
	{
		if (m_current_scene && m_current_scene->finished_loading())
		{
			m_current_scene->on_touch_up(x, y);
		}
	}

	void game_instance::on_touch_move(float x, float y)
	{
		if (m_current_scene && m_current_scene->finished_loading())
		{
			m_current_scene->on_touch_move(x, y);
		}
	}

	void game_instance::on_mouse_button_action(mouse_button key, input_state state)
	{
		if (m_current_scene && m_current_scene->finished_loading())
		{ 
			m_current_scene->on_mouse_button_action(key, state);
		}
	}

	void game_instance::on_mouse_moved(const float x, const float y)
	{
		if (m_current_scene && m_current_scene->finished_loading())
		{
			m_current_scene->on_mouse_moved(x, y);
		}
	}

	void game_instance::on_mouse_scrolled(const float delta)
	{
		if (m_current_scene && m_current_scene->finished_loading())
		{
			m_current_scene->on_mouse_scrolled(delta);
		}
	}

	void game_instance::on_pinch_scale(const float scale)
	{
		if (m_current_scene && m_current_scene->finished_loading())
		{
			m_current_scene->on_pinch_scale(scale);
		}
	}

}