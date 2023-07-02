#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 in_position;

layout(location = 0) out vec3 out_direction_vec;

layout (set = 0, binding = 0) uniform camera
{
	mat4 view;
	mat4 projection;
}u_camera;

void main()
{
    out_direction_vec = in_position;
    out_direction_vec.xyz *= -1.0;

    mat4 view = mat4(mat3(u_camera.view));
    gl_Position = u_camera.projection * view * vec4(in_position, 1.0);
}  
