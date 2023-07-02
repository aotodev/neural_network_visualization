#include "scene/scene.h"
#include "scene/audio_mixer.h"
#include "scene/game_object.h"

#include "scene/scene_actor.h"
#include "scene/game_object.h"
#include "scene/game_statics.h"
#include "scene/components.h"
#include "scene/particle_system.h"
#include "scene/default_loading_scene.h"

#include "renderer/renderer.h"
#include "renderer/ui_renderer.h"
#include "renderer/geometry/cube.h"

#include "core/core.h"
#include "core/log.h"
#include "core/system.h"
#include "core/time.h"

#include "core/gensou_app.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>

PUSH_IGNORE_WARNING
#include <box2d/b2_world.h>
#include <box2d/b2_body.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_world_callbacks.h>
#include <box2d/b2_contact.h>
POP_IGNORE_WARNING

namespace gs {

	class contact_callback : public b2ContactListener
	{
	public:
		virtual void BeginContact(b2Contact* contact) override
		{
			scene_actor* actor_a = (scene_actor*)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
			scene_actor* actor_b = (scene_actor*)contact->GetFixtureB()->GetBody()->GetUserData().pointer;

			if (!actor_a || !actor_b)
				return;

			actor_a->on_begin_contact(actor_b);
		}

		virtual void EndContact(b2Contact* contact) override
		{
			scene_actor* actor_a = (scene_actor*)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
			scene_actor* actor_b = (scene_actor*)contact->GetFixtureB()->GetBody()->GetUserData().pointer;

			if (!actor_a || !actor_b)
				return;

			actor_a->on_end_contact(actor_b);
		}
	};

	scene::scene()
		: m_scene_viewport_in_pixels(runtime::viewport())
	{
		LOG_ENGINE(trace, "scene constructor");
		m_objects_to_destroy.reserve(32);
	}

	scene* scene::get_loading_scene()
	{
		return new default_loading_scene();
	}

	void scene::init()
	{
		glm::vec2 viewport = { m_scene_viewport_in_pixels.width,  m_scene_viewport_in_pixels.height };

		game_object cameraObject = create_object("default scene_camera", {});
		m_default_scene_camera = cameraObject;
		auto& camera = cameraObject.add_component<camera_component>();
		camera.set_perspective(0.785398f);
		camera.set_viewport_size((uint32_t)viewport.x, (uint32_t)viewport.y);

		auto& cameraPos = cameraObject.get_component<transform_component>().translation;
		cameraPos.z = 300.0f;

		#if INVERT_VIEWPORT
		camerapos.z *= -1.0f;
		#endif

		camera.update(cameraObject.get_component<transform_component>());
		for (uint32_t i = 0; i < runtime::get_frames_in_flight_count(); i++)
			renderer::update_view_projection(camera.get_projection_view(), i);

		if(m_current_camera == entt::null)
			m_current_camera = m_default_scene_camera;

		on_init();

		m_finished_loading = true;

		LOG_ENGINE(trace, "init scene with tag '%s'", scene_tag.c_str());
	}

	void scene::start()
	{
		LOG_ENGINE(trace, "starting scene with tag '%s'", scene_tag.c_str());

		auto& camera = m_registry.get<camera_component>(m_default_scene_camera);
		camera.set_perspective(0.785398f);
		camera.set_viewport_size(runtime::viewport().width, runtime::viewport().height);

		if (m_current_camera == entt::null)
			m_current_camera = m_default_scene_camera;

		if (has_physics)
		{
			float gravity = 9.81f;
			#if INVERT_VIEWPORT
			gravity *= -9.81f;
			#endif	

			m_physics_world = new b2World({ 0.0f, gravity });

			auto view = m_registry.view<rigidbody2d_component>();
			for (auto ent : view)
			{
				add_rigidbody_component(ent);
			}

			m_contact_listener = new contact_callback();
			m_physics_world->SetContactListener(m_contact_listener);
		}

		set_custom_engine_texture();

		m_is_active = true;
		m_scene_state = scene_state::playing;

		on_start();
	}

	game_object scene::create_object(const std::string& name, game_object parent)
	{
		game_object gObj(this);

		auto& relation = gObj.add_component<relationship_component>();

		if (parent)
		{
			auto& parentRelation = parent.get_component<relationship_component>();
			relation.parent = parent;

			if (parentRelation.children_count == 0) // has no children
			{
				parentRelation.first = gObj;
				parentRelation.last = gObj;
			}
			else
			{
				auto& previousRelation = parentRelation.last.get_component<relationship_component>();
				previousRelation.next = gObj;
				relation.previous = parentRelation.last;
				parentRelation.last = gObj;
			}

			parentRelation.children_count++;
		}

		gObj.add_component<id_component>();
		gObj.add_component<tag_component>(name);
		gObj.add_component<state_component>();
		gObj.add_component<transform_component>();

		return gObj;
	}

