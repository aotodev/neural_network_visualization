#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_corners;

layout (set = 0, binding = 0) uniform camera
{
    mat4 projection_view;
} u_camera;

vec2 corners[2] = vec2[](
    vec2(1.0, 1.0),
    vec2(0.0, 0.0)
);

void main() 
{
	out_color = in_color;
    gl_Position = u_camera.projection_view * vec4(in_position, 1.0);

    const int index = int(mod(gl_VertexIndex, 2));
    out_corners = corners[index];
}