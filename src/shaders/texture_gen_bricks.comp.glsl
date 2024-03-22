#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

const vec4 c_brick_color= vec4(0.65, 0.35, 0.32, 1.0);
const vec4 c_mortar_color= vec4(0.843, 0.792, 0.792, 1.0);

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	int bricks_row= texel_coord.y >> 3;
	int y_within_row= texel_coord.y & 7;

	bool is_horisontal_mortar= y_within_row == 0;

	bool is_vertical_mortar= ((texel_coord.x + ((bricks_row & 1) << 3)) & 15) == 3;

	vec4 color= (is_horisontal_mortar || is_vertical_mortar) ? c_mortar_color : c_brick_color;

	imageStore(out_image, texel_coord, color);
}
