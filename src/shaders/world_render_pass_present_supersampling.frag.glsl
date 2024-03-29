#version 450

layout(binding= 0) uniform sampler2D frame_image;

layout(location= 0) out vec4 out_color;

void main()
{
	// Perform 2x dowsampling and 4x4 box filtering for dithering (if needed).

	ivec2 coord= ivec2(gl_FragCoord.xy) << 1;

	vec4 box_2x2_sum=
		texelFetch(frame_image, coord + ivec2(0, 0), 0) +
		texelFetch(frame_image, coord + ivec2(0, 1), 0) +
		texelFetch(frame_image, coord + ivec2(1, 0), 0) +
		texelFetch(frame_image, coord + ivec2(1, 1), 0);

	vec4 box_4x4_sum=
		box_2x2_sum +
		texelFetch(frame_image, coord + ivec2(-1, -1), 0) +
		texelFetch(frame_image, coord + ivec2(-1,  0), 0) +
		texelFetch(frame_image, coord + ivec2(-1,  1), 0) +
		texelFetch(frame_image, coord + ivec2(-1,  2), 0) +
		texelFetch(frame_image, coord + ivec2( 0, -1), 0) +
		texelFetch(frame_image, coord + ivec2( 0,  2), 0) +
		texelFetch(frame_image, coord + ivec2( 1, -1), 0) +
		texelFetch(frame_image, coord + ivec2( 1,  2), 0) +
		texelFetch(frame_image, coord + ivec2( 2, -1), 0) +
		texelFetch(frame_image, coord + ivec2( 2,  0), 0) +
		texelFetch(frame_image, coord + ivec2( 2,  1), 0) +
		texelFetch(frame_image, coord + ivec2( 2,  2), 0);

	if(box_4x4_sum.a < 16.0)
		out_color= box_4x4_sum * 0.0625;
	else
		out_color= box_2x2_sum * 0.25;
}
