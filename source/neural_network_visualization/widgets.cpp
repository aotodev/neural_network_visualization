#include "widgets.h"

#include "application_scene.h"

void application_widgets::on_init()
{
    auto viewport = get_scene()->get_scene_viewport();

	/*--------------------TOGGLES-------------------------------------------------*/
	bool supportsNonVsync = gs::system::supports_nonvsync_mode();
	bool supportsRumbler = gs::system::supports_rumbler();

	glm::vec2 rectSize = { viewport.y * 0.0882f, viewport.y * 0.0504f };
	float fontSize = rectSize.y;

	//---------------------------------------------------------------------------
	// orbit toggle
	//---------------------------------------------------------------------------
	m_orbit_toggle = add_subobject("orbit toogle");
	auto& orbitToggle = m_orbit_toggle.add_component<gs::toggle_switch_component>();
	orbitToggle.set_rect(rectSize);
	orbitToggle.handle_scale = 0.9f;
	orbitToggle.user_data = this;
	orbitToggle.set_off();

	m_orbit_toggle.get_component<gs::anchor_component>().set(gs::anchor::top_left);

	auto& orbitTogglePosition = m_orbit_toggle.get_component<gs::transform_component>().translation;
	orbitTogglePosition.x = rectSize.x * 1.0f;
	orbitTogglePosition.y = rectSize.y * 2.0f;

	orbitToggle.on_toggle_action = [](gs::toggle_switch_component* toggle, gs::scene* scene, bool on, void* data)
	{
        auto appScene = static_cast<application_scene*>(scene);
        appScene->set_camera_orbiting(on);
	};

	/*------------------orbit-text--------------------------------*/
	m_orbit_text = add_subobject("orbit text");
	auto& orbitText = m_orbit_text.add_component<gs::text_component>();
	orbitText.text = "Orbit model";
	orbitText.text_size_dynamic = true;
	orbitText.font_size = fontSize;
	orbitText.color = { 1.0f, 1.0f, 1.0f, 1.0f };

	m_orbit_text.get_component<gs::anchor_component>().set(gs::anchor::top_left);

	auto& orbitTextPos = m_orbit_text.get_component<gs::transform_component>().translation;
	orbitTextPos.x = orbitTogglePosition.x + rectSize.x * 1.0f;
	orbitTextPos.y = orbitTogglePosition.y;

	/*---------------------vsyn-toggle---------------------------------------*/
	if (supportsNonVsync)
	{
		m_vsync_toggle = add_subobject("vsync toogle");
		auto& vsyncToggle = m_vsync_toggle.add_component<gs::toggle_switch_component>();
		vsyncToggle.set_rect(rectSize);
		gs::system::vsync() ? vsyncToggle.set_on() : vsyncToggle.set_off();
		vsyncToggle.user_data = this;
		vsyncToggle.handle_scale = orbitToggle.handle_scale;

		m_vsync_toggle.get_component<gs::anchor_component>().set(gs::anchor::top_left);

		auto& vsyncTogglePosition = m_vsync_toggle.get_component<gs::transform_component>().translation;
		vsyncTogglePosition.x = rectSize.x * 1.0f;
		vsyncTogglePosition.y = rectSize.y * 4.0f;

		vsyncToggle.on_toggle_action = [](gs::toggle_switch_component* toggle, gs::scene* scene, bool on, void* data)
		{
			gs::system::set_vsync(on);
		};

		/*------------------vsync-text--------------------------------*/
		m_vsync_text = add_subobject("vsync text");
		auto& vsyncText = m_vsync_text.add_component<gs::text_component>();
		vsyncText.text = "Vsync";
		vsyncText.text_size_dynamic = true;
		vsyncText.font_size = fontSize;
		vsyncText.color = { 1.0f, 1.0f, 1.0f, 1.0f };

		m_vsync_text.get_component<gs::anchor_component>().set(gs::anchor::top_left);

		auto& vsyncTextPos = m_vsync_text.get_component<gs::transform_component>().translation;
		vsyncTextPos.x = vsyncTogglePosition.x + rectSize.x * 1.0f;
		vsyncTextPos.y = vsyncTogglePosition.y;
	}

}