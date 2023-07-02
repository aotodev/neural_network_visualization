#pragma once

#include "renderer/texture.h"
#include <renderer/texture.h>

#include <glm/glm.hpp>

namespace gs {

	struct sprite
	{
		sprite() : uv(0.0f, 0.0f), stride(1.0f, 1.0f) {}

		sprite(std::shared_ptr<texture> inTexture, const glm::vec2& inUv, const glm::vec2& inStride)
			: tex(inTexture), uv(inUv), stride(inStride) {}

		std::shared_ptr<texture> tex;
		glm::vec2 uv;
		glm::vec2 stride;

		float get_width() { return tex->get_width() * stride.x; }
		float get_height() { return tex->get_height() * stride.y; }

		void reset()
		{
			tex.reset(); uv = glm::vec2(0.0f); stride = glm::vec2(1.0f);
		}

		operator bool() const { return tex != nullptr; }
	};

}