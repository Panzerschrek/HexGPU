#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 8, local_size_y = 16, local_size_z= 1) in;

layout(binding= 0, rgba8) uniform writeonly image2D out_image;

const vec4 c_color_a= vec4(0.17, 0.06, 0.06, 1.0);
const vec4 c_color_b= vec4(0.36, 0.29, 0.25, 1.0);

const int c_texture_size_log2= 7; // This must match texture size in C++ code!

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	const int octaves= 2;

	int scaler= 0;
	for(int i= 0; i < octaves; ++i)
		scaler+= 65536 >> i;

	int noise_val=
		hex_TriangularInterpolatedOctaveNoiseWraped(texel_coord.x, texel_coord.y, 0, octaves, c_texture_size_log2);

	float brightness= float(noise_val) / float(scaler);
	vec4 color= mix(c_color_a, c_color_b, brightness);

	imageStore(out_image, texel_coord, color);
}