	game_object scene::create_object(const std::string& name)
	{
		return create_object(name, {});
	}

	void scene::destroy_game_object(game_object& gObject)
	{
		/* update parent/sibilings relationship (if any) */
		if(auto parent = gObject.get_component<relationship_component>().parent)
		{
			auto previous = gObject.get_component<relationship_component>().previous;
			auto next = gObject.get_component<relationship_component>().next;

			auto& parentRelation = parent.get_component<relationship_component>();

			if(previous)
			{
				/* can be an empty entity if next is null */
				previous.get_component<relationship_component>().next = next;

				if(next)
					next.get_component<relationship_component>().previous = previous;
					
			}
			else /* it was the parent's first child */
			{
				/* can be an empty entity if next is null */
				parentRelation.first = next;

				/* it was the parents only child */
				if(parentRelation.last == gObject)
				{
					parentRelation.last = next;
				}

				if(next)
					next.get_component<relationship_component>().previous.reset();
			}

			parentRelation.children_count--;
		}

		/* delete all children as well */
		gObject.for_each_postorder(this, [](game_object gObj, void* data) -> bool
		{
			auto pScene = static_cast<scene*>(data);

			if (pScene->has_physics)
			{
				if (gObj.has_components<rigidbody2d_component>())
				{
					auto& rigidbody = gObj.get_component<rigidbody2d_component>();

					if (auto body = rigidbody.body)
					{
						pScene->m_physics_world->DestroyBody(body);
						rigidbody.body = nullptr;
					}
				}
			}

			pScene->m_registry.destroy(gObj.m_entity);

			return false;
		});

		gObject.reset();
	}

	void scene::set_current_camera(game_object gObject)
	{
		m_current_camera = gObject ? gObject : m_default_scene_camera;
	}

	game_object scene::get_current_camera()
	{
		return game_object(m_current_camera, this);
	}

	uint32_t scene::destroy_all_objects_with_tag(const std::string& tag)
	{		
		auto view = m_registry.view<tag_component>();
		uint32_t i = 0;
		for (auto [ent, tagComp] : view.each())
		{
			if(tagComp.tag == tag)
			{
				i++;
				m_objects_to_destroy.push_back(game_object(ent, this));
			}
		}

		LOG_ENGINE(warn, "set to destroy %u objects with tag '%s'", i, tag.c_str());

		return i;
	}

	std::vector<game_object> scene::get_all_objects_with_tag(const std::string& tag)
	{
		std::vector<game_object> outVector;
		outVector.reserve(16);

		auto view = m_registry.view<tag_component>();
		for (auto [ent, tagComp] : view.each())
		{
			if(tagComp.tag == tag)
			{
				outVector.push_back(game_object(ent, this));
			}
		}

		return outVector;
	}

	game_object scene::get_object_with_tag(const std::string& tag)
	{
		auto view = m_registry.view<tag_component>();
		for (auto [ent, tagComp] : view.each())
		{
			if(tagComp.tag == tag)
			{
				return game_object(ent, this);
			}
		}

		LOG_ENGINE(warn, "could not find object with tag '%s', returning an empty game_object", tag.c_str());
		return game_object();
	}

	game_object scene::get_object_by_id(uuid id)
	{
		auto view = m_registry.view<id_component>();
		for (auto [ent, idComp] : view.each())
		{
			if(idComp.id == id)
			{
				return game_object(ent, this);
			}
		}

		LOG_ENGINE(warn, "could not find object with id %zu, returning an empty game_object", id);
		return game_object();
	}

