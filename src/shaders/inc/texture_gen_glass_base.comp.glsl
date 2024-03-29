#include "inc/constants.glsl"
#include "inc/texture_gen_common.glsl"

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	vec2 texel_coord_f= vec2(float(texel_coord.x), float(texel_coord.y) - 0.5 * float(texel_coord.x & 1));

	float frequency= c_pi / 16.0;
	float s= sin((texel_coord_f.x + texel_coord_f.y * 2.0) * frequency);

	vec4 color= vec4(GLASS_COLOR * (0.75 + 0.25 * s), 0.45 + 0.15 * s);

	const float x_step= float(1 << c_texture_size_log2) / 6.0;

	float grid_cell= float(texel_coord.x) / x_step;
	float grid_cell_fract= fract(grid_cell);
	if(grid_cell_fract < 1.0 / x_step || grid_cell_fract >= (1.0 - 1.0 / x_step))
		color= vec4(GLASS_COLOR, 1.0);

	int vertical_bar= texel_coord.y & 31;
	if(vertical_bar == 0 || vertical_bar == 31)
		color= vec4(GLASS_COLOR, 1.0);

	imageStore(out_image, texel_coord, color);
}
