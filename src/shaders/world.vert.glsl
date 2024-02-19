#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/constants.glsl"

layout(push_constant) uniform uniforms_block
{
	mat4 view_matrix;
};

layout(location=0) in vec3 pos;
layout(location=1) in i16vec4 tex_coord;

layout(location= 0) out vec2 f_light;
layout(location= 1) out vec2 f_tex_coord;
layout(location= 2) out flat float f_tex_index;

// Texture coordnates are initialiiy somewhat scaled. Rescale them back.
const vec2 c_tex_coord_scale= vec2(1.0 / 8.0, 1.0 / 4.0);

void main()
{
	f_tex_coord= vec2(tex_coord.xy) * c_tex_coord_scale;
	f_tex_index= int(tex_coord.z);

	// Use linear light function - using such function allows to lit more area.
	f_light.x= float(tex_coord.w & c_fire_light_mask) / 9.0 + 0.05;
	f_light.y= float(tex_coord.w >> c_sky_light_shift) / 16.0 + 0.05;

	gl_Position= view_matrix * vec4(pos, 1.0);
}