	void scene::add_rigidbody_component(entt::entity ent)
	{
		game_object gObj(ent, this);
		auto& transform = gObj.get_component<transform_component>();
		auto& rigidbody = gObj.get_component<rigidbody2d_component>();

		if (auto body = rigidbody.body)
			m_physics_world->DestroyBody(body);

		b2BodyDef bodyDef;
		bodyDef.type = (b2BodyType)rigidbody.body_type;

		LOG_ENGINE(warn, "tag '%s', [%.3f, %.3f]", gObj.tag().c_str(), transform.translation.x, transform.translation.y);
		auto position = gObj.world_position();
		LOG_ENGINE(info, "wolrd pos [%.3f, %.3f]", position.x, position.y);

		bodyDef.position.Set(position.x, position.y);
		bodyDef.angle = transform.rotation.z;
		bodyDef.gravityScale = rigidbody.gravity_scale;
		bodyDef.userData.pointer = (uintptr_t)rigidbody.get_data_pointer();

		auto pBody = m_physics_world->CreateBody(&bodyDef);
		pBody->SetFixedRotation(rigidbody.fixed_rotation);
		pBody->SetLinearVelocity({ rigidbody.linear_velocity.x, rigidbody.linear_velocity.y });
		rigidbody.body = pBody;

		if (box_collider2d_component* boxCollider = gObj.try_get_component<box_collider2d_component>())
		{
			b2PolygonShape polygonShape;
			glm::vec2 boxShape{ boxCollider->x_half_extent * transform.scale.x, boxCollider->y_half_extent * transform.scale.y };

			if (boxCollider->center.x + boxCollider->center.y == 0.0f)
			{
				polygonShape.SetAsBox(boxShape.x, boxShape.y);
			}
			else
			{
				glm::vec2 center{ boxCollider->center.x * transform.scale.x, boxCollider->center.y * transform.scale.y };
				polygonShape.SetAsBox(boxShape.x, boxShape.y, { center.x, center.y }, 0.0f);
			}

			b2FixtureDef fixture;
			fixture.shape = &polygonShape;
			fixture.density = boxCollider->density;
			fixture.friction = boxCollider->friction;
			fixture.restitution = boxCollider->restitution;
			fixture.restitutionThreshold = boxCollider->restitution_threashold;

			pBody->CreateFixture(&fixture);
		}

		rigidbody.recreate = false;
	}

	void scene::update_loading_scene(float dt)
	{
		if (m_finished_loading) // set to true atomically when the init(load) funtion returns from the loading thread
		{
			if (m_loading_scene_min_duration <= 0.0f)
			{
				if (m_loading_scene)
				{
					renderer::wait_render_cmds();
					command_manager::reset_all_pools();
					renderer::reset_render_cmds();

					m_loading_scene->terminate();
					delete m_loading_scene;
					m_loading_scene = nullptr;

					if (m_resized_during_loading)
						on_viewport_resize(rt::viewport().width, rt::viewport().height);
				}

				m_loading_scene_min_duration = 0.0f;
				m_scene_state = scene_state::loaded;
				start();

				return;
			}
		}

		if (m_loading_scene)
		{
			if (!m_loading_scene->is_active())
				m_loading_scene->start();

			m_loading_scene->update(dt);
		}

		m_loading_scene_min_duration -= dt;
	}

