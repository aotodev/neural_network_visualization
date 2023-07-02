#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_uv; /* w is the layer */
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

layout (set = 0, binding = 0) uniform camera
{
    mat4 projection_view;
} u_camera;


void main() 
{
    gl_Position = u_camera.projection_view * vec4(in_position, 1.0);

    out_uv = in_uv;
    out_color = in_color;
}