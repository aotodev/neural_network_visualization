#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;

// instanced attributes
layout (location = 1) in vec4 in_instance_color;
layout (location = 2) in mat4 in_instance_transform;

layout(location = 0) out vec4 out_color;

layout (set = 0, binding = 0) uniform camera
{
    mat4 projection_view;
} u_camera;


void main() 
{
	out_color = in_instance_color;
    gl_Position = u_camera.projection_view * in_instance_transform * vec4(in_position, 1.0);
}