	void scene::update(float deltaTime)
	{
		BENCHMARK("scene on update");

		if (deltaTime > 1.0f)
		{
			LOG_ENGINE(trace, "delta time bigger than 1.0f | [%.3f]", deltaTime);
			return;
		}

		if(is_loading())
		{
			update_loading_scene(deltaTime);
			return;
		}

		if (is_playing())
		{
			//scripts
			{
				BENCHMARK_VERBOSE("Scripts")

				auto view = m_registry.view<script_component>();
				for (auto ent : view)
				{
					game_object gObj(ent, this);

					auto& script = view.get<script_component>(ent);
					auto instance = script.get_instance();

					if (instance->m_destroy_object)
					{
						destroy_game_object(gObj);
						continue;
					}

					if (!gObj.is_active())
						continue;

					if (!script.m_has_started)
					{
						script.m_has_started = true;

						if (auto rigidbody = gObj.try_get_component<rigidbody2d_component>())
							add_rigidbody_component(ent);

						instance->on_start();
					}

					instance->on_update(deltaTime);
				}
			}

			// physics 2d
			if (has_physics && simulating)
			{
				BENCHMARK_VERBOSE("Physics")

				const int32_t velocityIterations = 6;
				const int32_t positionIterations = 2;
			
				m_physics_world->Step(std::min(deltaTime, 0.1f), velocityIterations, positionIterations);

				auto view = m_registry.view<rigidbody2d_component>();
				for (auto ent : view)
				{
					game_object gObj(ent, this);

					if (!gObj.is_active())
						continue;

					auto& transform = gObj.get_component<transform_component>();
					auto& rigidbody = gObj.get_component<rigidbody2d_component>();

					if (rigidbody.body == nullptr || rigidbody.recreate)
					{
						if(rigidbody.recreate)
							LOG_ENGINE(warn, "recreating rigidbody_component for entity with tag '%s'", gObj.tag().c_str());

						add_rigidbody_component(ent);
					}

					transform.translation.x = rigidbody.body->GetPosition().x;
					transform.translation.y = rigidbody.body->GetPosition().y;
					transform.rotation.z = rigidbody.body->GetAngle();

					#if RENDER_BOXCOLLIDER
					if (box_collider2d_component* box = gObj.try_get_component<box_collider2d_component>())
					{
						auto boxTransform = transform; /* copy, not reference */
						boxTransform.translation.x += box->center.x * boxTransform.scale.x;
						boxTransform.translation.y += box->center.y * boxTransform.scale.y;
						boxTransform.translation.z = 0.0f;

						boxTransform.translation.x *= m_base_quad_size;
						boxTransform.translation.y *= m_base_quad_size;

						ui_renderer::get()->submit_quad({ box->x_half_extent * 2.0f * m_base_quad_size, box->y_half_extent * 2.0f * m_base_quad_size }, boxTransform.get_transform(), glm::vec4(1.0f, 0.0f, 0.0f, 0.4f));
					}
					#endif
				}
			}
		}

		//camera
		{
			BENCHMARK_VERBOSE("camera")

			auto view = m_registry.view<camera_component>();
			for (auto ent : view)
			{
				if (ent == m_current_camera)
				{
					auto& camera = view.get<camera_component>(ent);
					transform_component& transform = m_registry.get<transform_component>(ent);

					camera.update(transform);
					renderer::update_view_projection(camera.get_projection_view(), rt::current_frame());

					break;
				}
			}
		}

		// line
		{
			auto view = m_registry.view<line_renderer_component>();
			for (auto it = view.rbegin(); it != view.rend(); it++)
			{
				game_object gObj(*it, this);

				if (!gObj.is_visible())
					continue;

				auto& lineRenderer = gObj.get_component<line_renderer_component>();
				auto transform = gObj.world_transform();

				/* convert position from world space to pixel space */
				transform[3].x *= m_base_quad_size;
				transform[3].y *= m_base_quad_size;
				transform[3].z *= m_base_quad_size;

				uint32_t end = lineRenderer.end;
				if(end < 0)
					end = lineRenderer.lines.size();

				uint32_t start = std::min(lineRenderer.start, int32_t(lineRenderer.lines.size() - 1UL));
				end = std::min(end, uint32_t(lineRenderer.lines.size()));

				if(lineRenderer.size_in_pixels)
				{
					uint32_t count = end - start;
					if(count > 0)
						renderer::submit_line_range(&lineRenderer.lines[start].p1, count, lineRenderer.edge_range);
				}
				else
				{
					for (uint32_t i = start; i < end; i++)
					{
						const auto& line = lineRenderer.lines[i];
						renderer::submit_line(lineRenderer.edge_range, line.p1.position * m_base_quad_size, line.p1.color, line.p2.position * m_base_quad_size, line.p2.color);
					}
				}
			}
		}

		// cube
		{
			auto view = m_registry.view<cube_component>();
			for (auto it = view.rbegin(); it != view.rend(); it++)
			{
				game_object gObj(*it, this);

				if (!gObj.is_visible())
					continue;

				auto& cube = gObj.get_component<cube_component>();

				auto transform = gObj.world_transform();

				/* convert position from world space to pixel space */
				transform[3].x *= m_base_quad_size;
				transform[3].y *= m_base_quad_size;
				transform[3].z *= m_base_quad_size;

				renderer::submit_cube(cube.color, transform);
			}
		}

		//sprites
		{
			BENCHMARK_VERBOSE("sprites")

			auto view = m_registry.view<sprite_component>();
			for (auto it = view.rbegin(); it != view.rend(); it++)
			{
				game_object gObj(*it, this);

				if (!gObj.is_visible())
					continue;

				auto& sprite = gObj.get_component<sprite_component>();

				if(sprite.is_hidden())
					continue;

				if(!is_paused() || sprite.animate_when_inactive)
					sprite.animate(deltaTime);
				
				auto transform = gObj.world_transform();

				/* convert position from world space to pixel space */
				transform[3].x *= m_base_quad_size;
				transform[3].y *= m_base_quad_size;

				glm::vec2 size = sprite.get_size() * m_base_quad_size;

				if (auto tex = sprite.get_texture())
				{
					renderer::submit_quad(tex, sprite.get_coords(), sprite.get_stride(), size, sprite.color, transform, sprite.squash_constant, sprite.mirror_texture);
				}
				else
				{
					renderer::submit_quad(size, transform, sprite.color);			
				}
			}
		}

		// particles
		{
			auto particleSystemView = m_registry.view<particle_system>();
			for (auto [ent, system] : particleSystemView.each())
			{
				if (!system.is_system_active())
					continue;

				system.ou_update(deltaTime);

				game_object gObj(ent, this);

				const auto systemTransform = gObj.world_transform_component();
				auto particleTransform = systemTransform;

				for(auto& particle : system)
				{
					if (!particle.active)
						continue;

					particleTransform.translation.x = systemTransform.translation.x + particle.position.x;
					particleTransform.translation.y = systemTransform.translation.y + particle.position.y;
					particleTransform.rotation.z = systemTransform.rotation.z + particle.rotation;

					particleTransform.translation.x *= m_base_quad_size;
					particleTransform.translation.y *= m_base_quad_size;

					glm::vec2 size = particle.size * m_base_quad_size;

					if (system.m_texture_sprite)
					{
						renderer::submit_quad(system.m_texture_sprite, particle.texture_uv, system.m_texture_uv_stride, size, particle.color, particleTransform.get_transform(), 1.0f, false);
					}
					else
						renderer::submit_quad(size, particleTransform.get_transform(), particle.color);
				}
			}

		}

		/*--------------------------------UI-COMPONENTS------------------------------------------------*/
		/*---------------------------------------------------------------------------------------------*/
		auto view = m_registry.view<ui_component>();
		for (auto ent : view)
		{
			game_object gObj(ent, this);

			// only root entities will be used, the others will be traversed from the root
			if (auto parent = gObj.get_component<relationship_component>().parent)
            {
                if (parent.has_components<ui_component>())
                    continue;
            }

			gObj.for_each_visible(this, [](game_object gameObject, const transform_component& worldTransform, void* data) -> bool
			{
				auto pScene = static_cast<scene*>(data);
				auto transform = worldTransform.get_transform();

				/* convert position from world space to pixel space */
				transform[3].x *= pScene->m_base_quad_size;
				transform[3].y *= pScene->m_base_quad_size;

				if (auto image = gameObject.try_get_component<image_component>())
				{
					glm::vec2 size = image->get_rect_size() * pScene->m_base_quad_size;

					if (image->blur_texture)
					{
						ui_renderer::submit_blurred_background_image(size, transform, image->color, image->round_corners_radius);
					}
					else
					{
						if (image->get_texture())
						{
							ui_renderer::submit_image(image->get_texture(), image->get_texture_coords(), image->get_texture_stride(), size, image->color, transform, image->round_corners_radius);
						}
					}

					/* each game_object(entity) can have only one ui component */
					return false;
				}

				if (auto button = gameObject.try_get_component<button_component>())
				{
					glm::vec2 size = button->get_rect_size() * pScene->m_base_quad_size;
					ui_renderer::submit_button(button, size, transform, pScene);

					return false;
				}

				if (auto text = gameObject.try_get_component<text_component>())
				{
					ui_renderer::submit_text(text, transform, pScene->m_base_quad_size);

					return false;
				}

				if (auto slider = gameObject.try_get_component<slider_component>())
				{
					glm::vec2 size = slider->get_rect_size() * pScene->m_base_quad_size;
					ui_renderer::submit_slider(slider, size, transform, pScene);

					return false;
				}

				if (auto toggle = gameObject.try_get_component<toggle_switch_component>())
				{
					glm::vec2 size = toggle->get_rect_size() * pScene->m_base_quad_size;
					ui_renderer::submit_toggle_switch(toggle, size, transform, pScene);

					return false;
				}

				if (auto bar = gameObject.try_get_component<bar_component>())
				{
					glm::vec2 size = bar->get_rect_size() * pScene->m_base_quad_size;
					ui_renderer::submit_bar(bar, size, transform);

					return false;
				}

				if (auto dialog = gameObject.try_get_component<dialog_box_component>())
				{
					if(!dialog->m_open)
						return false;

					if(dialog->m_updating)
						dialog->update(rt::delta_time());

					ui_renderer::submit_dialog_box(dialog, transform, pScene->m_base_quad_size);

					return false;
				}

				if(auto sprite = gameObject.try_get_component<ui_sprite_component>())
				{
					if (auto tex = sprite->get_texture())
					{	
						sprite->animate(rt::delta_time());
						glm::vec2 size = sprite->get_size() * pScene->m_base_quad_size;
						ui_renderer::submit_quad(tex, sprite->get_coords(), sprite->get_stride(), size, sprite->color, transform, sprite->mirror_texture);
					}
				}

				return false;
			});
		}

		on_update(deltaTime);
		clean_up();

		#if VIEWPORT_FRAME_TIME && !defined(APP_SHIPPING)
		transform_component dtTransform;
		dtTransform.translation.x = rt::viewport().width * -0.47f;
		ui_renderer::submit_text(std::to_string((int)(1.0f / deltaTime)), 0.36f, glm::vec4(1.0f, 0.5f, 0.1f, 1.0f), dtTransform.get_transform(), false, "default", 0.0f);
		#endif
	}

