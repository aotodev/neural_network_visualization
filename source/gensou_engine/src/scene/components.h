#pragma once

#include "core/core.h"
#include "core/input_codes.h"
#include "core/log.h"
#include "core/uuid.h"
#include "core/input.h"
#include "core/misc.h"

#include "renderer/texture.h"
#include "renderer/geometry/lines.h"

#include "scene/game_object.h"
#include "scene/sprite.h"


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

PUSH_IGNORE_WARNING
#include <box2d/b2_world.h>
#include <box2d/b2_body.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/b2_fixture.h>
#include <renderer/texture.h>
POP_IGNORE_WARNING


namespace gs {

    struct id_component
    {
        uuid id;
    };

    struct tag_component
    {
        std::string tag;

		tag_component() = default;
		tag_component(const std::string& inTag) : tag(inTag) {}
    };

    // for entity hierarchy tree
    struct relationship_component
    {
        size_t children_count = 0;

        game_object first; // the entity identifier of the first child, if any
        game_object last;  // the entity identifier of the last child, if any
		
        game_object parent;
        game_object previous; // the previous sibling in the list of children for the parent
        game_object next; //the next sibling in the list of children for the parent
    };


    struct state_component
    {
		/* active entities will be updated and receive input events */
        bool is_active = true;

		/* depending on the components, visible entities will be rendered */
        bool is_visible = true;
    };

    struct transform_component
    {
        glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

		transform_component() = default;
		transform_component(const transform_component& other) = default;
		transform_component(const glm::vec3& inTranslation) : translation(inTranslation) {}

        glm::mat4 get_transform() const { return glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(glm::quat(rotation)) * glm::scale(glm::mat4(1.0f), scale); }

        static glm::vec3 get_translation_from_mat4(const glm::mat4& transform) { return glm::vec3(transform[3].x, transform[3].y, transform[3].z); }
    };

    class sprite_component
    {
		friend class scene;

	public:
		struct animation
		{
			animation(float framesPerSecond, std::initializer_list<glm::vec4> framesList)
				: frames_uv_and_stride(framesList), frame_count(framesList.size()), frames_per_second(framesPerSecond),
				  change_frame(1.0 / (double)frames_per_second), epsilon(change_frame * 0.1)
			{}

			/* coords == x, y | stride == z, w */
			std::vector<glm::vec4> frames_uv_and_stride;

			const uint32_t frame_count;
			const float frames_per_second;

			bool loop = true;
			bool is_active = false;

			/* used for animation */
			uint32_t current_frame = 0;
			double accumulator = 0.0;

			const double change_frame;
			const double epsilon;

			bool has_just_started = true;

			uint32_t next_frame() { return current_frame = (current_frame + 1) % frame_count; }

			void reset()
			{
				current_frame = 0;
				accumulator = 0.0;
				has_just_started = true;
			}
		};

    public:
		sprite_component() = default;
		sprite_component(const std::string& path, bool mips = false, bool flip = false, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f);
		sprite_component(const std::string& path, float u, float v, float strideX, float strideY);
		sprite_component(const std::string& path, const glm::vec4& coords, bool mips = false, bool flip = false);
		sprite_component(std::shared_ptr<texture> texture, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f);

		sprite_component(const std::string& path, bool mips, bool flip, glm::vec2 uv, glm::vec2 stride, sampler_info samplerInfo);

		/* inheritance just for ui_sprite_component
		 * sprite_component will never be used polymorphically, so no need for a virtual distructor
		 */

        void set_texture(const std::string& path, bool mips = false, bool flip = false, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f);
        void set_texture(std::shared_ptr<texture> inTexture, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f);

        void set_texture_ex(const std::string& path, bool mips, bool flip, float u, float v, float strideX, float strideY, sampler_info samplerInfo);

        void set_texture_coords(float u, float v)
		{ 
			m_coords.x = u; m_coords.y = v;
		}
        void set_texture_coords(glm::vec2 coords)
		{ 
			m_coords = coords;
		}

