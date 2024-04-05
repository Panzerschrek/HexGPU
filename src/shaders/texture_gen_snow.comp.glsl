#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

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

	const ivec4 seed= ivec4(43, 41, 39, 37);
	ivec4 noise= ivec4(
		hex_Noise2(texel_coord.x, texel_coord.y, seed.x),
		hex_Noise2(texel_coord.x, texel_coord.y, seed.y),
		hex_Noise2(texel_coord.x, texel_coord.y, seed.z),
		hex_Noise2(texel_coord.x, texel_coord.y, seed.w));

	vec3 snow_color=
		vec3(0.95, 0.95, 0.95) * (7.0 / 8.0 + float(noise.w) / (65536.0 * 8.0)) +
		noise.rgb * (1.0 / (65536.0 * 64.0));

	vec4 color= vec4(large_scale_noise_factor * snow_color, 1.0);

	imageStore(out_image, texel_coord, color);
}
