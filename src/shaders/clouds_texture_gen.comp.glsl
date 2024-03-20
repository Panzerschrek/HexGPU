#version 450

#extension GL_GOOGLE_include_directive : require

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 8, local_size_y = 16, local_size_z= 1) in;

layout(binding= 0, rgba8) uniform writeonly image2D out_image;

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	vec4 color= vec4(fract(vec2(texel_coord) / 32.0), 0.0, 0.5);

	imageStore(out_image, texel_coord, color);
}
