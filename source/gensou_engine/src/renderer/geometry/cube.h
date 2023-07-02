#pragma once

#include "renderer/buffer.h"
#include "renderer/pipeline.h"
#include "renderer/memory_manager.h"

#include <glm/glm.hpp>

namespace gs {

    class cube_geometry
    {
        friend class renderer;
    public:
        cube_geometry();

        static VkPipelineVertexInputStateCreateInfo get_state_input_info();

        void submit(const glm::vec4& color, const glm::mat4& transform);

        void start_frame();
        void end_frame();

        static uint32_t indices_count();

    private:
        buffer<gpu_only> vertex_buffer, index_buffer;
        buffer<cpu_to_gpu> instance_buffer;

        buffer<no_vma_cpu> working_buffer;
        uint32_t count = 0;
        uint32_t current_offset = 0;
    };

}