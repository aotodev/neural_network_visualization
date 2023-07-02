#pragma once

#include <gensou/core.h>
#include <gensou/components.h>
#include <gensou/scene_actor.h>


class application_widgets : public gs::scene_actor
{
public:

	virtual void on_init() override;

private:
	gs::game_object m_vsync_toggle;
	gs::game_object m_vsync_text;

	gs::game_object m_orbit_toggle;
	gs::game_object m_orbit_text;
};