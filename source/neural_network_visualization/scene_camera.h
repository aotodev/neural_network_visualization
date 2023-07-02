#pragma once

#include <gensou/core.h>
#include <gensou/scene_actor.h>

class scene_camera : public gs::scene_actor
{
public:
	virtual void on_init() override;
    virtual void on_update(float dt) override;

    virtual bool on_mouse_scrolled(const float delta) override;
    virtual bool on_mouse_button_action(mouse_button key, input_state state) override;

	virtual bool on_touch_down(float x, float y) override;
	virtual bool on_touch_up(float x, float y) override;
    virtual bool on_pinch_scale(const float movingSpan) override;

    void reset_camera_transform();

    void set_orbit(bool b)
    { 
        m_orbit = b;
        reset_camera_transform();
    }

private:
    void rotate();
    void orbit(float dt);

    bool m_middle_mouse_button = false;
    bool m_touching = false;
    glm::vec2 m_last_input;

    float m_base_z = 300.0f;

    float m_speed = 100.0f;
    float m_zoom_speed = 10.0f;
    float m_orbit_speed = 0.35f;

    bool m_orbit = false;
};