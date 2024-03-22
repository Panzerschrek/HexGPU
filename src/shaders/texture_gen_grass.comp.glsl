#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 8, local_size_y = 16, local_size_z= 1) in;

layout(binding= 0, rgba8) uniform writeonly image2D out_image;

const vec4 c_color_grass_dark= vec4(0.23, 0.45, 0.12, 1.0);
const vec4 c_color_grass_light= vec4(0.38, 0.65, 0.32, 1.0);
const vec4 c_color_yellow= vec4(0.50, 0.55, 0.28, 1.0);

const int c_texture_size_log2= 7; // This must match texture size in C++ code!

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	const int octaves= 2;

	int scaler= 0;
	for(int i= 0; i < octaves; ++i)
		scaler+= 65536 >> i;

	float grass_noise=
		hex_TextureOctaveNoiseWraped(texel_coord.x, texel_coord.y, 0, octaves, c_texture_size_log2) / float(scaler);

	float mask_noise=
		hex_TextureOctaveNoiseWraped(texel_coord.x, texel_coord.y, 2, octaves, c_texture_size_log2) / float(scaler);

	float mask_factor= smoothstep(0.6, 0.8, mask_noise);

	vec4 grass_color= mix(c_color_grass_dark, c_color_grass_light, grass_noise);

	vec4 color= mix(grass_color, c_color_yellow, mask_factor);

	imageStore(out_image, texel_coord, color);
}
