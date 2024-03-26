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

	int noise_val=
		hex_TextureOctaveNoiseWraped(texel_coord.x * 2, texel_coord.y, 0, octaves, c_texture_size_log2);

	float brightness= float(noise_val) / float(scaler);
	vec4 color= vec4(brightness, brightness, brightness, 1.0);

	imageStore(out_image, texel_coord, color);
}
