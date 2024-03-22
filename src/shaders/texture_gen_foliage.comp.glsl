#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

const vec3 c_foliage_base_color= vec3(0.30, 0.70, 0.30);

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	const int cell_size_log2= 3;
	const int cell_size= 1 << cell_size_log2;
	const int c_num_cells_log2= c_texture_size_log2 - cell_size_log2;

	ivec2 cell_coord= texel_coord >> cell_size_log2;
	ivec2 coord_within_cell= texel_coord & ((cell_size_log2 << 1) - 1);

	ivec2 cell_center= ivec2(cell_size >> 1, cell_size >> 1);

	vec2 texel_coord_in_cell_units=
		vec2(float(texel_coord.x), float(texel_coord.y) - 0.5 * float(texel_coord.x & 1)) / float(cell_size);

	const float max_dist= 65536.0;
	float second_min_dist= max_dist;
	float min_dist= max_dist;
	ivec2 closest_cell_coord= ivec2(0, 0);
	for(int dx= -2; dx <= 1; ++dx)
	for(int dy= -2; dy <= 1; ++dy)
	{
		ivec2 adjacent_cell_coord= ivec2(cell_coord.x + dx, cell_coord.y + dy);
		ivec2 adjacent_cell_coord_wrapped= adjacent_cell_coord & ((1 << c_num_cells_log2) - 1);
		const ivec2 seed= ivec2(67, 54);
		ivec2 cell_noise=
			ivec2(
				hex_Noise2(adjacent_cell_coord_wrapped.x, adjacent_cell_coord_wrapped.y, seed.x),
				hex_Noise2(adjacent_cell_coord_wrapped.x, adjacent_cell_coord_wrapped.y, seed.y));

		const float amplitude= 0.4;

		vec2 cell_point_coord= vec2(adjacent_cell_coord) + vec2(0.5 - amplitude) + vec2(cell_noise) * (amplitude * 2.0 / 65536.0);

		vec2 vec_to_cell_point= texel_coord_in_cell_units - cell_point_coord;
		float dist= length(vec_to_cell_point);
		if(dist < min_dist)
		{
			closest_cell_coord= adjacent_cell_coord_wrapped;
			second_min_dist= min_dist;
			min_dist= dist;

			if(second_min_dist >= max_dist)
				second_min_dist= min_dist;
		}
		else if(dist < second_min_dist)
		{
			second_min_dist= dist;
		}
	}

	int closest_cell_noise= hex_Noise2(closest_cell_coord.x, closest_cell_coord.y, 8765);
	float closest_cell_brightness= float(closest_cell_noise) / 65536.0 * 0.25 + 0.75;

	float min_dist_diff= abs(second_min_dist - min_dist);
	float edge_factor= smoothstep(0.0, 2.0 / float(cell_size), min_dist_diff);

	vec4 color= vec4(c_foliage_base_color * closest_cell_brightness, edge_factor);

	imageStore(out_image, texel_coord, color);
}
