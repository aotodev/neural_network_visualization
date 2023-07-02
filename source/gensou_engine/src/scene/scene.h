#pragma once

#include "core/core.h"
#include "core/log.h"
#include "core/system.h"
#include "core/runtime.h"

#include "scene/audio_mixer.h"
#include "scene/game_instance.h"
#include "scene/sprite.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

class b2World;
class b2ContactListener;

namespace gs {

	struct sprite;
	class texture;
	class game_object;
	class scene_actor;

	class scene
	{
		friend class game_object;
		friend class game_statics;
		friend class gensou_app;
		friend class game_instance;

	public:
		scene();
		virtual ~scene() = default;

		template<typename T>
		static std::shared_ptr<T> load_scene()
		{
			static_assert(std::is_base_of_v<scene, T>);

			auto newScene = std::make_shared<T>();
			return newScene;
		}

		void on_save_state();

		/*--------------overridables------------------------------------*/

		virtual void on_init() {}
		virtual void on_start() {}
		virtual void on_update(float deltaTime) {}
		virtual void on_terminate() {}

		virtual void on_loading_scene_end() { m_scene_state = scene_state::playing; }

		virtual void on_game_save() {}

		virtual void on_window_resize(uint32_t width, uint32_t height);
		virtual void on_viewport_resize(uint32_t width, uint32_t height);

		virtual void on_key_action(key_code key, input_state state);
		virtual void on_touch_down(float x, float y);
		virtual void on_touch_up(float x, float y);
		virtual void on_touch_move(float x, float y);
		virtual void on_pinch_scale(const float scale);
		virtual void on_mouse_button_action(mouse_button key, input_state state);

		virtual void on_mouse_moved(const float x, const float y);
		virtual void on_mouse_scrolled(const float delta);

		/*--------------ecs-related-------------------------------------*/

		game_object create_object(const std::string& name, game_object parent);
		game_object create_object(const std::string& name);
		void destroy_game_object(game_object& gObject);

		void set_current_camera(game_object gObject);
		game_object get_current_camera();

		void set_player(std::shared_ptr<class scene_actor> player) { m_player = player; }
		std::shared_ptr<class scene_actor> get_player() { return m_player; }

		/* return the number of objects to be destroyed */
		uint32_t destroy_all_objects_with_tag(const std::string& tag);
		std::vector<game_object> get_all_objects_with_tag(const std::string& tag);

		/* returns the first object found with tag */
		game_object get_object_with_tag(const std::string& tag);

		game_object get_object_by_id(uuid id);

		/* scene state */

		void pause() { m_scene_state = scene_state::paused; }
		void unpause() { if(m_is_active) m_scene_state = scene_state::playing; }

		/* will stop or start simulating physics. will only have an effect if has_physics is set to true */
		void set_simulation(bool simulate) { simulating = simulate; }
		bool is_simulating() const { return simulating; }

		bool is_playing() const { return m_scene_state == scene_state::playing; }
		bool is_paused() const { return m_scene_state == scene_state::paused; }
		bool is_loading() const { return m_scene_state == scene_state::loading; }

		bool finished_loading() const { return m_finished_loading; }
		bool is_active() const { return m_is_active; }


		/*--------------scene-units-------------------------------------*/

		void set_base_unit_by_height(float quadsPerHeight, float minUnitSize = 32.0f)
		{
			m_calculate_base_unit_by_height = true;
			m_quads_per_dimension = quadsPerHeight;
			m_base_quad_min_size = minUnitSize;

			m_base_quad_size = std::max(m_scene_viewport_in_pixels.height / m_quads_per_dimension, m_base_quad_min_size);

			m_scene_viewport.x = m_scene_viewport_in_pixels.width / m_base_quad_size;
			m_scene_viewport.y = m_quads_per_dimension;
		}

