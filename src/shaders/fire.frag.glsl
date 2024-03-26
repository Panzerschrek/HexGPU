#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/world_common.glsl"
#include "inc/world_shader_uniforms.glsl"

layout(binding= 0) uniform uniforms_block
{
	WorldShaderUniforms uniforms;
};

layout(binding= 1) uniform sampler2D texture_image;

layout(location= 0) in vec2 f_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	vec4 tex_value= HexagonFetch(texture_image, f_tex_coord);

	out_color= vec4(tex_value.rgb, 1.0);
}
