#pragma once

#include "scene/scene.h"
#include "scene/game_object.h"
#include "scene/components.h"

#include "core/runtime.h"

class default_loading_scene : public gs::scene
{
public:
	virtual void on_init() override
	{
		gs::system::set_clear_value({ 0.0f, 0.0f, 0.0f, 1.0f });

		has_physics = false;
		set_base_unit_by_width(32.0f);
		auto viewport = get_scene_viewport();

		m_loading_text = create_object("loading text");
		auto& text = m_loading_text.add_component<gs::text_component>();
		text.text_size_dynamic = true;
		text.center_text = true;
		text.font_size = viewport.x * 2.5f;
		text.text = "LOADING";

		m_loading_spinner = create_object("loading spinner sprite");
		auto& sprite = m_loading_spinner.add_component<gs::sprite_component>("engine_res/textures/loading_spinner.gsasset");
		sprite.scale_size_by_width(viewport.x * 0.025f);

		auto& pos = m_loading_spinner.get_component<gs::transform_component>().translation;
		pos.y = sprite.get_size().y * 0.65f;

		auto& textPos = m_loading_text.get_component<gs::transform_component>().translation;
		textPos.y = pos.y * + text.font_size * -0.03f;
	}

	virtual void on_update(float dt) override
	{
		m_angle += dt * m_rotation_speed;

		if (m_angle > 6.2831853f)
			m_angle = 0.0f;

		auto& rotation = m_loading_spinner.get_component<gs::transform_component>().rotation;
		rotation.z = m_angle;
	}

private:
	gs::game_object m_loading_text;
	gs::game_object m_loading_spinner;

	float m_angle = 0.0f;
	float m_rotation_speed = 1.75f;
};