		void set_base_unit_by_width(float quadsPerWidth, float minUnitSize = 32.0f)
		{
			m_calculate_base_unit_by_height = false;
			m_quads_per_dimension = quadsPerWidth;
			m_base_quad_min_size = minUnitSize;

			m_base_quad_size = std::max(m_scene_viewport_in_pixels.width / m_quads_per_dimension, m_base_quad_min_size);

			m_scene_viewport.x = m_quads_per_dimension;
			m_scene_viewport.y = m_scene_viewport_in_pixels.height / m_base_quad_size;
		}

		void set_const_base_unit(float unit)
		{
			m_base_quad_min_size = unit;
			m_scene_viewport.x = m_scene_viewport_in_pixels.width / m_base_quad_size;
			m_scene_viewport.y = m_scene_viewport_in_pixels.height / m_base_quad_size;
			m_const_base_unit = true;
		}

		float get_base_unit_in_pixels() const { return m_base_quad_size; }

		glm::vec2 get_scene_viewport() const { return m_scene_viewport; }

		/* return null if mixer not found */
		std::shared_ptr<audio_mixer> get_audio_mixer(const std::string& mixerName);

	protected:
		[[nodiscard]] virtual scene* get_loading_scene();

		void set_loading_scene_min_duration(float duration) { m_loading_scene_min_duration = duration; }

		/* gives the developer the possibility to batch all textures on a single texture and render everything on a sigle draw call */
		void override_renderer_white_texture(const std::string& path, float u, float v, float strideX, float strideY);
		void override_ui_white_texture(const std::string& path, float u, float v, float strideX, float strideY);

		std::shared_ptr<audio_mixer> add_audio_mixer(const std::string& mixerName);

	private:
		void init();
		void start();
		void update(float deltaTime);
		void terminate();

		void clean_up();

		void add_rigidbody_component(entt::entity ent);

		void create_loading_scene()
		{ 
			if(m_loading_scene = get_loading_scene())
				m_loading_scene->init();
		}

		void update_loading_scene(float dt);

		void ui_viewport_resize(float width, float height);
		bool ui_mouse_button_action(mouse_button key, input_state state);
		bool ui_touch_down(float x, float y);
		bool ui_touch_up(float x, float y);

		void set_custom_engine_texture();

	/* member variables */
	protected:
		using super = scene;

		bool has_physics = true;
		bool simulating = true;
		std::string scene_tag = "default scene";

		std::unordered_map<std::string, std::shared_ptr<audio_mixer>> m_audio_mixers;

	private:
		/* track scene initialization and loading */
		std::atomic<bool> m_is_active = false;
		std::atomic<bool> m_finished_loading = false;
		bool m_resized_during_loading = false;

	private:
		/* ecs related */
		entt::registry m_registry;
		entt::entity m_current_camera{ entt::null };
		entt::entity m_default_scene_camera{ entt::null };

		std::shared_ptr<class scene_actor> m_player;
		std::vector<game_object> m_objects_to_destroy;

		/* physics */
		b2World* m_physics_world = nullptr;
		b2ContactListener* m_contact_listener = nullptr;

		/* scene state */
		enum class scene_state { loading, loaded, playing, paused };
		std::atomic<scene_state> m_scene_state{ scene_state::loading };

		/* the scene base unit will be this quad.
		 * all quads will be converted to pixel space before beeing submited to the renderer
		 */
		float m_quads_per_dimension = 10.0f;
		float m_base_quad_size = 64.0f;
		float m_base_quad_min_size = 32.0f;
		bool m_calculate_base_unit_by_height = true, m_const_base_unit = false;
		enum class base_quad_count_fixed_by { none, screen_height, screen_width };

		glm::vec2 m_scene_viewport{};/* size in base_quad units */
		extent2d m_scene_viewport_in_pixels{};

		/* loading scene */
		float m_loading_scene_min_duration = 0.0f;
		scene* m_loading_scene = nullptr;

		/*---------------------------------*/
		struct
		{
			sprite renderer_white;
			sprite ui_white;

			void clear()
			{
				renderer_white.reset();
				ui_white.reset();
			}

		} m_engine_textures;
	};

}