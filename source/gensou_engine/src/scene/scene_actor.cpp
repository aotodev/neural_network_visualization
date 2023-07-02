#include "scene/scene_actor.h"

#include "scene/game_statics.h"

namespace gs {

	game_object scene_actor::add_subobject(const std::string& name)
	{
		return m_scene_ref->create_object(name, m_game_object);
	}

}