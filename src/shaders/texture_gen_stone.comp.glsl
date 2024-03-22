#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

const vec4 c_color_dark= vec4(0.30, 0.33, 0.33, 1.0);
const vec4 c_color_light= vec4(0.60, 0.60, 0.55, 1.0);
const vec4 c_color_vein_dark= vec4(0.35, 0.35, 0.30, 1.0);
const vec4 c_color_vein_light= vec4(0.55, 0.60, 0.60, 1.0);

float BaseStoneNoise(ivec2 texel_coord)
{
	const int octaves= 4;

	int scaler= 0;
	for(int i= 0; i < octaves; ++i)
		scaler+= 65536 >> i;

	int noise_val=
		hex_TextureOctaveNoiseWraped(texel_coord.x, texel_coord.y * 2, 0, octaves, c_texture_size_log2);

	return float(noise_val) / float(scaler);
}

float AdditionalStoneNoise(ivec2 texel_coord)
{
	const int octaves= 3;

	int scaler= 0;
	for(int i= 0; i < octaves; ++i)
		scaler+= 65536 >> i;

	int noise_val=
		hex_TextureOctaveNoiseWraped(texel_coord.x, texel_coord.y, 2, octaves, c_texture_size_log2);

	return float(noise_val) / float(scaler);
}

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	float stone_factor= BaseStoneNoise(texel_coord);

	float additional_factor= AdditionalStoneNoise(texel_coord);
	float vein_dark_factor= smoothstep(0.6, 0.9, additional_factor);
	float vein_light_factor= 1.0 - smoothstep(0.1, 0.4, additional_factor);

	vec4 stone_base_color= mix(c_color_dark, c_color_light, stone_factor);
	vec4 stone_color= mix(mix(stone_base_color, c_color_vein_dark, vein_dark_factor), c_color_vein_light, vein_light_factor);

	imageStore(out_image, texel_coord, stone_color);
}
