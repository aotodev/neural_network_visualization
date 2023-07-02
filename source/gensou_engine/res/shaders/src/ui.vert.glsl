#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3    in_position;
layout(location = 1) in vec2    in_uv;
layout(location = 2) in vec4    in_color;
layout(location = 3) in vec2    in_size;
layout(location = 4) in float   in_radius;
layout(location = 5) in float   in_thickness;
layout(location = 6) in float   in_fade;

layout(location = 0) out vec2   out_uv;
layout(location = 1) out vec4   out_color;
layout(location = 2) out vec2   out_size;
layout(location = 3) out float  out_fade;
layout(location = 4) out float  out_radius;
layout(location = 5) out float  out_thickness;
layout(location = 6) out vec2   out_corners;

layout (set = 0, binding = 0) uniform camera
{
    mat4 projection_view;
} u_camera;

vec2 corners[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, 1.0)
);

void main() 
{
    gl_Position = u_camera.projection_view * vec4(in_position, 1.0);

    out_uv = in_uv;
    out_color = in_color;
    out_size = in_size;
    out_radius = in_radius;
    out_thickness = in_thickness;
    out_fade = in_fade;

    const int index = int(mod(gl_VertexIndex, 4));
    out_corners = corners[index];
}