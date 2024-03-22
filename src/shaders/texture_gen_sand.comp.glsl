#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 8, local_size_y = 16, local_size_z= 1) in;

layout(binding= 0, rgba8) uniform writeonly image2D out_image;

const int c_texture_size_log2= 7; // This must match texture size in C++ code!

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	const int octaves= 4;
	int scaler= 0;
	for(int i= 0; i < octaves; ++i)
		scaler+= 65536 >> i;

	int large_scale_noise=
		hex_TextureOctaveNoiseWraped(texel_coord.x, texel_coord.y, 0, octaves, c_texture_size_log2);

	float large_scale_noise_factor= (float(large_scale_noise) / float(scaler)) * 0.2 + 0.8;

	const ivec4 seed= ivec4(37, 39, 41, 43);
	ivec4 noise= ivec4(
		hex_Noise2(texel_coord.x, texel_coord.y, seed.x),
		hex_Noise2(texel_coord.x, texel_coord.y, seed.y),
		hex_Noise2(texel_coord.x, texel_coord.y, seed.z),
		hex_Noise2(texel_coord.x, texel_coord.y, seed.w));

	vec3 sand_color=
		vec3(0.65, 0.65, 0.45) * (5.0 / 6.0 + float(noise.w) / (65536.0 * 6.0)) +
		noise.rgb * (1.0 / (65536.0 * 24.0));

	vec4 color= vec4(large_scale_noise_factor * sand_color, 1.0);

	imageStore(out_image, texel_coord, color);
}
