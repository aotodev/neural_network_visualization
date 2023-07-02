#include "scene/game_statics.h"

#include "scene/game_instance.h"
#include "scene/scene.h"
#include "renderer/ui_renderer.h"

namespace gs {

	void game_statics::change_scene(std::shared_ptr<scene> inScene, bool startScene, bool keepOldSceneAlive)
	{
		game_instance::get()->set_current_scene(inScene, startScene, keepOldSceneAlive);
	}

	std::shared_ptr<scene> game_statics::get_active_scene()
	{
		return game_instance::get()->get_current_scene();
	}

	game_object game_statics::create_game_object(const std::string& name, game_object parent)
	{
		game_object outObj;

		if (auto scene = game_instance::get()->get_current_scene().get())
		{
			outObj = scene->create_object(name, parent);
		}
		else
		{
			LOG_ENGINE(error, "trying to create an entity (game_object) before creating a scene");
		}

		return outObj;
	}

	void game_statics::add_ttf_font(const std::string& path, const std::string& fontName, float maxHeightInPixels)
	{
		ui_renderer::push_font(path, maxHeightInPixels, fontName);
	}

	std::shared_ptr<gensou_file> game_statics::load_save_data(const std::string& path)
	{
		return system::deserialize_data(path);
	}

}