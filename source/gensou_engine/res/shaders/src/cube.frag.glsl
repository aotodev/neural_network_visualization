#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec4 in_color;

void main()
{
	if(in_color.a < 0.1)
       discard;

	out_color = in_color;
}