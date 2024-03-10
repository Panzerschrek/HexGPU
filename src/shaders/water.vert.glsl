#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/world_rendering_constants.glsl"

layout(binding= 0) uniform uniforms_block
{
	mat4 view_matrix;
};

layout(location=0) in vec3 pos;
layout(location=1) in i16vec4 tex_coord;

layout(location= 0) out vec2 f_light;
layout(location= 1) out vec2 f_tex_coord;

void main()
{
	f_tex_coord= vec2(tex_coord.xy) * c_tex_coord_scale;

	// Normalize light [0; 255] -> [0; 1]
	const float c_light_scale= 1.0 / 255.0;
	f_light.x= float(int(tex_coord.w) & 0xFF) * c_light_scale;
	f_light.y= float(uint(uint16_t(tex_coord.w)) >> 8) * c_light_scale;

	vec3 pos_corrected= pos * vec3(1.0, 1.0, 1.0 / 128.0); // Rescale water height back.
	gl_Position= view_matrix * vec4(pos_corrected, 1.0);
}
