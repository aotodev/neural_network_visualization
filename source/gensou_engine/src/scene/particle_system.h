#pragma once

#include "core/core.h"

#include <glm/glm.hpp>

namespace gs {

	template<typename System>
	class particle_system_iterator
	{
	public:
		using value_type = typename System::particle;
		using ptr_type = value_type*;
		using ref_type = value_type&;
		using const_ptr_type = const value_type*;
		using const_ref_type = const value_type&;

		particle_system_iterator(System* ptr, uint32_t index)
			: m_system(ptr), m_index(index) {}

		ptr_type operator->() { return &(m_system->m_particles[m_index]); }
		const ptr_type operator->() const { return &(m_system->m_particles[m_index]); }

		ref_type operator*() { return m_system->m_particles[m_index]; }
		const_ref_type operator*() const { return m_system->m_particles[m_index]; }

		particle_system_iterator& operator++()
		{
			m_index = (m_index + 1) % m_system->get_max_particles();
			return *this;
		}

		particle_system_iterator operator++(int)
		{
			auto temp = *this;
			
			m_index = (m_index + 1) % m_system->get_max_particles();
			return temp;
		}

		particle_system_iterator& operator--()
		{
			auto size = m_system->get_max_particles();
			m_index = (m_index + size - 1) % size;
			return *this;
		}

		particle_system_iterator operator--(int)
		{
			auto temp = *this;

			auto size = m_system->get_max_particles();
			m_index = (m_index + size - 1) % size;

			return temp;
		}

		bool operator==(const particle_system_iterator<System>& other) const { return m_index == other.m_index; }
		bool operator!=(const particle_system_iterator<System>& other) const { return m_index != other.m_index; }

	private:
		System* m_system;
		uint32_t m_index;
	};

	class particle_system
	{
		friend class scene;
		friend class particle_system_iterator<particle_system>;

	public:
		using iterator = particle_system_iterator<particle_system>;

		particle_system(uint32_t maxParticles = 512, bool startAlive = true);

		void set_emit_interval(float interval) { m_emit_interval = interval; }
		void set_particles_emited_per_interval(uint32_t count) { m_particles_emited_per_interval = count; }

		void set_texture(const std::string& path, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f);
		void set_texture(std::shared_ptr<class texture>, float u = 0.0f, float v = 0.0f, float strideX = 1.0f, float strideY = 1.0f);

		void set_emit_random_texture_parts(bool set) { m_sample_different_coords = set; }

		void set_life_time(float time) { m_life_time = time; }
		void set_base_size(float x, float y) { m_base_size = { x, y }; }

		void set_velocity_range(float minX, float minY, float maxX, float maxY)
		{
			m_min_velocity = { minX, minY };
			m_max_velocity = { maxX, maxY };
		}

		void set_min_velocity(float x, float y) { m_min_velocity = { x, y }; }
		void set_max_velocity(float x, float y) { m_max_velocity = { x, y }; }

		void set_rotation_range(float min, float max) { m_rotation_range = { min, max }; }
		void set_rotation_speed_range(float min, float max) { m_rotation_speed_range = { min, max }; }

		void enable_rotation(bool enable) { m_rotate = enable; }

		bool is_system_active() const { return m_is_system_active; }
		void set_system_active() { m_is_system_active = true; }
		void set_system_inactive() { m_is_system_active = false; }

		void set_color_end(float r, float g, float b, float a) { m_color_end = { r, g, b, a }; }
		void set_color_end(const glm::vec4& color) { m_color_end = color; }

		void set_begin_end(float r, float g, float b, float a) { m_color_begin = { r, g, b, a }; }
		void set_begin_end(const glm::vec4& color) { m_color_begin = color; }

		std::shared_ptr<class texture> get_texture() { return m_texture_sprite; }
		glm::vec2 get_sprite_size();

		uint32_t get_max_particles() const { return m_max_particles; }
		
		struct particle
		{
			glm::vec2 position{ 0.0f, 0.0f };
			glm::vec2 velocity{ 1.0f, 1.0f };
			glm::vec2 size{ 1.0f, 1.0f };
			glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };

			float rotation = 0.0f;
			float rotation_speed = 0.1f;

			float life_time = 1.0f;
			float life = life_time;

			glm::vec2 texture_uv{ 0.0f, 0.0f };

			bool rotation_enabled = true;
			bool active = false;
		};

		iterator begin() const { return m_begin; }
		iterator end() const { return m_end; }

	private:
		void emit();
		void ou_update(float deltaTime);

	private:
		uint32_t m_max_particles;

		std::shared_ptr<class texture> m_texture_sprite;
		glm::vec2 m_texture_uv{ 0.0f, 0.0f }, m_texture_uv_stride{ 1.0f ,1.0f };

		bool m_sample_different_coords = false;
		/* if sample different coords use one random genator for u and another of v (as in uv coords) */
		std::uniform_int_distribution<> s_texture_coord_generator_u, s_texture_coord_generator_v;

		uint32_t m_particles_emited_per_interval = 4;
		float m_emit_interval = 0.2f;
		float m_emit_counter = 0.0f;

		float m_life_time = 1.0f;

		glm::vec2 m_base_size{ 1.0f, 1.0f };

		float size_begin = 1.0f, size_end = 0.01f;
		glm::vec4 m_color_begin{ 1.0f, 1.0f, 1.0f, 1.0f }, m_color_end{ 1.0f, 1.0f, 1.0f, 0.0f };

		glm::vec2 m_rotation_range{ 0.0f, 3.14159f * 2.0f };
		glm::vec2 m_rotation_speed_range{ 0.1f, 2.0f };

		glm::vec2 m_min_velocity{ 0.2f, 1.00f };
		glm::vec2 m_max_velocity{ 5.0f, 9.81f };

		std::vector<particle> m_particles;
		iterator m_begin, m_end;

		bool m_is_system_active;
		bool m_rotate = true;
	};

}