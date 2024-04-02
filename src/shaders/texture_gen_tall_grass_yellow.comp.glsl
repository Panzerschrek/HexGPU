#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

const vec4 c_color_grass_dark= vec4(0.45, 0.45, 0.12, 1.0);
const vec4 c_color_grass_light= vec4(0.64, 0.65, 0.32, 1.0);

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	const int octaves= 4;

	int scaler= 0;
	for(int i= 0; i < octaves; ++i)
		scaler+= 65536 >> i;

	int noise_val=
		hex_TextureOctaveNoiseWraped(texel_coord.x, texel_coord.y, 0, octaves, c_texture_size_log2);

	float tex_coord_warped= float(texel_coord.x) + float(noise_val) * (2.0 / 65536.0);

	const float frequency= 2.0 * c_pi * 32.0 / float(1 << c_texture_size_log2);
	float s= sin(tex_coord_warped * frequency);

	float y= float(texel_coord.y) - 0.5 * float(texel_coord.x & 1);

	float alpha= s * 0.5 + 0.5;
	alpha= 1.0 - alpha * smoothstep(16.0, 20.0, y);
	alpha*= 1.0 - smoothstep(24.0, 38.0, y);
	float alpha_binary= step(0.5, alpha);

	float vertical_gradient= 0.8 + 0.2 * smoothstep(16.0, 32.0, y);
	vec3 self_color= mix(c_color_grass_light.rgb, c_color_grass_dark.rgb, s * 0.3 + 0.3) * vertical_gradient;

	vec4 color= vec4(self_color, alpha_binary);

	imageStore(out_image, texel_coord, color);
}
