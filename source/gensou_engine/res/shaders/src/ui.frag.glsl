#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D u_samplers[16];

layout(location = 0) in vec2	in_uv;
layout(location = 1) in vec4	in_color;
layout(location = 2) in vec2	in_size;
layout(location = 3) in float	in_fade;
layout(location = 4) in float	in_radius;
layout(location = 5) in float	in_thickness;
layout(location = 6) in vec2	in_corners;

layout(push_constant, std430) uniform push_data { uint index; } u_data;


float get_alpha(vec2 coords, float fade)
{
	float distance = 1.0 - length(coords);
    float alpha = smoothstep(0.0, fade, distance);
    alpha *= smoothstep(1.0 + fade, 1.0, distance);

	return alpha;
}

float sdf_round_box(vec2 coords, vec2 resolution, float radius)
{
	return length(max(abs(coords) - resolution + vec2(radius), vec2(0.0))) - radius;
} 

void main()
{
	vec4 color = in_color;

 	// for circles
	if(in_fade > 0.0)
		color.a = get_alpha(in_corners, in_fade);

	// for round edges
	if(in_radius > 0.0)
	{
		float smooth_alpha = 1.0;

		if(in_thickness > 0.0)
		{
			float d = abs(sdf_round_box(in_corners * in_size, in_size / 1.1, in_radius) / in_thickness);
			smooth_alpha = smoothstep(0.8, 0.33, d);
		}
		else
		{
			smooth_alpha = smoothstep(1.0, 0.0, sdf_round_box(in_corners * in_size, in_size, in_radius));
		}

		color.a *= smooth_alpha;
	}

	color *= texture(u_samplers[u_data.index], in_uv);

	if(color.a < 0.01)
       discard;

	out_color = color;
}