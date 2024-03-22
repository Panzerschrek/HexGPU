#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

ivec3 GetCubeCoord(ivec2 coord)
{
	// See https://www.redblobgames.com/grids/hexagons/#conversions.
	int q= coord.x;
	int r= coord.y - ((coord.x + (coord.x & 1)) >> 1);
	return ivec3(q, r, -q - r);
}

int HexDiagonalDist(ivec2 from, ivec2 to)
{
	ivec3 from_cube= GetCubeCoord(from);
	ivec3 to_cube= GetCubeCoord(to);
	ivec3 diff= from_cube - to_cube;
	return max(abs(diff.x) + abs(diff.y), max(abs(diff.y) + abs(diff.z), abs(diff.z) + abs(diff.x)));
}

vec3 c_color_dark= vec3(0.60, 0.55, 0.40);
vec3 c_color_light= vec3(0.80, 0.75, 0.60);
vec3 c_color_bark= vec3(0.31, 0.20, 0.14);

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

	int min_dist= 65536;
	for(int i= 0; i < 3; ++i)
		for(int j= 0; j < 3; ++j)
			min_dist= min(min_dist, HexDiagonalDist(ivec2(texel_coord.x, y_shifted), centers[i] + center_offsets[j] + cell_center));

	float dist_fract= float(min_dist % 5) / 4.0;
	bool is_bark= min_dist > 29;

	// TODO -add some noise.
	vec3 ring_color= mix(c_color_dark, c_color_light, dist_fract);

	vec4 color= vec4(is_bark ? c_color_bark : ring_color, 1.0);

	imageStore(out_image, texel_coord, color);
}
