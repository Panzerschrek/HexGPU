#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

const vec4 c_color_grass_light= vec4(0.38, 0.65, 0.32, 1.0);

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	const int octaves= 4;

	int scaler= 0;
	for(int i= 0; i < octaves; ++i)
		scaler+= 65536 >> i;

	int noise_val=
		hex_TextureOctaveNoiseWraped(texel_coord.x, texel_coord.y, 0, octaves, c_texture_size_log2);

	float tex_coord_warped= float(texel_coord.x) + float(noise_val) * (3.0 / 65536.0);

	float frequency= c_pi / 4.0;
	float s= sin(tex_coord_warped * frequency);

	float alpha= s * 0.5 + 0.5;
	alpha= 1.0 - alpha * smoothstep(0.0, 8.0, float(texel_coord.y));
	float alpha_binary= step(0.5, alpha);

	vec4 color= vec4(c_color_grass_light.rgb, alpha_binary);

	imageStore(out_image, texel_coord, color);
}
