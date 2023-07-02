#pragma once

#include "core/core.h"
#include "core/runtime.h"

#include <glm/glm.hpp>

namespace gs {

	struct quad_area
	{
		quad_area() : x(0.0f), y(0.0f), size_x(1.0f), size_y(1.0f) {}
		quad_area(float lowerX, float lowerY, float sizeX, float sizeY) : x(lowerX), y(lowerY), size_x(sizeX), size_y(sizeY) {}

		void reset() { x = 0.0f, y = 0.0f, size_x = 1.0f; size_y = 1.0f; }

		void set_quad(float xPos, float yPos, float sizeX, float sizeY)
		{
			x = xPos; y = yPos; size_x = sizeX; size_y = sizeY;
		}

		float x = 0.0f, y = 0.0f;
		float size_x = 1.0f, size_y = 1.0f;
	};

	/* converts world position to uv coords[0.0, 1.0].usefull for sampling parts of a framebuffer */
	inline std::pair<glm::vec2, glm::vec2> world_position_to_uv(const glm::vec2& coords, const glm::vec2& size)
	{
		glm::vec2 stride = size;
		glm::vec2 uv = coords;

		uv += glm::vec2(runtime::viewport().width * 0.5f, runtime::viewport().height * 0.5f);

		uv.x -= size.x * 0.5f;
		uv.y -= size.y * 0.5f;

		uv *= glm::vec2(1.0f / runtime::viewport().width, 1.0f / runtime::viewport().height);
		stride *= glm::vec2(1.0f / runtime::viewport().width, 1.0f / runtime::viewport().height);

		return std::make_pair(uv, stride);
	}

	inline bool overlaps_rect_rect(const glm::vec2& rect1Pos, const glm::vec2& rect1Size, const glm::vec2& rect2Pos, const glm::vec2& rect2Size)
	{
		const float left = rect1Pos.x - (rect1Size.x * 0.5f);
		const float right = rect1Pos.x + (rect1Size.x * 0.5f);
		const float up = rect1Pos.y + (rect1Size.y * 0.5f);
		const float down = rect1Pos.y - (rect1Size.y * 0.5f);

		const float otherLeft = rect2Pos.x - (rect2Size.x * 0.5f);
		const float otherRight = rect2Pos.x + (rect2Size.x * 0.5f);
		const float otherUp = rect2Pos.y + (rect2Size.y * 0.5f);
		const float otherDown = rect2Pos.y - (rect2Size.y * 0.5f);

		return otherLeft >= left && otherRight <= right && otherUp <= up && otherDown >= otherDown;
	}

	inline bool overlaps_rect_point(const glm::vec2& rectPos, const glm::vec2& rectSize, const glm::vec2& point)
	{
		const float left = rectPos.x - (rectSize.x * 0.5f);
		const float right = rectPos.x + (rectSize.x * 0.5f);
		const float up = rectPos.y + (rectSize.y * 0.5f);
		const float down = rectPos.y - (rectSize.y * 0.5f);

		return point.x >= left && point.x <= right && point.y <= up && point.y >= down;
	}

    inline bool overlaps_rect_circle(const glm::vec2& rectPos, const glm::vec2& rectSize, const glm::vec2& circleCenter, const float circleRadius)
	{
		glm::vec2 rectHalfExtent = { rectSize.x * 0.5f, rectSize.y * 0.5f };
		return glm::length(rectPos + glm::clamp(circleCenter - rectPos, -rectHalfExtent, rectHalfExtent) - circleCenter) < circleRadius;
	}

	inline glm::vec3 revert_gamma_correction(const glm::vec3& color)
	{
		return glm::pow(color, glm::vec3(2.2f));
	}

    inline uint32_t calculate_mip_count(uint32_t width, uint32_t height)
    {
    	return std::floor(std::log2(std::max(width, height))) + 1;
    }

	extern dword get_hashcode_from_binary(const byte* pData, size_t dataSize);

	template<uint8_t v>
	constexpr float normalized_color_channel = static_cast<float>(static_cast<double>(v) / 255.0);

	template<uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255>
	constexpr glm::vec4 normalized_color = glm::vec4{ normalized_color_channel<r>, normalized_color_channel<g>, normalized_color_channel<b>, normalized_color_channel<a> };

	template<typename T>
	inline bool is_future_ready(const std::future<T>& future)
	{
		if (!future.valid())
			return false;

		return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	}

	/* first == quad_count in this draw, second == texture index (push constant) in this draw */
	typedef std::vector<std::pair<uint32_t, uint32_t>> draw_call;
}