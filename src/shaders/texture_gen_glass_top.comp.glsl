#version 450

#extension GL_GOOGLE_include_directive : require

#define GLASS_COLOR vec3(0.8, 0.8, 0.8)

#include "inc/constants.glsl"
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

	vec2 texel_coord_f= vec2(float(texel_coord.x), float(texel_coord.y) - 0.5 * float(texel_coord.x & 1));

	float frequency= c_pi / 16.0;
	float s= sin((texel_coord_f.x + texel_coord_f.y * 2.0) * frequency);

	vec4 color= vec4(GLASS_COLOR * (0.75 + 0.25 * s), 0.45 + 0.15 * s);

	if(min_dist > 29)
		color= vec4(GLASS_COLOR, 1.0);

	imageStore(out_image, texel_coord, color);
}