        void set_texture_coords(float u, float v, float strideX, float strideY, bool keepScale = true)
        {
            m_coords.x = u;
            m_coords.y = v;
            m_stride.x = strideX;
            m_stride.y = strideY;

			calculate_size(keepScale);
        }

        void set_texture_coords(glm::vec2 coords, glm::vec2 stride, bool keepScale = true)
        {
            m_coords = coords;
            m_stride = stride;

			calculate_size(keepScale);
        }

        void set_stride(float x, float y, bool keepScale = true)
		{ 
			m_stride.x = x; m_stride.y = y;
			calculate_size(keepScale);
		}

        void set_stride(glm::vec2 stride, bool keepScale = true)
		{
			m_stride = stride;
			calculate_size(keepScale);
		}

        std::shared_ptr<texture> get_texture() const { return m_texture; }

        const glm::vec2& get_coords() const { return m_coords; }
        const glm::vec2& get_stride() const { return m_stride; }

		glm::vec2 get_coords() { return m_coords; }
		glm::vec2 get_stride() { return m_stride; }

		glm::vec2 get_size() const { return m_size; }

        void add_animation(const std::string& animationName, float framesPerSecond, std::initializer_list<glm::vec4> framesList)
        {
			LOG_ENGINE(info, "adding animation with %.3f frames per second", framesPerSecond);
			m_animations.emplace(std::make_pair(animationName, animation(framesPerSecond, framesList)));
        }

        void set_animation(const std::string& animationName, bool startActive = true)
        {
            const auto mapIterator = m_animations.find(animationName);
			if (mapIterator != m_animations.end())
			{
				m_current_animation = &mapIterator->second;
            }
			else
            {
				m_current_animation = nullptr;
                LOG_ENGINE(error, "Attempting to set an animation that does not exist");
                return;
            }
			m_current_animation->is_active = startActive;

            if (!startActive)
            {
				const auto& uv_and_stride = m_current_animation->frames_uv_and_stride[0];
                m_coords = glm::vec2(uv_and_stride.x, uv_and_stride.y);
                m_stride = glm::vec2(uv_and_stride.z, uv_and_stride.w);
				m_current_animation->current_frame = 1;
            }
        }

		animation& get_animation(const std::string& animationName)
        {
            return m_animations.at(animationName);
        }

        animation* get_current_animation()
        {
            return m_current_animation;
        }

        void set_animation_active(bool isActive)
        { 
			m_current_animation->is_active = isActive;
        }

        bool is_animation_active() const
        {
            if (m_current_animation)
                return m_current_animation->is_active;
            
            return false;
        }

		void set_loop_animation(bool loop)
		{
			m_current_animation->loop = loop;
		}

		void set_size(float width, float height)
		{
			m_size = { width, height };
		}

		void set_size(const glm::vec2 size)
		{
			m_size = size;
		}

		void scale_size(float scale)
		{
			m_size *= scale;
		}

		void scale_size_by_height(float height)
		{
			float ratio = m_size.x / m_size.y;
			m_size.y = height;
			m_size.x = height * ratio;
		}

		void scale_size_by_width(float width)
		{
			float ratio = m_size.x / m_size.y;
			m_size.x = width;
			m_size.y = width / ratio;
		}

		void set_width(float width) { m_size.x = width; }
		void set_height(float height) { m_size.y = height; }

		void set_hidden(bool hide) { m_hidden = hide; }
		bool is_hidden() const { return m_hidden; }

	public:
		float squash_constant = 1.0f;
		bool mirror_texture = false;
		bool animate_when_inactive = false;
		glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };

    private:
		void calculate_size(bool keepScale);
		void animate(float dt);

        std::shared_ptr<texture> m_texture;
        glm::vec2 m_coords, m_stride;

        std::unordered_map<std::string, animation> m_animations;
		animation* m_current_animation = nullptr;

