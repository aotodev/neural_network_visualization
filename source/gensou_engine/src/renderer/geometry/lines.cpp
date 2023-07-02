#include "renderer/geometry/lines.h"

#include "core/core.h"
#include "renderer/renderer.h"
#include "core/runtime.h"
#include "core/misc.h"

namespace gs {

    static constexpr uint64_t s_frame_vertex_buffer_size = uint64_t(MiB) * 4ULL;

    line_geometry::line_geometry()
        : vertex_buffer(s_frame_vertex_buffer_size * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, nullptr, 0),
		  working_buffer(s_frame_vertex_buffer_size)
    {
        working_draw_calls.reserve(16);

        for(auto& dc : draw_calls)
            dc.reserve(16);
    }

    VkPipelineVertexInputStateCreateInfo line_geometry::get_state_input_info()
    {
        static constexpr VkVertexInputAttributeDescription vertexDescription[3] =
        {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,		offsetof(line_vertex, position) }, // position
            { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT,	offsetof(line_vertex, color) } // color
        };

        static constexpr VkVertexInputBindingDescription vertexBindingDescriptions[1] =
        {
            { 0, sizeof(line_vertex), VK_VERTEX_INPUT_RATE_VERTEX }
        };

        VkPipelineVertexInputStateCreateInfo vertexInputState{};
        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputState.vertexBindingDescriptionCount = 1;
        vertexInputState.pVertexBindingDescriptions = vertexBindingDescriptions;
        vertexInputState.vertexAttributeDescriptionCount = 2;
        vertexInputState.pVertexAttributeDescriptions = vertexDescription;

        return vertexInputState;
    }

    void line_geometry::push_draw_call(const glm::vec2& pushConstant, uint32_t count)
    {
        if (working_draw_calls.empty())
	    {
		    working_draw_calls.emplace_back(std::make_pair(count, pushConstant));
	    }
        else if(pushConstant == working_draw_calls.back().second)
        {
		    working_draw_calls.back().first += count;
	    }
        else
		{
			working_draw_calls.emplace_back(std::make_pair(count, pushConstant));
		}
    }

    void line_geometry::push_draw_call(const glm::vec2& pushConstant)
    {
        push_draw_call(pushConstant, 1ul);
    }


    void line_geometry::submit(const glm::vec2& edgeRange, const glm::vec3& p1Pos, const glm::vec4& p1Color, const glm::vec3& p2Pos, const glm::vec4& p2Color)
    {
		auto p1NewColor = glm::vec4(revert_gamma_correction(glm::vec3(p1Color)), p1Color.a);
		auto p2NewColor = glm::vec4(revert_gamma_correction(glm::vec3(p2Color)), p2Color.a);

        size_t offset = count * sizeof(line_vertex) * 2UL;
        working_buffer.emplace<line_vertex>(offset, p1Pos, p1NewColor);
        working_buffer.emplace<line_vertex>(offset + sizeof(line_vertex), p2Pos, p2NewColor);

        push_draw_call(edgeRange);
        count++;
    }

    void line_geometry::submit_range(const line_vertex* start, size_t lineCount, const glm::vec2& edgeRange)
    {
        working_buffer.write(start, lineCount * sizeof(line_vertex) * 2ULL, count * sizeof(line_vertex) * 2UL);

        push_draw_call(edgeRange, lineCount);
        count += lineCount;
    }

    void line_geometry::start_frame()
    {
        current_offset = s_frame_vertex_buffer_size * runtime::current_frame();

        if(count)
        {
            size_t dataSize = count * sizeof(line_vertex) * 2ULL;
            vertex_buffer.write(working_buffer.data(), dataSize, current_offset);
        }
    }

    void line_geometry::end_frame()
    {
        working_buffer.reset();
        working_draw_calls.clear();
        count = 0;
    }

}