	void scene::terminate()
	{
		LOG_ENGINE(trace, "terminating scene with tag '%s'", scene_tag.c_str());

		on_terminate();

		m_registry.clear<id_component>();

		if(has_physics)
		{
			if (m_contact_listener)
			{
				delete m_contact_listener;
				m_contact_listener = nullptr;
			}
				
			if (m_physics_world)
			{
				delete m_physics_world;
				m_physics_world = nullptr;
			}
		}

		m_engine_textures.clear();
	}

	void scene::clean_up()
	{
		if(!m_objects_to_destroy.empty())
		{
			LOG_ENGINE(warn, "destroying %zu objects on clean_up", m_objects_to_destroy.size());

			for(auto obj : m_objects_to_destroy)
				destroy_game_object(obj);

			m_objects_to_destroy.clear();
		}
	}

	void scene::ui_viewport_resize(float width, float height)
	{
		glm::vec2 viewport = { width, height };

		auto view = m_registry.view<ui_component>();
		for (auto ent : view) // only root entities will be used, the others will be traversed from the root
		{
			game_object gObj(ent, this);
			relationship_component* relationship = &gObj.get_component<relationship_component>();

			// only root entities will be used, the others will be traversed from the root
			if (auto parent = gObj.get_component<relationship_component>().parent)
			{
				if (parent.has_components<ui_component>())
					continue;
			}

			// traverse the entire hierarchy tree in a Depth first search fashion
			bool reachedRoot = false;
			while (!reachedRoot)
			{
				// vist node
				{
					if(auto anchor = gObj.try_get_component<anchor_component>())
					{
						if (anchor->m_custom_anchor)
						{
							glm::vec2 parentSize = { width, height };

							if (auto parentEntity = relationship->parent)
							{
								if(parentEntity.has_components<ui_component>())
								{
									parentSize = parentEntity.get_component<rect2d_component>().get_rect();
									parentSize *= glm::vec2(parentEntity.get_world_scale());
								}
							}

							anchor->m_custom_anchor(gObj, parentSize.x, parentSize.y, this);
						}
					}
				}

				// advance to the next node
				{
					if (relationship->first)
						gObj = relationship->first;// going down
					else
					{
						while (!gObj.get_component<relationship_component>().next)
						{
							if (!gObj.get_component<relationship_component>().parent)
							{
								reachedRoot = true;
								break;
							}
							gObj = gObj.get_component<relationship_component>().parent; // go up
						}
						if (reachedRoot) break;
						gObj = gObj.get_component<relationship_component>().next; // then go right
					}
					relationship = &gObj.get_component<relationship_component>();
				}
			}
		}
	}

