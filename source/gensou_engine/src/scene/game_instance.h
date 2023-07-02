#pragma once

/******************************************************************************************************** 
 * game_instance is a persistent object instantiated at startup, which is keept                         *
 * alive for the application's life time                                                                *
 * it's an inheritable class perfect for holding information that should be uniform across all scenes   *
 ********************************************************************************************************/

#include "core/core.h"
#include "core/input_codes.h"
#include "core/input.h"

namespace gs {

    class scene;

    class game_instance
    {
        friend class gensou_app;

    private:
        void init();
        void start();
        void update(float dt);
        void terminate();

        static game_instance* create();

    protected:

    public:
        game_instance();
        virtual ~game_instance() = default;
         static game_instance* get() { return s_instance; }

       template<typename T, bool safe = false>
        static T* get_as()
        {
            static_assert(std::is_base_of_v<game_instance, T>);

            if constexpr (safe)
            {   
                return dynamic_cast<T*>(s_instance);
            }
            else
            {
                return static_cast<T*>(s_instance);
            }
        }

        /* called on a loading thread on creation. Great for loading and caching resources during the splash screen */
        virtual void on_create() {}

        virtual void on_init() {}
        virtual void on_start() {}
        virtual void on_update(float dt) {}
        virtual void on_terminate() {}

		std::shared_ptr<scene> get_current_scene() { return m_current_scene; }

		void set_current_scene(std::shared_ptr<scene> inScene, bool startScene = true, bool keepOldSceneAlive = false);

		void on_resize(uint32_t width, uint32_t height);
		void on_save_state();

		void on_key_action(key_code key, input_state state);
		void on_touch_down(float x, float y);
		void on_touch_up(float x, float y);
		void on_touch_move(float x, float y);
        void on_pinch_scale(const float scale);

		void on_mouse_button_action(mouse_button key, input_state state);
		void on_mouse_moved(const float x, const float y);
		void on_mouse_scrolled(const float delta);

    protected:
        virtual std::shared_ptr<scene> create_first_scene() = 0;

    private:
        static game_instance* s_instance;

        std::shared_ptr<scene> m_current_scene;
    };

}

#define GAME_INSTANCE_IMPLEMENTAION(name) gs::game_instance* gs::game_instance::create() { assert(!s_instance); s_instance = new name(); return s_instance; }