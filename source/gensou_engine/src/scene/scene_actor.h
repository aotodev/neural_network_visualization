#pragma once

#include "scene/game_object.h"
#include "scene/scene.h"
#include "scene/game_instance.h"

#include "core/log.h"
#include "core/input.h"

namespace gs {

    class scene_actor
    {
    public:
        //protected constructor?
        scene_actor() = default;
        virtual ~scene_actor() = default;

        template<typename T, bool safe = false>
        T* get_as()
        {
            static_assert(std::is_base_of_v<scene_actor, T>);

            if constexpr (safe)
            {   
                return dynamic_cast<T*>(this);
            }
            else
            {
                return static_cast<T*>(this);
            }
        }

        game_object get_game_object() const { return m_game_object; }
        uint64_t id() const { return m_game_object.id(); }

        const std::string& tag() const { return m_game_object.tag(); }

        bool is_active() { return m_game_object.is_active(); }
        void set_active() { m_game_object.set_active(); }
        void set_inactive() { m_game_object.set_inactive(); }

        bool is_visible() { return m_game_object.is_visible(); }
        void set_visible() { m_game_object.set_visible(); }
        void set_invisible() { m_game_object.set_invisible(); }

        /* destroyes actor and object on the next frame */
        void destroy()
        {
            m_destroy_object = true;
        }

    protected:
        virtual void on_init() {} // called when created 
        virtual void on_start() {} // called when instantiated in the game
        virtual void on_update(float deltaTime) {}
        virtual void on_terminate() {}

        virtual void on_begin_contact(scene_actor* other) {}
        virtual void on_end_contact(scene_actor* other) {}

        virtual void on_game_save() {}

        /* passes scene viewport in local units (not in pixels) */
        virtual void on_viewport_resize(float width, float height) {}

        virtual bool on_key_action(key_code key, input_state state) { return false; }
        virtual bool on_touch_down(float x, float y) { return false; }
        virtual bool on_touch_up(float x, float y) { return false; }
        virtual bool on_touch_move(float x, float y) { return false; }
        virtual bool on_pinch_scale(const float) { return false; }

        virtual bool on_mouse_button_action(mouse_button key, input_state state) { return false; }
        virtual bool on_mouse_moved(const float x, const float y) { return false; }
        virtual bool on_mouse_scrolled(const float delta) { return false; }

        game_object add_subobject(const std::string& name);

        template<typename T, typename... Args>
        game_object add_subobject(const std::string& name, Args... args)
        {
            return game_object();
        }

        template<typename T, typename... Args>
        T& add_component(Args&&... args) { return m_game_object.add_component<T>(std::forward<Args>(args)...); }

        template<typename T>
        void remove_component() { m_game_object.remove_component<T>(); }

        template<typename T>
        T& get_component() { return m_game_object.get_component<T>(); }

        void set_camera(game_object cameraObject)
        {
            m_scene_ref->set_current_camera(cameraObject);
        }

        void set_camera()
        {
            m_scene_ref->set_current_camera(m_game_object);
        }

        class scene* get_scene() { return m_scene_ref; }
        class game_instance* get_game_instance()
        { 
            return game_instance::get();
        }
        
        template<typename T, bool safe = false>
        T* get_scene_as()
        {
            static_assert(std::is_base_of_v<scene, T>);

            if constexpr (safe)
            {
                return dynamic_cast<T*>(m_scene_ref);
            }
            else
            {
                return static_cast<T*>(m_scene_ref);
            }
        }

        template<typename T, bool safe = false>
        T* get_game_instance_as()
        {
            return game_instance::get_as<T, safe>();
        }

    private:
        game_object m_game_object;
        scene* m_scene_ref = nullptr;
        bool m_destroy_object = false;

        friend class scene;
        friend class script_component;
        friend class contact_callback;
    };


    // use entt on_contruct //.connect // on_update (reload dynamic lib)
    class script_component
    {
    public:
        script_component(game_object gObj) : m_game_object(gObj) {}
        ~script_component()
        {
            if (m_scene_actor_instance)
            {
                m_scene_actor_instance->on_terminate();
                m_scene_actor_instance.reset();
            }
        }

        void instantiate_scene_actor(std::shared_ptr<scene_actor> pBehaviour)
        {
            assert(m_scene_actor_instance == nullptr);
            assert(pBehaviour != nullptr);

            m_scene_actor_instance = pBehaviour;
            m_scene_actor_instance->m_game_object = m_game_object;
            m_scene_actor_instance->m_scene_ref = m_game_object.get_scene();
            m_scene_actor_instance->on_init();
        }

        std::shared_ptr<scene_actor> get_instance() { return m_scene_actor_instance; }

        template<typename T>
        std::shared_ptr<T> get_instance_as()
        {
            static_assert(std::is_base_of_v<scene_actor, T>);
            return std::dynamic_pointer_cast<T>(m_scene_actor_instance);
        }

        bool is_active() { return m_game_object.is_active() && m_has_started; }

    private:
        std::shared_ptr<scene_actor> m_scene_actor_instance;

        game_object m_game_object;
        bool m_has_started = false;

        friend class scene;
    };

}
