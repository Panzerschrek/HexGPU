#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/texture_gen_common.glsl"

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	vec2 texel_coord_f= vec2(texel_coord);

	vec2 tex_coord_warped= texel_coord_f+= 1.5 * sin(texel_coord_f.yx * (c_pi / 8.0));

	float frequency= c_pi / 16.0;
	float s0= sin(tex_coord_warped.y * frequency);
	float s1= sin(( tex_coord_warped.y * 1.5 + tex_coord_warped.x) * frequency);
	float s2= sin((-tex_coord_warped.y * 1.5 + tex_coord_warped.x) * frequency);

	vec3 base= vec3(0.05, 0.05, 0.1);
	vec3 wave0= (s0 * 0.4 + 0.6) * vec3(0.0, 0.1, 0.3);
	vec3 wave1= (s1 * 0.4 + 0.6) * vec3(0.05, 0.15, 0.25);
	vec3 wave2= (s2 * 0.4 + 0.6) * vec3(0.1, 0.17, 0.23);

	vec4 color= vec4(base + wave0 + wave1 + wave2, 0.5);

	imageStore(out_image, texel_coord, color);
}
