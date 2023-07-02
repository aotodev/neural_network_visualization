#pragma once

#include "core/core.h"
#include "core/system.h"

#include "scene/game_object.h"
#include "scene/scene_actor.h"
#include "scene/components.h"
#include "scene/scene.h"
#include <type_traits>

namespace gs {

	class game_statics
	{
	public:

		template<typename T, typename... Args>
		static std::shared_ptr<T> attach_script_component(game_object& gObj, Args&&... args)
		{
			static_assert(std::is_base_of_v<scene_actor, T>);

			auto sceneActor = std::make_shared<T>(std::forward<decltype(args)>(args)...);
			gObj.add_component<script_component>(gObj).instantiate_scene_actor(sceneActor);

			return sceneActor;
		}

		static game_object create_game_object(const std::string& name, game_object parent);

		static game_object create_game_object(const std::string& name)
		{ 
			return create_game_object(name, {});
		}

		template<typename T, typename... Args>
		static auto create_game_object(const std::string& name, game_object parent, Args&&... args)
		{
			game_object gObj = create_game_object(name, parent);

			if constexpr (std::is_base_of_v<scene_actor, T>)
			{
				auto instance = attach_script_component<T>(gObj, std::forward<decltype(args)>(args)...);
				return std::make_pair(gObj, instance);
			}
			else
			{
				auto& component = gObj.add_component<T>(std::forward<decltype(args)>(args)...);
				return std::make_pair(gObj, std::reference_wrapper(component));
			}
		}

		template<typename T, typename... Args>
		static std::shared_ptr<T> spawn_scene_actor(const std::string& name, game_object parent, Args&&... args)
		{
			static_assert(std::is_base_of_v<scene_actor, T>);

			game_object gObj = create_game_object(name, parent);

			auto sceneActor = std::make_shared<T>(std::forward<decltype(args)>(args)...);
			gObj.add_component<script_component>(gObj).instantiate_scene_actor(sceneActor);

			return sceneActor;
		}

		template<typename T, typename... Args>
		static std::shared_ptr<T> spawn_scene_actor(const transform_component& transform, const std::string& name, game_object parent, Args&&... args)
		{
			static_assert(std::is_base_of_v<scene_actor, T>);

			game_object gObj = create_game_object(name, parent);

			auto sceneActor = std::make_shared<T>(std::forward<decltype(args)>(args)...);
			gObj.add_component<script_component>(gObj).instantiate_scene_actor(sceneActor);

			gObj.get_component<transform_component>() = transform;

			return sceneActor;
		}

		template<typename T, typename... Args>
		static std::shared_ptr<T> spawn_scene_actor(const glm::vec3& location, const std::string& name, game_object parent, Args&&... args)
		{
			static_assert(std::is_base_of_v<scene_actor, T>);

			game_object gObj = create_game_object(name, parent);

			auto sceneActor = std::make_shared<T>(std::forward<decltype(args)>(args)...);
			gObj.add_component<script_component>(gObj).instantiate_scene_actor(sceneActor);

			gObj.get_component<transform_component>().translation = location;

			return sceneActor;
		}

		static void change_scene(std::shared_ptr<scene> inScene, bool startScene = true, bool keepOldSceneAlive = false);

		template<typename T>
		static std::shared_ptr<T> create_scene(bool startScene = true, bool keepOldSceneAlive = false)
		{
			static_assert(std::is_base_of_v<scene, T>);
			auto outScene = std::make_shared<T>();

			change_scene(outScene, startScene, keepOldSceneAlive);

			return outScene;
		}

		static std::shared_ptr<scene> get_active_scene();

		///////////////////////////////////////////////////////
		static void add_ttf_font(const std::string& path, const std::string& fontName, float maxHeight);

		template<typename T>
		static void save_data(const std::string& path, const T* pData)
		{
			system::serialize_data(path, pData, sizeof(T));
		}

		/* returns a buffer of raw data loaded from file. it also checks the hash code to make sure the data is not corrupted */
		static std::shared_ptr<gensou_file> load_save_data(const std::string& path);

		/* T must be trivially copyable */
		template<typename T>
		static std::unique_ptr<T> load_save_data_as(const std::string& path)
		{
			static_assert(std::is_trivially_copyable_v<T>);

			if (auto gFile = load_save_data(path))
			{
				return gFile->get_data_as<T>();
			}
			else
			{
				return std::unique_ptr<T>(nullptr);
			}
		}
	};
}