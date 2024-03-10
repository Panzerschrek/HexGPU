#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/world_common.glsl"
#include "inc/world_rendering_constants.glsl"

layout(push_constant) uniform uniforms_block
{
	float water_phase;
};

layout(binding= 1) uniform sampler2DArray texture_image;

layout(location= 0) in vec2 f_light;
layout(location= 1) in vec2 f_tex_coord;
layout(location= 2) in flat float f_tex_index;

layout(location = 0) out vec4 out_color;

void main()
{
	vec2 tc= f_tex_coord + sin(f_tex_coord.yx * 8.0 + vec2(water_phase)) * 0.06125;

	vec4 tex_value= HexagonFetch(texture_image, vec3(tc, f_tex_index));

	if(tex_value.a < 0.5)
		discard;

	vec3 l= CombineLight(f_light.x * c_fire_light_color, f_light.y * c_sky_light_color, c_ambient_light_color);
	out_color= vec4(l * tex_value.rgb, 0.5);
}
