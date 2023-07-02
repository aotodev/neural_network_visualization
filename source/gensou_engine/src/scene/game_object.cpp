#include "scene/game_object.h"

#include "scene/components.h"
#include "scene/audio_clip_component.h"
#include "scene/game_statics.h"

#include "core/system.h"

namespace gs {

	game_object game_object::add_child_object(const std::string& name)
	{
		return m_scene->create_object(name, *this);
	}

	void game_object::on_add_ui_component(struct base_ui* component)
	{
		component->m_game_object = *this;

		m_scene->m_registry.emplace<ui_component>(m_entity);
		m_scene->m_registry.emplace<rect2d_component>(m_entity);

		auto& anchorComp = m_scene->m_registry.emplace<anchor_component>(m_entity);
		anchorComp.m_game_object = *this;
	}

	void game_object::destroy()
	{
		m_scene->destroy_game_object(*this);
	}

	uuid game_object::id() const
	{
		return m_scene->m_registry.get<id_component>(m_entity).id;
	}

	const std::string& game_object::tag() const
	{
		return m_scene->m_registry.get<tag_component>(m_entity).tag;
	}

	glm::mat4 game_object::transform() const
	{
		return m_scene->m_registry.get<transform_component>(m_entity).get_transform();
	}

	glm::mat4 game_object::world_transform()
	{
		glm::mat4 outTransform = transform();
		if (anchor_component* anchor = try_get_component<anchor_component>())
		{
			auto center = anchor->get_center();
			outTransform[3].x += center.x;
			outTransform[3].y += center.y;
		}

		game_object parent = *this;
		while (parent = parent.get_component<relationship_component>().parent)
		{
			glm::mat4 parentTransform = parent.transform();
			if (anchor_component* anchor = parent.try_get_component<anchor_component>())
			{
				auto center = anchor->get_center();
				parentTransform[3].x += center.x;
				parentTransform[3].y += center.y;
			}
			outTransform = parentTransform * outTransform;
		}

		return outTransform;
	}

	transform_component game_object::world_transform_component()
	{
		/* copy, we don't to want to mess with the original transform */
		transform_component outTransform = m_scene->m_registry.get<transform_component>(m_entity);

		if (auto anchor = try_get_component<anchor_component>())
		{
			auto center = anchor->get_center();
			outTransform.translation.x += center.x;
			outTransform.translation.y += center.y;
		}

		game_object gObj = *this;

		while (auto parent = gObj.get_component<relationship_component>().parent)
		{
			auto& parentTransform = parent.get_component<transform_component>();
			if (auto anchor = parent.try_get_component<anchor_component>())
			{
				auto center = anchor->get_center();
				outTransform.translation.x += center.x;
				outTransform.translation.y += center.y;
			}
			outTransform.translation += parentTransform.translation;
			outTransform.rotation += parentTransform.rotation;
			outTransform.scale *= parentTransform.scale;
			gObj = parent;
		}

		return outTransform;
	}

	glm::vec3 game_object::get_world_scale() const
	{
		glm::vec3 outScale= m_scene->m_registry.get<transform_component>(m_entity).scale;

		game_object gObj = *this;

		while (auto parent = m_scene->m_registry.get<relationship_component>(gObj).parent)
		{
			outScale *= m_scene->m_registry.get<transform_component>(parent).scale;
			gObj = parent;
		}

		return outScale;
	}

	glm::vec3 game_object::get_world_rotation() const 
	{
		glm::vec3 outRotation = m_scene->m_registry.get<transform_component>(m_entity).rotation;

		game_object gObj = *this;

		while (auto parent = m_scene->m_registry.get<relationship_component>(gObj).parent)
		{
			outRotation += m_scene->m_registry.get<transform_component>(parent).rotation;
			gObj = parent;
		}

		return outRotation;
	}

	glm::vec2 game_object::local_position() const
	{
		return glm::vec2(m_scene->m_registry.get<transform_component>(m_entity).translation);
	}

	glm::vec2 game_object::world_position() const
	{
		auto position = glm::vec2(get_component<transform_component>().translation);
		if (auto anchor = try_get_component<anchor_component>())
		{
			auto center = anchor->get_center();
			position.x += center.x;
			position.y += center.y;
		}

		game_object gObj= *this;

		while (auto parent = gObj.get_component<relationship_component>().parent)
		{
			auto parentPosition = glm::vec2(parent.get_component<transform_component>().translation);
			if (auto anchor = parent.try_get_component<anchor_component>())
			{
				auto center = anchor->get_center();
				parentPosition.x += center.x;
				parentPosition.y += center.y;
			}
			position += parentPosition;
			gObj = parent;
		}

		return position;
	}

