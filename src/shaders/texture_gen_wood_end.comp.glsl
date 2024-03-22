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

vec3 c_color_dark= vec3(0.56, 0.53, 0.44);
vec3 c_color_light= vec3(0.73, 0.68, 0.55);
vec3 c_color_bark= vec3(0.47, 0.35, 0.18);

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	// TODO - fill the whole texture.

	ivec2 centers[3]= ivec2[3](ivec2(21, 16), ivec2(21, 15), ivec2(20, 15));
	int dists[3]= int[3](
		HexDiagonalDist(texel_coord, centers[0]),
		HexDiagonalDist(texel_coord, centers[1]),
		HexDiagonalDist(texel_coord, centers[2]));

	int min_dist= min(dists[0], min(dists[1], dists[2]));
	float dist_fract= float(min_dist % 5) / 4.0;
	bool is_bark= min_dist > 29;

	vec3 ring_color= mix(c_color_dark, c_color_light, dist_fract);

	vec4 color= vec4(is_bark ? c_color_bark : ring_color, 1.0);
	//color= vec4(vec2(texel_coord) / 128.0, 0.0, 1.0);

	imageStore(out_image, texel_coord, color);
}