	bool scene::ui_mouse_button_action(mouse_button key, input_state state)
	{
		if (state == input_state::released)
		{
			LOG_ENGINE(trace, "releasing mouse button");
			ui_renderer::set_selected_object({});
			return false;
		}

		glm::vec2 mousePos = input::mouse_position() / m_base_quad_size;

		struct input_data
		{
			float x, y, selected_entity_z;
			bool handled;

		} inputData{ mousePos.x, mousePos.y, -1.0f, false };

		auto view = m_registry.view<ui_component>();
		for (auto ent : view)
		{
			game_object gObj(ent, this);

			// only root entities will be used, the others will be traversed from the root
			if (auto parent = gObj.get_component<relationship_component>().parent)
			{
				if (parent.has_components<ui_component>())
					continue;
			}

			gObj.for_each_visible(&inputData, [](game_object gameObject, const transform_component& worldTransform, void* data) -> bool
			{
				/* it's possible for a parent with a ui_component to have children that do not */
				if(!gameObject.has_components<ui_component>())
					return false;

				auto size = gameObject.get_component<rect2d_component>().get_rect();
				glm::vec2 scale = glm::vec2(worldTransform.scale);
				glm::vec2 finalSize = { size.x * scale.x, size.y * scale.y };

				auto pData = static_cast<input_data*>(data);

				if (overlaps_rect_point(glm::vec2(worldTransform.translation), finalSize, glm::vec2(pData->x, pData->y)))
				{
					pData->selected_entity_z = worldTransform.translation.z;
					pData->handled = true;
					LOG_ENGINE(trace, "selecting entity with tag '%s' and id 0x%llX", gameObject.tag().c_str(), gameObject.id());
					ui_renderer::set_selected_object(gameObject);
				}

				return false;
			});
		}

		return inputData.handled;
	}