		glm::vec2 m_size{ 1.0f, 1.0f };

		bool m_hidden = false;
    };

    class camera_component
    {
    public:
        void set_viewport_size(uint32_t width, uint32_t height);
        void update(transform_component& transform);

        projection get_projection_type() const { return m_projection_type; }
        void set_projection_type(projection projectionType);

        /* change FOV for perspective | change Orthographic size for orthographic */
        void zoom(float dy);

        const glm::mat4& get_projection() const { return m_projection; }
        const glm::mat4& get_view() const { return m_view; }
		glm::mat4 get_projection_view() const { return m_projection * m_view; }

        static constexpr glm::vec3 up_vector() noexcept { return glm::vec3(0.0f, 1.0f, 0.0f); }
        static constexpr glm::vec3 forward_vector() noexcept { return glm::vec3(0.0f, 0.0f, -1.0f); }
        static constexpr glm::vec3 right_vector() noexcept { return glm::vec3(1.0f, 0.0f, 0.0f); }

        /* size is the viewport height */
        void set_orthographic(float size, float nearClip = -10.0f, float farClip = 10.0f);
        void set_perspective(float fov, float nearClip = 0.1f, float farClip = 1000.0f);

        void update_projection() { set_viewport_size(m_rect2d.s, m_rect2d.t); }

        glm::vec2 get_viewport_size() const { return m_rect2d; }

        float get_orthographic_size() const noexcept { return m_orthographic_size; }
        float get_orthographic_near() const noexcept { return m_orthographic_near; }
        float get_orthographic_far()	const noexcept { return m_orthographic_far; }

        float get_perspective_fov() const noexcept { return m_perspective_fov; }
        float get_perspective_near() const noexcept { return m_perspective_near; }
        float get_perspective_far()	const noexcept { return m_perspective_far; }

        void set_orthographic_size(float size) { m_orthographic_size = size; }
        void set_orthographic_near(float near_) { m_orthographic_near = near_; }
        void set_orthographic_far(float far_) { m_orthographic_far = far_; }

        void set_perspective_fov(float fov) { m_perspective_fov = fov; }
        void set_perspective_near(float near_) { m_perspective_near = near_; }
        void set_perspective_far(float far_) { m_perspective_far = far_; }

		void set_look_at(float x, float y, float z)
		{
			m_look_at = glm::vec3(x, y, z);
		}

    private:
        projection m_projection_type = projection::orthographic;

        glm::mat4 m_projection, m_view;

        float m_orthographic_size = 720.0f, m_orthographic_near = -1.0f, m_orthographic_far = 1.0f, m_orthographic_zoom = 1.0f;
        float m_perspective_fov = glm::radians(30.0f), m_perspective_near = 0.1f, m_perspective_far = 1000.0f;
        float m_zoom_speed = 0.016f;

        glm::vec2 m_rect2d;
		glm::vec3 m_look_at{ 0.0f, 0.0f, 0.f};

        /* states */
        input_state m_move_right = input_state::released;
        input_state m_move_left = input_state::released;
        input_state m_move_up = input_state::released;
        input_state m_move_down = input_state::released;
        input_state m_move_forward = input_state::released;
        input_state m_move_backward = input_state::released;

        friend class renderer;
        friend class scene;
    };

	enum rigidbody2d_type
	{
		static_body = b2_staticBody,
		kinematic_body = b2_kinematicBody,
		dynamic_body = b2_dynamicBody
	};

    struct rigidbody2d_component
    {
		rigidbody2d_component(void* pointerToClass) : m_data_pointer(pointerToClass) {}

		rigidbody2d_type body_type = static_body;

        glm::vec2 get_velocity() const { return glm::vec2{ body->GetLinearVelocity().x, body->GetLinearVelocity().y }; }
        void set_velocity(float x, float y)
        { 
			linear_velocity = glm::vec2(x, y);
			if(body)
				body->SetLinearVelocity({ x, y });
        }

        void set_velocity_x(float x)
        {
            linear_velocity.x = x;
			if(body)
				body->SetLinearVelocity({ x, body->GetLinearVelocity().y });
        }
        void set_velocity_y(float y)
        { 
			linear_velocity.y = y;
			if(body)
				body->SetLinearVelocity({ body->GetLinearVelocity().x, y });
        }
        void set_position(float x, float y, float angle = 0.0f)
		{
			if(body)
				body->SetTransform({ x, y }, angle);
		}

        void apply_force(float x, float y)
        {
			if (body)
			{
				body->ApplyForceToCenter({ x, y }, true);
				body->ApplyLinearImpulseToCenter({ x, y }, true);
			}
        }

        float gravity_scale = 1.0f;
        bool fixed_rotation = true;

        bool recreate = false;

        void* get_data_pointer() { return m_data_pointer; }

    private:
        b2Body* body = nullptr;
        void* m_data_pointer = nullptr;
        glm::vec2 linear_velocity{ 0.0f, 0.0f };

        friend class scene;
    };

    struct box_collider2d_component
    {
    public:
		box_collider2d_component() = default;
		box_collider2d_component(const box_collider2d_component&) = default;

        float x_half_extent = 1.0f;
        float y_half_extent = 1.0f;
        glm::vec2 center{ 0.0f, 0.0f };

        float density = 1.0f, friction = 0.5f, restitution = 0.0f, restitution_threashold = 0.5f;
    };

	struct cube_component
	{
		cube_component() = default;
		cube_component(const glm::vec4& inColor) : color(inColor) {}
		
		glm::vec4 color{1.0f};
	};

	struct line_segment
	{
		line_vertex p1, p2;
	};

	struct line_renderer_component
	{
		std::vector<line_segment> lines;
		int32_t start = 0, end = -1;
		glm::vec2 edge_range{0.0f, 1.0f};
		bool size_in_pixels = true;
	};

    ///////////////////////////////////////////////////////////////////////////
    // UI COMPONENTS
    ///////////////////////////////////////////////////////////////////////////
	
	enum class anchor : uint32_t
	{
		horizontal_center = 1,
		left = 2,
		right = 4,
		vertical_center = 8,
		top = 16,
		bottom = 32,
		horizontal_stretch = 64,
		vertical_stretch = 128,

		//=====================================================//

		center = vertical_center | horizontal_center,
		center_right = vertical_center | right,
		center_left = vertical_center | left,

		top_center = top | horizontal_center,
		top_right = top | right,
		top_left = top | left,

		bottom_center = horizontal_center | bottom,
		bottom_right = bottom | right,
		bottom_left = bottom | left,

		horizontal_stretch_center = horizontal_stretch | vertical_stretch,
		horizontal_stretch_top = horizontal_stretch | top,
		horizontal_stretch_bottom = horizontal_stretch | bottom,

		vertical_stretch_center = vertical_stretch | horizontal_stretch,
		vertical_stretch_right = vertical_stretch | right,
		vertical_stretch_left = vertical_stretch | left,

		fit_parent = horizontal_stretch | vertical_stretch
	};

	class anchor_component
	{
		friend class scene;
		friend class ui_renderer;
		friend class game_object;

		typedef void (*dynamic_layout_fn)(game_object, float, float, class scene*);

	public:

		void set(anchor inAnchor) { m_anchor = inAnchor; }
		/* delegate will be called every time the viewport size changes */
		void add_custom_anchor(dynamic_layout_fn customAction) { m_custom_anchor = customAction; }

		void clear_anchor() { m_anchor = anchor::center; m_center = glm::vec2(0.0f, 0.0f); }
		void clear_custom_anchor() { m_custom_anchor = nullptr; }

		anchor get() const { return m_anchor; }

		/* returns local center position (relative to parent if any) */
		glm::vec2 get_center();
		glm::vec2 get_center(const glm::vec3& parentWorldScale);

		void* user_data = nullptr;

	private:
		game_object m_game_object;

		anchor m_anchor = anchor::center;
		dynamic_layout_fn m_custom_anchor = nullptr;
		glm::vec2 m_center = glm::vec2(0.0);
	};


	/*--------------------------------------------------------------------------------------*/
	/* empty component to identiy, find/access and iterate all ui components */
	struct ui_component {};

	enum class ui_texture_mode : uint32_t {  fit_height = 0, fit_width, fit_both };

	/* struct to set the shape of the ui component */
	struct rect2d_component
	{
		glm::vec2 get_rect() const { return glm::vec2(width, height); }

		float get_aspect_ratio() const { return width / height; }

		rect2d_component operator*(float constant) const { return rect2d_component{ width * constant, height * constant }; }
		rect2d_component operator*=(float constant)
		{
			width *= constant;
			height *= constant;

			return rect2d_component{ width, height };
		}


		/* w:h, x:y */
		float width = 1.0f;

		/* for setting base geometry without affecting the transform (thus also not affecting its children's transform) */
		float height = 1.0f;
	};

	/* not a component, to be inherited by all ui components */
	struct base_ui
	{
		friend class game_object;

		game_object get_game_object() const { return m_game_object; }

		/* sets basic quad geometry that will be passed as vertices to the vertex shader */
		void set_rect(float width, float height)
		{
			auto& rect = m_game_object.get_component<rect2d_component>();
			rect.width = width;
			rect.height = height;
		}

		/* sets basic quad geometry that will be passed as vertices to the vertex shader */
		void set_rect(const glm::vec2& newRect)
		{
			auto& rect = m_game_object.get_component<rect2d_component>();
			rect.width = newRect.x;
			rect.height = newRect.y;
		}

		void set_rect_width(float width) { m_game_object.get_component<rect2d_component>().width = width; }
		void set_rect_height(float height) { m_game_object.get_component<rect2d_component>().height = height; }

		void scale_rect(float scale)
		{
			auto& rect = m_game_object.get_component<rect2d_component>();

			rect *= scale;
		}

		void scale_rect_by_height(float height)
		{
			auto& rect = m_game_object.get_component<rect2d_component>();
			float ratio = rect.width / rect.height;
			rect.height = height;
			rect.width = height * ratio;
		}

		void scale_rect_by_width(float width)
		{
			auto& rect = m_game_object.get_component<rect2d_component>();
			float ratio = rect.width / rect.height;
			rect.width = width;
			rect.height = width / ratio;
		}

		glm::vec2 get_rect_size()
		{
			return m_game_object.get_component<rect2d_component>().get_rect();
		}
		
		float get_aspect_ratio() const
		{
			return m_game_object.get_component<rect2d_component>().get_aspect_ratio();
		}


	protected:
		game_object m_game_object;
	};

	/* as an invisible container for child ui components */
	class ui_box_component : public base_ui
	{
	public:
		ui_box_component() = default;
	};

	class button_component : public base_ui
	{
		friend class ui_renderer;
		friend class scene;

		typedef void (*button_action_fn)(class button_component*, class scene*, void*);

	public:
		enum state : uint32_t { none = 0, hovered = 1, pressed = 2 };

		void set_texture(std::shared_ptr<texture> texture, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f, bool overrideCurrentSize = false);
		void set_texture(const std::string& path, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f, bool overrideCurrentSize = false);

		void set_texture_coords(float u, float v)
		{
			m_texture_uv.x = u; m_texture_uv.y = v;
		}

		void set_stride(float x, float y);

		void set_texture_coords(float u, float v, float strideX, float strideY)
		{
			m_texture_uv.x = u;
			m_texture_uv.y = v;

			if(m_texture_uv_stride.x != strideX && m_texture_uv_stride.y != strideY)
				set_stride(strideX, strideY);
		}

		void set_texture_mode(ui_texture_mode mode) { m_texture_mode = mode; }
		ui_texture_mode get_texture_mode() const { return m_texture_mode; } 

		std::shared_ptr<texture> get_texture() const { return m_texture; }
		glm::vec2 get_texture_coords() const { return m_texture_uv; }
		glm::vec2 get_texture_stride() const { return m_texture_uv_stride; }

		glm::vec2 get_texture_size() const
		{
			if(!m_texture)
				return glm::vec2(0.0f);
				
			return { m_texture->get_width() * m_texture_uv_stride.x, m_texture->get_height() * m_texture_uv_stride.y };
		}

		float get_texture_aspect_ratio() const
		{
			auto size = get_texture_size();
			return size.x / size.y;
		}

		void set_rect_to_texture();

		glm::vec4 default_color = glm::vec4(1.0f);
		glm::vec4 hovered_color = glm::vec4(1.0f);
		glm::vec4 pressed_color = glm::vec4(1.0f);

		/* sets default, hoverd and pressed colors to this same value */
		void set_consistent_color(const glm::vec4& color)
		{
			default_color = color;
			hovered_color = color;
			pressed_color = color;
		}

		/* disabled color will be multiplied by the default color, be it texture, label, background, etc */
		glm::vec4 disabled_color = glm::vec4(0.2f, 0.2f, 0.2f, 1.0);

		glm::vec4 default_background_color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
		glm::vec4 hovered_background_color = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);
		glm::vec4 pressed_background_color = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);

		void set_consistent_background_color(const glm::vec4& color)
		{
			default_background_color = color;
			hovered_background_color = color;
			pressed_background_color = color;
		}

		glm::vec4 border_color = { 0.0f, 0.0f, 0.0f, 0.0f };
		float border_thickness = 0.0f;

		float corner_radius = 0.0f;
		float texture_scale = 1.0f;

		std::string label = "button";
		std::string label_font = "default";

		/* data to be passed to the action delegates */
		void* user_data = nullptr;
		button_action_fn on_pressed_action = nullptr;
		button_action_fn on_released_action = nullptr;
		button_action_fn on_hover_started_action = nullptr;
		button_action_fn on_hover_ended_action = nullptr;

	private:
		std::shared_ptr<texture> m_texture;
		glm::vec2 m_texture_uv = glm::vec2(0.0f, 0.0f);
		glm::vec2 m_texture_uv_stride = glm::vec2(1.0f, 1.0f);

		state m_state = state::none;
		ui_texture_mode m_texture_mode = ui_texture_mode::fit_both;
	};

	class text_component : public base_ui
	{
	public:
		std::string text;
		float font_size = 0.12f;

		/* 0 means no limit */
		float line_width = 0.0f;
		std::string font = "default";

		glm::vec4 color{ 1.0, 1.0f, 1.0f, 1.0f };

		bool center_text = false;
		/* font size will be relative to scene size */
		bool text_size_dynamic = true;
	};

	class slider_component : public base_ui
	{
		friend class scene;
		friend class ui_renderer;

		typedef void (*slider_action_fn)(class slider_component*, class scene*, float, void*);

	public:
		void set_handle_texture(std::shared_ptr<texture> texture) { m_handle_texture = texture; }
		void set_handle_texture(std::shared_ptr<texture> texture, float u, float v, float strideX, float strideY);
		void set_handle_texture(const std::string& path, float u, float v, float strideX, float strideY);

		void set_handle_texture_coords(float u, float v) { m_handle_texture_uv = glm::vec2(u, v); }
		void set_handle_texture_stride(float strideX, float strideY) { m_handle_texture_uv_stride = glm::vec2(strideX, strideY); }

	public:
		glm::vec4 background_color{ 0.2f, 0.2f, 0.2f, 1.0f };
		glm::vec4 border_color{ 0.1f, 0.2f, 0.8f, 1.0f };
		glm::vec4 fill_color{ 0.0f, 0.2f, 0.7f, 1.0f };
		glm::vec4 disabled_color{ 0.5f, 0.5f, 0.5f, 1.0f };
		glm::vec4 handle_color{ 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec2 range{ 0.0f, 1.0f };

		float border_thickness = 1.0f;

		float value = 0.0f;
		slider_action_fn on_value_changed_action = nullptr;
		slider_action_fn on_release_action = nullptr;
		void* user_data = nullptr;

	private:
		std::shared_ptr<texture> m_handle_texture;
		glm::vec2 m_handle_texture_uv{ 0.0f, 0.0f };
		glm::vec2 m_handle_texture_uv_stride{ 1.0f, 1.0f };

		bool m_is_pressed = false;

	};

	class image_component : public base_ui
	{
		friend class scene;

	public:
		std::shared_ptr<texture> get_texture() { return  m_texture; }
		glm::vec2 get_texture_coords() const { return m_texture_uv; }
		glm::vec2 get_texture_stride() const { return m_texture_uv_stride; }

		void set_texture(std::shared_ptr<texture> texture, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f, bool overrideCurrentSize = false);
		void set_texture(const std::string& path, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f, bool overrideCurrentSize = false);

		void set_rect_to_texture();

	public:
		glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float round_corners_radius = 0.0f;
		bool blur_texture = false;

	private:
		std::shared_ptr<texture> m_texture;
		glm::vec2 m_texture_uv = glm::vec2(0.0f, 0.0f);
		glm::vec2 m_texture_uv_stride = glm::vec2(1.0f, 1.0f);

		float m_aspect_ratio = 1.0f;
	};

	class toggle_switch_component : public base_ui
	{
		friend class scene;
		friend class ui_renderer;

		typedef void (*toggle_action_fn)(class toggle_switch_component*, class scene*, bool, void*);

	public:
		bool toggle() { return m_is_on = !m_is_on; }

		void set_on() { m_is_on = true; }
		void set_off() { m_is_on = false; }

		void set_handle_texture(std::shared_ptr<texture> texture) { m_handle_texture = texture; }
		void set_handle_texture(std::shared_ptr<texture> texture, float u, float v, float strideX, float strideY);
		void set_handle_texture(const std::string& path, float u, float v, float strideX, float strideY);

		void set_handle_texture_coords(float u, float v) { m_handle_texture_uv = glm::vec2(u, v); }
		void set_handle_texture_stride(float strideX, float strideY) { m_handle_texture_uv_stride = glm::vec2(strideX, strideY); }

		glm::vec4 border_color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float border_thickness = 0.0f;

		glm::vec4 on_color{ normalized_color<91, 194, 54> };
		glm::vec4 off_color{ 0.2f, 0.2f, 0.2f, 1.0f };
		glm::vec4 disabled_color{ 0.5f, 0.5f, 0.5f, 1.0f };

		glm::vec4 handle_on_color{ 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec4 handle_off_color{ 0.4f, 0.4f, 0.4f, 1.0f };

		float handle_scale = 0.88f;

		toggle_action_fn on_toggle_action = nullptr;
		void* user_data = nullptr;

	private:
		std::shared_ptr<texture> m_handle_texture;
		glm::vec2 m_handle_texture_uv{ 0.0f, 0.0f };
		glm::vec2 m_handle_texture_uv_stride{ 1.0f, 1.0f };

		bool m_is_on = true;
		bool m_is_pressed = false;
	};

	/* health/progress bar component */
	class bar_component : public base_ui
	{
		friend class scene;
		friend class ui_renderer;

	public:
		void set_vertical() { m_orientation = vertical; }
		void set_horizontal() { m_orientation = horizontal; }

		glm::vec4 fill_color{ 0.0f, 0.2f, 0.7f, 1.0f };
		glm::vec4 background_color{ 0.0f, 0.2f, 0.7f, 1.0f };
		glm::vec4 border_color{ 0.0f, 0.2f, 0.7f, 1.0f };
		glm::vec2 range{ 0.0f, 1.0f };

		float value = 0.0f;
		float border_thickness = 1.0f;

	private:
		enum orientation { horizontal = 0, vertical } m_orientation = horizontal;
		
	};

	class dialog_box_component : protected base_ui /* the base_ui methods are not relevant for this component */
	{
		friend class scene;
		friend class ui_renderer;
		friend class game_object;

	public:
		using base_ui::get_game_object;
		
		typedef void (*finished_dialog_action_fn)(dialog_box_component*, class scene*, void*);
		typedef void (*finished_dialog_line_action_fn)(dialog_box_component*, uint32_t, class scene*, void*);

		std::vector<std::string> dialogs_list;

		float font_size = 1.0f;
		float line_width = 0.0f;
		float text_speed = 40.0f;
		uint32_t max_lines = 2;

		glm::vec4 text_color = { 0.0f, 0.0f, 0.0f, 1.0f };
		glm::vec4 box_color = { 0.4f, 0.4f, 0.4f, 0.4f };
		glm::vec4 box_border_color = { 0.0f, 0.0f, 0.0f, 0.0f };

		float round_corners_radius = 0.0f;
		float border_thickness = 0.0f;

		std::string font = "default";

		bool blur_box = false;

		void open(bool rewind = true)
		{
			if(rewind)
				rewind_dialogs();

			m_updating = true;
			m_open = true;
		}

		void close()
		{
			m_updating = false;
			m_open = false;
			m_finished = false;
		}

		/* returns the index of the next dialog */
		uint32_t next_dialog();

		void rewind_dialogs()
		{
			m_current_dialog_index = 0;
			m_current_char_count = 1;
			m_finished = false;
			m_finished_line = false;
		}

		uint32_t get_current_dialog_index() const { return m_current_dialog_index; }

		bool finished_all_lines() const { return m_finished; }
		bool finished_line() const { return m_finished_line; }
		bool is_open() const { return m_open; }
		bool is_updating() const { return m_updating; }

		void set_custom_rect(const glm::vec2& newRect)
		{
			auto& rect = m_game_object.get_component<rect2d_component>();
			rect.width = newRect.x;
			rect.height = newRect.y;

			m_custom_rect = true;
		}

		void clear_custom_rect()
		{
			m_custom_rect = false;
		}

		finished_dialog_line_action_fn finished_line_action = nullptr;
		void* user_data = nullptr;

	private:
		void update(float dt);

		uint32_t m_current_dialog_index = 0, m_current_char_count = 1;
		float m_counter = 0.0f;
		bool m_updating = false, m_open = false;
		bool m_finished = false, m_finished_line = false;

		bool m_custom_rect = false;
	};

	/* same as sprite component but rendered and processed as UI */
	class ui_sprite_component : protected base_ui, public sprite_component
	{
		friend class scene;
		friend class ui_renderer;
		friend class game_object;

	public:
		ui_sprite_component() = default;

		ui_sprite_component(const std::string& path, bool mips = false, bool flip = false, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f)
			: sprite_component(path, mips, flip, u, v, strideX, strideY) {}

		ui_sprite_component(const std::string& path, float u, float v, float strideX, float strideY)
			: sprite_component(path, u, v, strideX, strideY) {}

		ui_sprite_component(const std::string& path, const glm::vec4& coords, bool mips = false, bool flip = false)
			: sprite_component(path, coords, mips, flip) {}

		ui_sprite_component(std::shared_ptr<texture> texture, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f)
			: sprite_component(texture, u, v, strideX, strideY) {}

		ui_sprite_component(const std::string& path, bool mips, bool flip, glm::vec2 uv, glm::vec2 stride, sampler_info samplerInfo)
			: sprite_component(path, mips, flip, uv, stride, samplerInfo) {}

		using base_ui::get_game_object;
	};

}