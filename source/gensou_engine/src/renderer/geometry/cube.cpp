#include "renderer/geometry/cube.h"

#include "core/runtime.h"
#include "core/misc.h"

namespace gs {

    static constexpr uint64_t s_frame_vertex_buffer_size = uint64_t(MiB);

    namespace {

        static constexpr std::array<float, 24> cube_vertices = 
        {
            -1.0f,1.0f,1.0f,
            -1.0f,-1.0f,1.0f,
            1.0f,1.0f,1.0f,
            1.0f,-1.0f,1.0f,
            -1.0f,1.0f,-1.0f,
            -1.0f,-1.0f,-1.0f,
            1.0f,1.0f,-1.0f,
            1.0f,-1.0f,-1.0f
        };

        static constexpr std::array<uint16_t, 36> cube_indices = 
        {
            0, 2, 3, 0, 3, 1,
            2, 6, 7, 2, 7, 3,
            6, 4, 5, 6, 5, 7,
            4, 0, 1, 4, 1, 5,
            0, 4, 6, 0, 6, 2,
            1, 5, 7, 1, 7, 3
        };

        struct cube_instance_data
        {
            glm::vec4 color;
            glm::mat4 transform;
        };

    }

    uint32_t cube_geometry::indices_count() { return cube_indices.size(); }

    cube_geometry::cube_geometry()
        : vertex_buffer(cube_vertices.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, cube_vertices.data(), cube_vertices.size() * sizeof(float)),
		  index_buffer(cube_indices.size() * sizeof(uint16_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, cube_indices.data(), cube_indices.size() * sizeof(uint16_t)),
          instance_buffer(s_frame_vertex_buffer_size * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, nullptr, 0),
		  working_buffer(s_frame_vertex_buffer_size)
    {}

    VkPipelineVertexInputStateCreateInfo cube_geometry::get_state_input_info()
    {
        static constexpr VkVertexInputAttributeDescription vertexDescription[6] =
        {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,		0u }, // position
            { 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT,	offsetof(cube_instance_data, color) },
            { 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT,	offsetof(cube_instance_data, transform) },
            { 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT,	offsetof(cube_instance_data, transform) + sizeof(glm::vec4) * 1 },
            { 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT,	offsetof(cube_instance_data, transform) + sizeof(glm::vec4) * 2 },
            { 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT,	offsetof(cube_instance_data, transform) + sizeof(glm::vec4) * 3 }
        };

        static constexpr VkVertexInputBindingDescription vertexBindingDescriptions[2] =
        {
            { 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX },
            { 1, sizeof(cube_instance_data), VK_VERTEX_INPUT_RATE_INSTANCE },
        };

        VkPipelineVertexInputStateCreateInfo vertexInputState{};
        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputState.vertexBindingDescriptionCount = 2;
        vertexInputState.pVertexBindingDescriptions = vertexBindingDescriptions;
        vertexInputState.vertexAttributeDescriptionCount = 6;
        vertexInputState.pVertexAttributeDescriptions = vertexDescription;

        return vertexInputState;
    }

    void cube_geometry::submit(const glm::vec4& color, const glm::mat4& transform)
    {
		auto newColor = glm::vec4(revert_gamma_correction(glm::vec3(color)), color.a);
        uint32_t offset = count * sizeof(cube_instance_data);

        auto instance = working_buffer.emplace<cube_instance_data>(offset);
        instance->color = newColor;
        instance->transform = transform;

        count++;
    }

    void cube_geometry::start_frame()
    {
        current_offset = s_frame_vertex_buffer_size * runtime::current_frame();
        if(count)
            instance_buffer.write(working_buffer.data(), count * sizeof(cube_instance_data), current_offset);
    }

    void cube_geometry::end_frame()
    {
        working_buffer.reset();
        count = 0;
    }

}