	bool game_object::is_active() const
	{
		if (get_component<state_component>().is_active)
		{
			game_object gObj = *this;
			while (auto parent = gObj.get_component<relationship_component>().parent)
			{
				if (!parent.get_component<state_component>().is_active)
					return false;

				gObj = parent;
			}
			return true;
		}

		return false;
	}

	bool game_object::is_visible() const
	{
		if (get_component<state_component>().is_visible)
		{
			game_object gObj = *this;
			while (auto parent = gObj.get_component<relationship_component>().parent)
			{
				if (!parent.get_component<state_component>().is_visible)
					return false;

				gObj = parent;
			}
			return true;
		}

		return false;
	}

	void game_object::set_active()
	{
		m_scene->m_registry.get<state_component>(m_entity).is_active = true;
	}

	void game_object::set_inactive()
	{
		m_scene->m_registry.get<state_component>(m_entity).is_active = false;
	}

	void game_object::set_visible()
	{
		m_scene->m_registry.get<state_component>(m_entity).is_visible = true;
	}

	void game_object::set_invisible()
	{
		m_scene->m_registry.get<state_component>(m_entity).is_visible = false;
	}

	bool game_object::for_each(void* data, bool(*action)(game_object, void*))
	{
		relationship_component* relationship = &get_component<relationship_component>();
		game_object gObj = *this;

		/* preorder DFS | traverse the entire hierarchy tree */
		bool reachedRoot = false;
		while (!reachedRoot)
		{
			/* vist node */
			if (action(gObj, data))
				return true;

			/* advance to the next node */
			{
				if (relationship->first)
					gObj = relationship->first; /* going down */ 
				else
				{
					while (!gObj.get_component<relationship_component>().next)
					{
						if (!gObj.get_component<relationship_component>().parent)
						{
							reachedRoot = true;
							break;
						}
						gObj = gObj.get_component<relationship_component>().parent; /* go up... */
						if (gObj == *this)
							reachedRoot = true;
					}
					if (reachedRoot) break;
					gObj = gObj.get_component<relationship_component>().next; /* ...then go right */
				}
				relationship = &(gObj.get_component<relationship_component>());
			}
		}

		return false;
	}

	bool game_object::for_each_visible(void* data, bool(*action)(game_object, void*))
	{
		/* handle root */
		{
			auto& state = get_component<state_component>();
			if (!state.is_active || !state.is_visible)
				return false;
			else
				if (action(*this, data))
					return true;
		}

		relationship_component* relationship = &get_component<relationship_component>();
		game_object gObj = relationship->first;
		if (!gObj) return false;

		/* DFS | traverse the entire hierarchy tree */
		bool reachedRoot = false;
		while (!reachedRoot)
		{
			auto& state = get_component<state_component>();
			bool visibleAndActive = state.is_active && state.is_visible;

			/* vist node, if handled return */
			if (visibleAndActive && action(gObj, data))
				return true;

			/* advance to the next node */
			{
				/* if this node is Invisible/Inactive so are its children and there is not point in traversing them */
				if (visibleAndActive && relationship->first)
					gObj = relationship->first; /* going down */
				else
				{
					while (!gObj.get_component<relationship_component>().next) // go up until we find a sibling (if any)
					{
						if (!gObj.get_component<relationship_component>().parent)
						{
							reachedRoot = true;
							break; /* break inner loop */
						}

						gObj = gObj.get_component<relationship_component>().parent; /* go up... */

						if (gObj == *this) /* check if root */
						{
							reachedRoot = true;
							break;  /* break inner loop */
						}
					}

					if (reachedRoot) break; /* break outer loop */

					gObj = gObj.get_component<relationship_component>().next; /* ...then go right */
				}
				relationship = &(gObj.get_component<relationship_component>());
			}
		}

		return false;
	}

	bool game_object::for_each_postorder(void* data, bool(*action)(game_object, void*))
	{
		relationship_component* relationship = &get_component<relationship_component>();
		game_object current = *this;

		/* as the current object may be destroyed on the action, we need to cache its parent and next sibling */
		game_object next, parent;

		/* preorder DFS | traverse the entire hierarchy tree */
		while (true)
		{
			/* go all they down the tree */
			while (current.get_component<relationship_component>().first) 
				current = current.get_component<relationship_component>().first;

			/* cache */
			parent = current.get_component<relationship_component>().parent;
			next = current.get_component<relationship_component>().next;

			/* check if root */
			if (current == *this)
			{
				if(action(current, data))
					return true;

				break;
			}

			if(action(current, data))
				return true;

			/* go to next sibling or parent */
			if(next)
			{
				current = next;
				current.get_component<relationship_component>().previous.reset();
			}
			else
			{
				current = parent;
				current.get_component<relationship_component>().first.reset();
			}
		}

		return false;
	}

