#version 450

layout(binding= 0) uniform sampler2D frame_image;

layout(location= 0) out vec4 out_color;

void main()
{
	// Perform 2x dowsampling and 4x4 box filtering for dithering (if needed).

	ivec2 coord= ivec2(gl_FragCoord.xy) << 1;

	vec4 average= vec4(0);
	for(int dx= 0; dx < 4; ++dx)
	for(int dy= 0; dy < 4; ++dy)
		average+= texelFetch(frame_image, coord + ivec2(dx - 1, dy - 1), 0);
	average/= 16.0;

	if(average.a < 1.0)
		out_color= average;
	else
	{
		out_color= 0.25 *(
			texelFetch(frame_image, coord + ivec2(0, 0), 0) +
			texelFetch(frame_image, coord + ivec2(0, 1), 0) +
			texelFetch(frame_image, coord + ivec2(1, 0), 0) +
			texelFetch(frame_image, coord + ivec2(1, 1), 0));
	}
}
