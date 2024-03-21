#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 8, local_size_y = 16, local_size_z= 1) in;

layout(binding= 0, rgba8) uniform writeonly image2D out_image;

const int c_texture_size_log2= 7; // This must match texture size in C++ code!

ivec3 GetCubeCoord(ivec2 coord)
{
	// See https://www.redblobgames.com/grids/hexagons/#conversions.
	int q= coord.x;
	int r= coord.y - ((coord.x + (coord.x & 1)) >> 1);
	return ivec3(q, r, -q - r);
}

int HexDist(ivec2 from, ivec2 to)
{
	// See https://www.redblobgames.com/grids/hexagons/#distances.
	ivec3 from_cube= GetCubeCoord(from);
	ivec3 to_cube= GetCubeCoord(to);
	const ivec3 diff= from_cube - to_cube;
	return (abs(diff.x) + abs(diff.y) + abs(diff.z)) >> 1;
}

vec3 c_color_dark= vec3(0.56, 0.53, 0.44);
vec3 c_color_light= vec3(0.73, 0.68, 0.55);
vec3 c_color_bark= vec3(0.47, 0.35, 0.18);

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	// TODO - fill the whole texture.

	ivec2 centers[3]= ivec2[3](ivec2(15, 15), ivec2(15, 16), ivec2(16, 15));

	int dists[3]= int[3](HexDist(texel_coord, centers[0]), HexDist(texel_coord, centers[1]), HexDist(texel_coord, centers[2]));
	int min_dist= min(dists[0], min(dists[1], dists[2]));

	float dist_fract= float(min_dist & 1);
	bool is_bark= min_dist > 14;

	vec3 ring_color= mix(c_color_dark, c_color_light, dist_fract);

	vec4 color= vec4(is_bark ? c_color_bark : ring_color, 1.0);
	//color= vec4(vec2(texel_coord) / 128.0, 0.0, 1.0);

	imageStore(out_image, texel_coord, color);
}
