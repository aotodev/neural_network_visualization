#pragma once

#include "core/core.h"
#include "core/uuid.h"

#include "scene/scene.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace gs {

	class audio_clip_component;
	struct transform_component;
	struct anchor_component;
	struct ui_component;
	struct rect2d_component;
	struct base_ui;

	/* base entity, can be used to add and get components 
	 * always has an id, tag, state, relationship and transform component attached 
	 * 16 bytes (12 + padding), cheap to copy */

	class game_object
	{
		friend class scene;
		friend class scene_actor;
		friend class game_statics;

	private:
		game_object(scene* pScene)
			: m_scene(pScene), m_entity(pScene->m_registry.create()) {}

		game_object(entt::entity entity, scene* pScene)
			: m_scene(pScene), m_entity(entity) {}

	public:
		constexpr game_object() = default;

		game_object(const game_object&) = default;
		game_object& operator=(const game_object&) = default;

		template <typename...T>
		bool has_components() const
		{
			return m_scene->m_registry.all_of<T...>(m_entity);
		}

		template<typename... T>
		bool has_any_component() const
		{
			return m_scene->m_registry.any_of<T...>(m_entity);
		}

		template<typename T, typename... Args>
		T& add_component(Args&&... args)
		{
			/* components that should not be added by the user */
			static_assert(!std::is_same_v<T, anchor_component>);
			static_assert(!std::is_same_v<T, ui_component>);
			static_assert(!std::is_same_v<T, rect2d_component>);

			assert(!has_components<T>());

			if constexpr (std::is_base_of_v<base_ui, T>)
			{
				/* only one ui component per entity/object allowed */
				assert(!has_components<ui_component>());

				auto& component = m_scene->m_registry.emplace<T>(m_entity, std::forward<Args>(args)...);
				on_add_ui_component(&component);
				return component;
			}
			else if constexpr (std::is_same_v<T, audio_clip_component>)
			{

				return m_scene->m_registry.emplace<T>(m_entity, *this, std::forward<Args>(args)...);
			}
			else
			{
				return m_scene->m_registry.emplace<T>(m_entity, std::forward<Args>(args)...);
			}
		}

		template<typename T>
		T& get_component() const
		{
			assert(has_components<T>());
			return m_scene->m_registry.get<T>(m_entity);
		}

		/* return null if failed */ 
		template<typename T>
		T* try_get_component() const
		{
			return has_components<T>() ? &m_scene->m_registry.get<T>(m_entity) : nullptr;
		}

		template<typename T>
		void remove_component()
		{
			assert(has_components<T>());
			m_scene->m_registry.remove<T>(m_entity);
		}

		game_object add_child_object(const std::string& name);

		void destroy();

		scene* get_scene() const { return m_scene; }

		uuid id() const;
		const std::string& tag() const;
		glm::mat4 transform() const;
		glm::mat4 world_transform();

		transform_component world_transform_component();

		glm::vec3 get_world_scale() const;
		glm::vec3 get_world_rotation() const;

		/* great for 2d objects (such as ui) if position is all you need. much cheaper than transform/world_transform */
		glm::vec2 local_position() const;
		glm::vec2 world_position() const;

		/* queries all up nodes i.e. the parent (if any), parent of parent, etc */
		bool is_active() const;
		bool is_visible() const;

		/* sets only local components, will not influence parent */
		void set_active();
		void set_inactive();
		void set_visible();
		void set_invisible();

		operator uint32_t () const { return (uint32_t)m_entity; }
		operator entt::entity() const { return m_entity; }

		operator bool() const { return (m_entity != entt::null) && m_scene != nullptr; }

		bool operator==(const game_object& other) const
		{
			return m_entity == other.m_entity && m_scene != nullptr &&  m_scene == other.m_scene;
		}

		bool operator!=(const game_object& other) const
		{
			return !(*this == other);
		}

		void reset() { m_entity = entt::null; m_scene = nullptr; }

		/* the for_each... methods execute a delegate for the current instance as well as all its children */

		/* depth-first-search, just iterates and calls the action without calculating anything else */
		bool for_each(void* data, bool(*action)(game_object, void*));
		bool for_each_visible(void* data, bool(*action)(game_object, void*));

		/* postorder depth-first-search, safe to use for deleting an object (along with its children) */
		bool for_each_postorder(void* data, bool(*action)(game_object, void*));

		/* recursive depth-first-search, calculates world transform as mat4 of each child entity */
		bool for_each(void* data, bool(*action)(game_object, const glm::mat4&, void*));
		bool for_each_visible(void* data, bool(*action)(game_object, const glm::mat4&, void*));

		/* recursive depth-first-search, calculates world transform of each child entity */
		bool for_each_visible(void* data, bool(*action)(game_object, const transform_component&, void*));

	private:
		void on_add_ui_component(struct base_ui* component);

	private:
		scene* m_scene = nullptr;
		entt::entity m_entity{ entt::null };

	};

}