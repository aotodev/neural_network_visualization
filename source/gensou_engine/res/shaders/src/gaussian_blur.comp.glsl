#version 450

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0, rgba32f) restrict writeonly uniform image2D o_image;
layout(binding = 1) uniform sampler2D u_sampler;

layout(push_constant, std430) uniform push_data
{ 
    uint x_offset, y_offset;
    uint horizontal_pass;

} u_data;


// gl_GlobalInvocationID = gl_WorkGroupID * gl_WorkGroupSize + gl_LocalInvocationID

const float weights[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{             
    vec2 imgSize = vec2(imageSize(o_image));
    vec2 texelSize = 1.0 / textureSize(u_sampler, 0);
    vec2 currentPixel = vec2(gl_GlobalInvocationID.x + u_data.x_offset, gl_GlobalInvocationID.y + u_data.y_offset) * texelSize;
    vec3 color = vec3(0, 0, 0);

    color += texture(u_sampler, currentPixel).rgb * weights[0];

    if(bool(u_data.horizontal_pass))
    {
        // -4 <- -3 <- -2 <- -1 <- current pixel -> 1 -> 2 -> 3 -> 4
        for(int i = 1; i < 5; ++i)
        {
            // five texels to the right
            color += texture(u_sampler, currentPixel + vec2(texelSize.x * i, 0.0)).rgb * weights[i];
            // five texels to the left
            color += texture(u_sampler, currentPixel - vec2(texelSize.x * i, 0.0)).rgb * weights[i];       
        }
    }
    else /* vertical pass */
    {
        for(int i = 1; i < 5; ++i)
        {
            // five texels upwards
            color += texture(u_sampler, currentPixel + vec2(0.0, texelSize.y * i)).rgb * weights[i];
            // five texels downwards
            color += texture(u_sampler, currentPixel - vec2(0.0, texelSize.y * i)).rgb * weights[i]; 
        }
    }

    imageStore(o_image, ivec2(gl_GlobalInvocationID.x + u_data.x_offset, gl_GlobalInvocationID.y + u_data.y_offset), vec4(color, 1.0));
}