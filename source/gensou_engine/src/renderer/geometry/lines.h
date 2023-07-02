#pragma once

#include "renderer/buffer.h"
#include "renderer/memory_manager.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace gs {

    struct line_vertex
    {
        line_vertex() = default;
        line_vertex(const glm::vec3& pos, const glm::vec4& col)
            : position(pos), color(col){}

        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    class line_geometry
    {
        friend class renderer;

        typedef std::vector<std::pair<uint32_t, glm::vec2>> line_draw_call;
    public:
        line_geometry();

        static VkPipelineVertexInputStateCreateInfo get_state_input_info();

        void submit(const glm::vec2& edgeRange, const glm::vec3& p1Pos, const glm::vec4& p1Color, const glm::vec3& p2Pos, const glm::vec4& p2Color);
        void submit_range(const line_vertex* start, size_t lineCount, const glm::vec2& edgeRange);

        void start_frame();
        void end_frame();

        auto& get_draw_calls(uint32_t frame)
        {
            draw_calls[frame] = working_draw_calls;
            return draw_calls[frame];
        }

    private:
        void push_draw_call(const glm::vec2& pushConstant);
        void push_draw_call(const glm::vec2& pushConstant, uint32_t count);

    private:
        buffer<cpu_to_gpu> vertex_buffer;

        buffer<no_vma_cpu> working_buffer;
        uint32_t count = 0;
        uint32_t current_offset = 0;

		/* one for the app/main thread and 3 for the render thread(one per frame in flight) */
		line_draw_call working_draw_calls;
		std::array<line_draw_call, MAX_FRAMES_IN_FLIGHT> draw_calls;
    };
}