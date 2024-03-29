#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/constants.glsl"
#include "inc/world_rendering_constants.glsl"
#include "inc/world_shader_uniforms.glsl"

layout(binding= 0) uniform uniforms_block
{
	WorldShaderUniforms uniforms;
};

layout(location=0) in vec4 pos;
layout(location=1) in i16vec4 tex_coord;

layout(location= 0) out vec2 f_tex_coord;
layout(location= 1) out flat float f_fire_power;

void main()
{
	f_tex_coord= vec2(tex_coord.xy) * c_tex_coord_scale;

	// TODO - maybe use greater factor to show fire growing slower?
	f_fire_power= min(1.0, float(tex_coord.w) * (1.0 / float(c_min_fire_power_for_fire_to_spread)));

	gl_Position= uniforms.view_matrix * vec4(pos.xyz, 1.0);
}
