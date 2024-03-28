#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/texture_gen_common.glsl"

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	ivec2 grid= texel_coord & 31;
	ivec2 grid_cell= texel_coord >> 5;

	vec2 texel_coord_f= vec2(float(texel_coord.x), float(texel_coord.y) - 0.5 * float(texel_coord.x & 1));

	float frequency= c_pi / 16.0;
	float s= sin((texel_coord_f.x + texel_coord_f.y * 2.0 + float(grid_cell.x * 14.0 + grid_cell.y * 7.0)) * frequency);

	vec4 color= vec4(vec3(0.8, 0.8, 0.8) * (0.75 + 0.25 * s), 0.45 + 0.15 * s);

	if(grid.x <= 1 || grid.y <= 1)
		color= vec4(0.4, 0.5, 0.6, 1.0);

	imageStore(out_image, texel_coord, color);
}
