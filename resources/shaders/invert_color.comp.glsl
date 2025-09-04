#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0, rgba8) uniform image2D inputImage;

void main()
{
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
    
    ivec2 dims = imageSize(inputImage);
    if (pixel_coords.x < dims.x && pixel_coords.y < dims.y)
    {
        vec4 pixel = imageLoad(inputImage, pixel_coords);
        pixel.rgb = vec3(1.0) - pixel.rgb;
        imageStore(inputImage, pixel_coords, pixel);
    }
}