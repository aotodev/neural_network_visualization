#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D u_samplers[16];

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;

layout(push_constant, std430) uniform push_data { uint index; } u_data;

void main()
{
	vec4 color = in_color * texture(u_samplers[u_data.index], in_uv);

	if(color.a < 0.01)
       discard;

	out_color = color;
}