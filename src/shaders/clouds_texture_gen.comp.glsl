#version 450

#extension GL_GOOGLE_include_directive : require

layout(binding= 0, rgba8ui) uniform writeonly uimage2D out_image;


void main()
{
	imageStore(out_image, ivec2(gl_GlobalInvocationID.xy), ivec4(255, 0, 255, 128));
}
