#include "renderer/skybox.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"

namespace gs {

    static constexpr std::array<float, 108> s_vertices =
    {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f,  1.0f
    };

    uint32_t skybox::get_vertex_count()
    {
        return s_vertices.size();
    }

    skybox::skybox()
        : vertex_buffer(s_vertices.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, s_vertices.data(), s_vertices.size() * sizeof(float))
    {
    }

    skybox::skybox(const std::string& path, bool singleTexture)
        : vertex_buffer(s_vertices.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, s_vertices.data(), s_vertices.size() * sizeof(float))
    {
        sampler_info samplerInfo{};
        samplerInfo.wrap.u = sampler_wrap::clamp_edge;
        samplerInfo.wrap.v = sampler_wrap::clamp_edge;

        m_texture_cube = std::make_shared<texture_cube>(path, singleTexture, INVERT_VIEWPORT, samplerInfo);
    }

}