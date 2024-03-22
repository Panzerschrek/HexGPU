#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/noise.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 8, local_size_y = 16, local_size_z= 1) in;

layout(binding= 0, rgba8) uniform writeonly image2D out_image;

const int c_texture_size_log2= 7; // This must match texture size in C++ code!

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
