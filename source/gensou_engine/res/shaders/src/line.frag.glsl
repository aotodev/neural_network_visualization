#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_corners;

layout(push_constant, std430) uniform push_data { vec2 edge_range; } u_data;

void main()
{
	float distance = length(in_corners);
	float value = distance > u_data.edge_range.x + u_data.edge_range.y ? 0.0 : distance;
	vec4 color = in_color * step(u_data.edge_range.x, value);

	if(color.a < 0.1)
       discard;

	out_color = color;
}