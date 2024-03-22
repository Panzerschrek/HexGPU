#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"
#include "inc/texture_gen_common.glsl"

const vec4 c_brick_color= vec4(0.65, 0.35, 0.32, 1.0);
const vec4 c_mortar_color= vec4(0.62, 0.62, 0.62, 1.0);

float BaseBrickNoise(ivec2 texel_coord)
{
	const int octaves= 4;

	int scaler= 0;
	for(int i= 0; i < octaves; ++i)
		scaler+= 65536 >> i;

	int noise_val=
		hex_TextureOctaveNoiseWraped(texel_coord.x, texel_coord.y * 2, 0, octaves, c_texture_size_log2);

	return float(noise_val) / float(scaler);
}

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	int bricks_row= texel_coord.y >> 3;
	int y_within_row= texel_coord.y & 7;

	bool is_horisontal_mortar= y_within_row == 0;

	int row_brick_size_log2= 4 + (bricks_row & 1); // Use different brick size in different rows.
	int bricks_in_current_row_log2= c_texture_size_log2 - row_brick_size_log2;
	int row_mask= (1 << row_brick_size_log2) - 1;
	int column_offset= (bricks_row & 1) << 3;

	bool is_vertical_mortar= ((texel_coord.x + column_offset) & row_mask) == 0;

	int bricks_column= ((texel_coord.x + column_offset) >> row_brick_size_log2) & ((1 << (bricks_in_current_row_log2)) - 1);

	float brick_pattern_noise= BaseBrickNoise(texel_coord + 256 * (bricks_column, bricks_row)) * 0.125 + 0.875;
	float individual_brick_brightness= float(hex_Noise2(bricks_column, bricks_row, 1454)) * (0.25 / 65536.0) + 0.75;

	vec4 brick_color= vec4(c_brick_color.rgb * (brick_pattern_noise * individual_brick_brightness), c_brick_color.a);

	vec4 color= (is_horisontal_mortar || is_vertical_mortar) ? c_mortar_color : brick_color;

	imageStore(out_image, texel_coord, color);
}
