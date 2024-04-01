#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/world_rendering_constants.glsl"
#include "inc/world_shader_uniforms.glsl"

layout(binding= 0) uniform uniforms_block
{
	WorldShaderUniforms uniforms;
};

layout(location=0) in vec3 pos;
layout(location=1) in i16vec4 tex_coord;

layout(location= 0) out vec2 f_light;
layout(location= 1) out vec2 f_tex_coord;
layout(location= 2) out vec3 f_fog_coord;

void main()
{
	f_tex_coord= vec2(tex_coord.xy) * c_tex_coord_scale;

	// Normalize light [0; 255] -> [0; 1]
	const float c_light_scale= 1.0 / 255.0;
	f_light.x= float(int(tex_coord.w) & 0xFF) * c_light_scale;
	f_light.y= float(uint(uint16_t(tex_coord.w)) >> 8) * c_light_scale;

	vec4 pos_scaled= vec4(pos.x, pos.y, pos.z * 0.5, 1.0);

	f_fog_coord= (uniforms.fog_matrix * pos_scaled).xyz;

	gl_Position= uniforms.view_matrix * pos_scaled;
}
