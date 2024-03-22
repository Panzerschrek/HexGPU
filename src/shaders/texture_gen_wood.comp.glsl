#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

vec3 c_color_dark= vec3(0.22, 0.12, 0.06);
vec3 c_color_light= vec3(0.40, 0.27, 0.22);

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	const int octaves= 3;

	int scaler= 0;
	for(int i= 0; i < octaves; ++i)
		scaler+= 65536 >> i;

	int noise_val=
		hex_TextureOctaveNoiseWraped(texel_coord.x, texel_coord.y, 0, octaves, c_texture_size_log2);

	float tex_coord_warped= float(texel_coord.x) + float(noise_val) * (1.5 / 65536.0);

	float frequency= c_pi / 2.0;
	float s= sin(tex_coord_warped * frequency);

	vec4 color= vec4(mix(c_color_dark, c_color_light, s * 0.5 + 0.5), 1.0);

	imageStore(out_image, texel_coord, color);
}
