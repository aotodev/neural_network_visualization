#include "scene/particle_system.h"

#include "renderer/texture.h"
#include "core/log.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/compatibility.hpp>

static std::random_device s_random_device;
static std::default_random_engine s_random_engine(s_random_device());
static std::uniform_real_distribution<float> s_real_distribution(0.0f, 1.0f);

namespace gs {

	particle_system::particle_system(uint32_t maxParticles, bool startAlive)
		: 	m_max_particles(maxParticles),
			m_is_system_active(startAlive),
			m_particles(m_max_particles),
			m_begin(this, 0),
			m_end(this, 0)
	{
		if (INVERT_VIEWPORT) // invert default y velocity (gravity)
		{
			m_min_velocity.y *= -1.0f;
			m_max_velocity.y *= -1.0f;
		}
	}

	void particle_system::set_texture(const std::string& path, float u, float v, float strideX, float strideY)
	{
		m_texture_sprite = texture::create(path, true);
		m_texture_uv = { u, v };
		m_texture_uv_stride = { strideX, strideY };

		s_texture_coord_generator_u = std::uniform_int_distribution<>(0, (uint32_t)(1.0f / strideX));
		s_texture_coord_generator_v = std::uniform_int_distribution<>(0, (uint32_t)(1.0f / strideY));
	}

	void particle_system::set_texture(std::shared_ptr<class texture> texture, float u, float v, float strideX, float strideY)
	{
		m_texture_sprite = texture;
		m_texture_uv = { u, v };
		m_texture_uv_stride = { strideX, strideY };

		s_texture_coord_generator_u = std::uniform_int_distribution<>(0, (uint32_t)(1.0f / strideX) - 1);
		s_texture_coord_generator_v = std::uniform_int_distribution<>(0, (uint32_t)(1.0f / strideY) - 1);
	}

	glm::vec2 particle_system::get_sprite_size()
	{
		if (m_texture_sprite)
			return glm::vec2{ m_texture_sprite->get_width() * m_texture_uv_stride.x, m_texture_sprite->get_height() * m_texture_uv_stride.y };

		LOG_ENGINE(warn, "This Particle system does not have a texture; returning size { 0, 0 }");
		return glm::vec2{ 0ul, 0ul };
	}

	void particle_system::emit()
	{
		auto& particle = *m_end;

		particle.position = { 0.0f, 0.0f };
		particle.rotation = m_rotation_range.x + (m_rotation_range.y - m_rotation_range.x) * s_real_distribution(s_random_engine);
		particle.rotation_speed = m_rotation_speed_range.x + (m_rotation_speed_range.y - m_rotation_speed_range.x) * s_real_distribution(s_random_engine);
		particle.velocity.x = m_min_velocity.x + (m_max_velocity.x - m_min_velocity.x) * s_real_distribution(s_random_engine);
		particle.velocity.y = m_min_velocity.y + (m_max_velocity.y - m_min_velocity.y) * s_real_distribution(s_random_engine);
		particle.life_time = m_life_time;
		particle.life = m_life_time;

		if (m_sample_different_coords)
		{
			particle.texture_uv.x = m_texture_uv_stride.x * (float)(uint32_t)(1.0f / (float)(s_texture_coord_generator_u(s_random_device)));
			particle.texture_uv.y = m_texture_uv_stride.y * (float)(uint32_t)(1.0f / (float)(s_texture_coord_generator_u(s_random_device)));
		}
		else
		{
			particle.texture_uv = m_texture_uv;
		}

		particle.size = m_base_size;
		particle.color = m_color_begin;
		particle.rotation_enabled = m_rotate;
		particle.active = true;

		++m_end;
	}

	void particle_system::ou_update(float deltaTime)
	{
		m_emit_counter += deltaTime;
		if (m_emit_counter >= m_emit_interval)
		{
			for(uint32_t i = 0; i < m_particles_emited_per_interval; i++)
				emit();

			m_emit_counter = 0.0f;
		}

		for(auto pParticle = m_begin; pParticle != m_end; ++pParticle)
		{
			if(pParticle->rotation_enabled)
			{
				float rotation = pParticle->rotation + (deltaTime * pParticle->rotation_speed);
				pParticle->rotation = rotation > m_rotation_range.y ? m_rotation_range.x : rotation;
			}

			pParticle->position += pParticle->velocity * deltaTime;
			pParticle->life -= deltaTime;
			if (pParticle->life <= 0.0f)
			{
				pParticle->active = false;
				++m_begin;
				continue;
			}
			// Fade away particles
			float life = pParticle->life / pParticle->life_time;
			glm::vec4 color = glm::lerp(m_color_end, m_color_begin, life);
			pParticle->color = glm::vec4(glm::vec3(color), std::max(life, 0.0f));
		}
	}
}