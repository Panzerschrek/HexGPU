#version 450

layout(binding= 0) uniform sampler2D frame_image;

layout(location= 0) out vec4 out_color;

void main()
{
	ivec2 coord= ivec2(gl_FragCoord.xy);
	vec4 c0= texelFetch(frame_image, coord, 0);
	/*
	vec4 average= 0.0625 *(
		texelFetch(frame_image, coord + ivec2(-1, -1), 0) +
		texelFetch(frame_image, coord + ivec2(-1,  0), 0) +
		texelFetch(frame_image, coord + ivec2(-1,  1), 0) +
		texelFetch(frame_image, coord + ivec2(-1,  2), 0) +
		texelFetch(frame_image, coord + ivec2( 0, -1), 0) +
		texelFetch(frame_image, coord + ivec2( 0,  0), 0) +
		texelFetch(frame_image, coord + ivec2( 0,  1), 0) +
		texelFetch(frame_image, coord + ivec2( 0,  2), 0) +
		texelFetch(frame_image, coord + ivec2( 1, -1), 0) +
		texelFetch(frame_image, coord + ivec2( 1,  0), 0) +
		texelFetch(frame_image, coord + ivec2( 1,  1), 0) +
		texelFetch(frame_image, coord + ivec2( 1,  2), 0) +
		texelFetch(frame_image, coord + ivec2( 2, -1), 0) +
		texelFetch(frame_image, coord + ivec2( 2,  0), 0) +
		texelFetch(frame_image, coord + ivec2( 2,  1), 0) +
		texelFetch(frame_image, coord + ivec2( 2,  2), 0));
	*/
	vec4 average= 0.24 *(
		c0 +
		texelFetch(frame_image, coord + ivec2(0, 0), 0) +
		texelFetch(frame_image, coord + ivec2(1, 0), 0) +
		texelFetch(frame_image, coord + ivec2(1, 1), 0));

	if(average.a < 1.0)
		out_color= average;
	else
		out_color= c0;
}
