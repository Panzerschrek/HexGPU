#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/world_common.glsl"
#include "inc/world_rendering_constants.glsl"
#include "inc/world_shader_uniforms.glsl"

layout(binding= 0) uniform uniforms_block
{
	WorldShaderUniforms uniforms;
};

layout(binding= 1) uniform sampler2DArray texture_image;

layout(location= 0) in vec2 f_light;
layout(location= 1) in vec2 f_tex_coord;
layout(location= 2) in flat float f_tex_index;

layout(location = 0) out vec4 out_color;

const float[4][4] c_dither_matrix=
float[4][4]
(
	float[4]( 0.0 / 16.0,  8.0 / 16.0,  2.0 / 16.0, 10.0 / 16.0),
	float[4](12.0 / 16.0,  4.0 / 16.0, 14.0 / 16.0,  6.0 / 16.0),
	float[4]( 3.0 / 16.0, 11.0 / 16.0,  1.0 / 16.0,  9.0 / 16.0),
	float[4](15.0 / 16.0,  7.0 / 16.0, 13.0 / 16.0,  5.0 / 16.0 )
);

void main()
{
	vec4 tex_value= HexagonFetch(texture_image, vec3(f_tex_coord, f_tex_index));

	ivec2 dither_coord= ivec2(gl_FragCoord.xy) & 3;
	if(tex_value.a < c_dither_matrix[dither_coord.x][dither_coord.y])
		discard;

	// tex_value.rgb= vec3(0.5, 0.5, 0.5);
	vec3 l= CombineLight(f_light.x * c_fire_light_color, f_light.y * uniforms.sky_light_color.rgb, c_ambient_light_color);
	out_color= vec4(l * tex_value.rgb, 1.0);
}
