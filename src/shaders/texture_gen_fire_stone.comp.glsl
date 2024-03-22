#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

const vec4 c_color_a= vec4(0.70, 0.30, 0.30, 1.0);
const vec4 c_color_b= vec4(0.60, 0.60, 0.20, 1.0);

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	const int octaves= 2;

	int scaler= 0;
	for(int i= 0; i < octaves; ++i)
		scaler+= 65536 >> i;

	int noise_val=
		hex_TextureOctaveNoiseWraped(texel_coord.x, texel_coord.y, 0, octaves, c_texture_size_log2);

	float brightness= float(noise_val) / float(scaler);
	vec4 color= mix(c_color_a, c_color_b, brightness);

	imageStore(out_image, texel_coord, color);
}
