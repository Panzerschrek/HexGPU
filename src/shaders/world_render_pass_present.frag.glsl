#version 450

layout(binding= 0) uniform sampler2D frame_image;

layout(location= 0) out vec4 out_color;

void main()
{
	// Perform 2x2 filtering for dithering (if needed).
	// Such filtering still doesn't filter all dithering artefacts, but it's better than 4x4 box filter, which is too blurry without supersampling.

	ivec2 coord= ivec2(gl_FragCoord.xy);

	vec4 c0=
		texelFetch(frame_image, coord, 0);

	vec4 box_2x2_sum=
		c0 +
		texelFetch(frame_image, coord + ivec2(0, 1), 0) +
		texelFetch(frame_image, coord + ivec2(1, 0), 0) +
		texelFetch(frame_image, coord + ivec2(1, 1), 0);

	if(box_2x2_sum.a < 4.0)
		out_color= box_2x2_sum * 0.25;
	else
		out_color= c0;
}