	bool scene::ui_touch_down(float x, float y)
	{
		float touchRadius = input::touch_overlap_radius() / m_base_quad_size;

		struct input_data
		{
			float x, y, selected_entity_z, touch_radius;
			bool handled = false;

		} inputData{ x, y, -1.0f, touchRadius, false };

		//LOG_ENGINE(info, "ui touch down values [x = %.3f, y = %.3f, radius = %.3f", data.x, data.y, data.touch_radius);

		auto view = m_registry.view<ui_component>();

		for (auto ent : view)
		{
			game_object gObj(ent, this);

			// only root entities will be used, the others will be traversed from the root
			if (auto parent = gObj.get_component<relationship_component>().parent)
            {
                if (parent.has_components<ui_component>())
                    continue;
            }

			gObj.for_each_visible(&inputData, [](game_object gameObject, const transform_component& worldTransform, void* data) -> bool
			{
				/* it's possible for a parent with a ui_component to have children that do not */
				if(!gameObject.has_components<ui_component>())
					return false;

                /* transparent ui component only meant to be a placeholder */
                if(gameObject.has_components<ui_box_component>())
                    return false;

				auto pData = static_cast<input_data*>(data);

				auto size = gameObject.get_component<rect2d_component>().get_rect();
				glm::vec2 finalSize = { size.x * worldTransform.scale.x, size.y * worldTransform.scale.y };

				if (overlaps_rect_circle(glm::vec2(worldTransform.translation), finalSize, glm::vec2(pData->x, pData->y), pData->touch_radius))
				{
					pData->selected_entity_z = worldTransform.translation.z;
					pData->handled = true;
					ui_renderer::set_selected_object(gameObject);
					LOG_ENGINE(trace, "Selecting entity == %s", gameObject.tag().c_str());
				}

				return false;
			});

            if(inputData.handled) break;
		}

		return inputData.handled;
	}

	bool scene::ui_touch_up(float x, float y)
	{
		ui_renderer::set_selected_object({});
		//LOG_ENGINE(trace, "deselecting entity");
		return false;
	}

	void scene::on_save_state()
	{
		on_game_save();

		auto scriptView = m_registry.view<script_component>();
		for (auto [entity, script] : scriptView.each())
			script.get_instance()->on_game_save();
	}

	void scene::on_window_resize(uint32_t width, uint32_t height)
	{
		auto cameraView = m_registry.view<camera_component>();
		for (auto [entity, camera] : cameraView.each())
		{
			camera.set_viewport_size(width, height);
			camera.update(m_registry.get<transform_component>(entity));
			if (entity == m_current_camera)
			{
				for (uint32_t i = 0; i < runtime::get_frames_in_flight_count(); i++)
					renderer::update_view_projection(camera.get_projection_view(), i);
			}
		}

		if (m_loading_scene)
		{
			m_loading_scene->on_window_resize(width, height);
			m_resized_during_loading = true;
		}
	}

	void scene::on_viewport_resize(uint32_t width, uint32_t height)
	{
		m_scene_viewport_in_pixels.width = width;
		m_scene_viewport_in_pixels.height = height;

		if(m_const_base_unit)
		{
			m_scene_viewport.x = m_scene_viewport_in_pixels.width / m_base_quad_size;
			m_scene_viewport.y = m_scene_viewport_in_pixels.height / m_base_quad_size;
		}
		else if (m_calculate_base_unit_by_height)
		{
			m_base_quad_size = std::max(m_scene_viewport_in_pixels.height / m_quads_per_dimension, m_base_quad_min_size);
			m_scene_viewport.x = m_scene_viewport_in_pixels.width / m_base_quad_size;
			m_scene_viewport.y = m_quads_per_dimension;
		}
		else
		{
			m_base_quad_size = std::max(m_scene_viewport_in_pixels.width / m_quads_per_dimension, m_base_quad_min_size);
			m_scene_viewport.x = m_quads_per_dimension;
			m_scene_viewport.y = m_scene_viewport_in_pixels.height / m_base_quad_size;
		}

		ui_viewport_resize(m_scene_viewport.x, m_scene_viewport.y);

		auto scriptView = m_registry.view<script_component>();
		for (auto [entity, script] : scriptView.each())
			script.get_instance()->on_viewport_resize(m_scene_viewport.x, m_scene_viewport.y);
	}

	void scene::on_key_action(key_code key, input_state state)
	{
		if (is_playing())
		{
			auto scriptView = m_registry.view<script_component>();
			for (auto [entity, script] : scriptView.each())
			{
				if (script.is_active())
				{
					if (script.get_instance()->on_key_action(key, state))
						break;
				}
			}
		}
	}

	void scene::on_touch_down(float x, float y)
	{
		glm::vec2 localPos = { x / m_base_quad_size, y / m_base_quad_size };

		if (ui_touch_down(localPos.x, localPos.y))
			return;

		if (is_playing())
		{
			auto scriptView = m_registry.view<script_component>();
			for (auto [entity, script] : scriptView.each())
			{
				if (script.is_active())
				{
					if (script.get_instance()->on_touch_down(localPos.x, localPos.y))
						break;
				}
			}
		}
	}

