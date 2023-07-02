#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec3 in_direction_vec;

layout(set = 1, binding = 0) uniform samplerCube u_texture_cube;

void main()
{
    out_color = texture(u_texture_cube, in_direction_vec);
}
