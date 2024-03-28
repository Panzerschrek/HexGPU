#version 450

layout(binding= 0) uniform sampler2D frame_image;

layout(location= 0) out vec4 out_color;

void main()
{
	out_color= texelFetch(frame_image, ivec2(gl_FragCoord.xy), 0);
}
