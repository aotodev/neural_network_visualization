#version 450

layout (location = 0) out vec4 out_color;

layout (set = 0, binding = 0) uniform sampler2D u_sampler;

layout (location = 0) in vec2 in_uv;

void main()
{
    out_color = vec4(texture(u_sampler, in_uv).rgb, 1.0);
}