	static bool for_each_internal(game_object gObj, const glm::mat4& parentTransform, void* data, bool(*action)(game_object gObj, const glm::mat4& worldTransform, void* data))
	{
		glm::mat4 currentObjTransform = gObj.transform();
		if (anchor_component* anchor = gObj.try_get_component<anchor_component>())
		{
			auto center = anchor->get_center();
			currentObjTransform[3].x += center.x;
			currentObjTransform[3].y += center.y;
		}
		currentObjTransform = currentObjTransform * parentTransform;

		if (action(gObj, currentObjTransform, data))
			return true;

		if (auto firstChild = gObj.get_component<relationship_component>().first) /* go down to child */
		{
			if (for_each_internal(firstChild, currentObjTransform, data, action))
				return true;
		}

		game_object nextSibling = gObj;
		while (nextSibling = nextSibling.get_component<relationship_component>().next) /* go right to sibling */
		{
			if (for_each_internal(nextSibling, parentTransform, data, action))
				return true;
		}

		return false; /* go back up to parent */
	}

	bool game_object::for_each(void* data, bool(*action)(game_object, const glm::mat4&, void*))
	{
		/* handle root */
		glm::mat4 worldTransform = world_transform();

		if (action(*this, worldTransform, data))
			return true;

		/* traverse nodes */
		if (auto firstChild = get_component<relationship_component>().first)
			return for_each_internal(firstChild, worldTransform, data, action);

		return false;
	}

	static bool for_each_visible_internal(game_object gObj, const glm::mat4& parentTransform, void* data, bool(*action)(game_object gObj, const glm::mat4& worldTransform, void* data))
	{
		/* if this entity is invisible, so are the its children. no need to go down this subtree */
		if (gObj.is_visible())
		{
			glm::mat4 currentObjTransform = gObj.transform();
			if (auto anchor = gObj.try_get_component<anchor_component>())
			{
				auto center = anchor->get_center();
				currentObjTransform[3].x += center.x;
				currentObjTransform[3].y += center.y;
			}
			currentObjTransform = currentObjTransform * parentTransform;

			if (action(gObj, currentObjTransform, data))
				return true;

			if (auto firstChild = gObj.get_component<relationship_component>().first) // go down to child
			{
				if (for_each_visible_internal(firstChild, currentObjTransform, data, action))
					return true;
			}
		}

		/* go right to sibling */
		if(auto nextSibling = gObj.get_component<relationship_component>().next)
			return for_each_visible_internal(nextSibling, parentTransform, data, action); // and back up to parent

		return false; /* go back up to parent */
	}

	bool game_object::for_each_visible(void* data, bool(*action)(game_object, const glm::mat4&, void*))
	{
		if (is_visible())
		{
			/* handle root */
			glm::mat4 worldTransform = world_transform();

			if (action(*this, worldTransform, data))
				return true;

			/* traverse nodes */
			if (auto firstChild = get_component<relationship_component>().first)
				return for_each_visible_internal(firstChild, worldTransform, data, action);
		}

		return false;
	}

	/*-------------------------------------------------------------------------------------------------*/
	static bool for_each_visible_internal(game_object gObj, const transform_component& parentTransform, void* data, bool(*action)(game_object gObj, const transform_component& worldTransform, void* data))
	{
		/* if this entity is invisible, so are the its children. no need to go down this subtree */
		if (gObj.is_visible())
		{
			auto currentObjTransform = gObj.get_component<transform_component>();
			if (auto anchor = gObj.try_get_component<anchor_component>())
			{
				auto center = anchor->get_center();
				currentObjTransform.translation.x += center.x;
				currentObjTransform.translation.y += center.y;
			}
			currentObjTransform.translation += parentTransform.translation;
			currentObjTransform.rotation += parentTransform.rotation;
			currentObjTransform.scale *= parentTransform.scale;

			if (action(gObj, currentObjTransform, data))
				return true;

			if (auto firstChild = gObj.get_component<relationship_component>().first) // go down to child
			{
				if (for_each_visible_internal(firstChild, currentObjTransform, data, action))
					return true;
			}
		}

		/* go right to sibling */
		if(auto nextSibling = gObj.get_component<relationship_component>().next)
			return for_each_visible_internal(nextSibling, parentTransform, data, action); // and back up to parent

		return false; /* go back up to parent */
	}

	bool game_object::for_each_visible(void* data, bool(*action)(game_object, const transform_component&, void*))
	{
		if (is_visible())
		{
			/* handle root */
			auto worldTransform = world_transform_component();

			if (action(*this, worldTransform, data))
				return true;

			/* traverse nodes */
			if (auto firstChild = get_component<relationship_component>().first)
				return for_each_visible_internal(firstChild, worldTransform, data, action);
		}

		return false;
	}

}