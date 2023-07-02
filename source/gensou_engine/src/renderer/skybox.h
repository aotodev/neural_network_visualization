#pragma once

#include "renderer/texture.h"
#include "renderer/buffer.h"
#include "renderer/memory_manager.h"
#include "renderer/pipeline.h"

#include <vulkan/vulkan.h>

namespace gs {

    class skybox
    {
        friend class renderer;

    public:
        skybox();
        skybox(const std::string& path, bool singleTexture);

        static uint32_t get_vertex_count();

    private:
        void create_renderpass();

    private:
    	buffer<gpu_only> vertex_buffer;
		std::shared_ptr<texture_cube> m_texture_cube;
    };

}