	void scene::on_touch_up(float x, float y)
	{
		glm::vec2 localPos = { x / m_base_quad_size, y / m_base_quad_size };

		if (ui_touch_up(localPos.x, localPos.y))
			return;

		if (is_playing())
		{
			auto scriptView = m_registry.view<script_component>();
			for (auto [entity, script] : scriptView.each())
			{
				if (script.is_active())
				{
					if (script.get_instance()->on_touch_up(localPos.x, localPos.y))
						break;
				}
			}
		}
	}

	void scene::on_touch_move(float x, float y)
	{
		glm::vec2 localPos = { x / m_base_quad_size, y / m_base_quad_size };

		if (is_playing())
		{
			auto scriptView = m_registry.view<script_component>();
			for (auto [entity, script] : scriptView.each())
			{
				if (script.is_active())
				{
					if (script.get_instance()->on_touch_move(localPos.x, localPos.y))
						break;
				}
			}
		}
	}

	void scene::on_pinch_scale(const float scale)
	{
		if (is_playing())
		{
			auto scriptView = m_registry.view<script_component>();
			for (auto [entity, script] : scriptView.each())
			{
				if (script.is_active())
				{
					if (script.get_instance()->on_pinch_scale(scale))
						break;
				}
			}			
		}		
	}

	void scene::on_mouse_button_action(mouse_button key, input_state state)
	{
		if (ui_mouse_button_action(key, state))
			return;

		if (is_playing())
		{
			auto scriptView = m_registry.view<script_component>();
			for (auto [entity, script] : scriptView.each())
			{
				if (script.is_active())
				{
					if (script.get_instance()->on_mouse_button_action(key, state))
						break;
				}
			}
		}
	}

	void scene::on_mouse_moved(const float x, const float y)
	{
		glm::vec2 localPos = { x / m_base_quad_size, y / m_base_quad_size };

		if (is_playing())
		{
			auto scriptView = m_registry.view<script_component>();
			for (auto [entity, script] : scriptView.each())
			{
				if (script.is_active())
				{
					if (script.get_instance()->on_mouse_moved(localPos.x, localPos.y))
						break;
				}
			}
		}
	}

	void scene::on_mouse_scrolled(const float delta)
	{
		if (is_playing())
		{
			auto scriptView = m_registry.view<script_component>();
			for (auto [entity, script] : scriptView.each())
			{
				if (script.is_active())
				{
					if (script.get_instance()->on_mouse_scrolled(delta))
						break;
				}
			}
		}
	}

	std::shared_ptr<audio_mixer> scene::get_audio_mixer(const std::string& mixerName)
	{
		const auto it = m_audio_mixers.find(mixerName);
		if(it != m_audio_mixers.end())
		{
			LOG_ENGINE(trace, "mixer with name '%s' found", mixerName.c_str());
			return it->second;
		}

		LOG_ENGINE(warn, "mixer with name '%s' not found", mixerName.c_str());
		return nullptr;
	}

	std::shared_ptr<audio_mixer> scene::add_audio_mixer(const std::string& mixerName)
	{
		const auto it = m_audio_mixers.find(mixerName);
		if(it != m_audio_mixers.end())
		{
			LOG_ENGINE(info, "mixer with name '%s' already exists, no new mixer was created", mixerName.c_str());
			return it->second;
		}

		auto mixer = std::make_shared<audio_mixer>(mixerName);
		m_audio_mixers[mixerName] = mixer;

		return mixer;
	}

	/*-------------------------------------------------------------------------------*/
	void scene::set_custom_engine_texture()
	{
		if (auto& texSprite = m_engine_textures.renderer_white)
			renderer::override_white_texture(texSprite.tex, texSprite.uv, texSprite.stride);

		if (auto& texSprite = m_engine_textures.ui_white)
			ui_renderer::override_white_texture(texSprite.tex, texSprite.uv, texSprite.stride);
	}

	void scene::override_renderer_white_texture(const std::string& path, float u, float v, float strideX, float strideY)
	{
		assert(!m_is_active);

		if (auto userTexture = texture::create(path))
			m_engine_textures.renderer_white = sprite(userTexture, glm::vec2(u, v), glm::vec2(strideX, strideY));
	}

	void scene::override_ui_white_texture(const std::string& path, float u, float v, float strideX, float strideY)
	{
		assert(!m_is_active);

		if (auto userTexture = texture::create(path))
			m_engine_textures.ui_white = sprite(userTexture, glm::vec2(u, v), glm::vec2(strideX, strideY));
	}

}
