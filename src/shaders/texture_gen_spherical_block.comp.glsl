#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

vec3 c_color_dark= vec3(0.1, 0.1, 0.1);
vec3 c_color_light= vec3(0.8, 0.8, 0.8);

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	const int c_cell_size_log2= c_texture_size_log2 - 2;
	const int c_cell_size= 1 << c_cell_size_log2;

	int column= texel_coord.x >> c_cell_size_log2;
	int y_shifted= texel_coord.y + (c_cell_size >> 1) - ((column & 1) << (c_cell_size_log2 - 1));
	int row= y_shifted >> c_cell_size_log2;

	ivec2 center_offsets[3]= ivec2[3](ivec2(1, 1), ivec2(1, 0), ivec2(0, 0));

	ivec2 centers[3]= ivec2[3](
		ivec2(column << c_cell_size_log2, row << c_cell_size_log2),
		ivec2((column - 1) << c_cell_size_log2, (row << c_cell_size_log2) + (c_cell_size >> 1)),
		ivec2((column - 1) << c_cell_size_log2, (row << c_cell_size_log2) - (c_cell_size >> 1)));

	const ivec2 cell_center= ivec2(20, 15);

	vec2 texel_coord_corrected= vec2(float(texel_coord.x), float(y_shifted) - 0.5 * (texel_coord.x & 1));

	float min_dist= 65536.0;
	for(int i= 0; i < 3; ++i)
	{
		vec2 vec_to_center= texel_coord_corrected - vec2(centers[i] + cell_center);
		min_dist= min(min_dist, int(length(vec_to_center * vec2(c_space_scale_x, 1.0))));
	}

	float circle_factor= smoothstep(0.2, 1.0, min_dist * (2.0 / float(c_cell_size)));

	vec3 circle_color= mix(c_color_light, c_color_dark, circle_factor);

	vec4 color= vec4(circle_color, 1.0);

	imageStore(out_image, texel_coord, color);
}
