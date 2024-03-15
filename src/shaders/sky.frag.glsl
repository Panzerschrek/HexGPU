#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/sky_shader_uniforms.glsl"

layout(binding= 0) uniform uniforms_block_variable
{
	SkyShaderUniforms uniforms;
};

layout(location= 0) in vec3 f_view_vec;

layout(location=  0) out vec4 out_color;

const vec3 c_sun_color= vec3(0.1, 0.1, 0.04);
const vec3 c_sun_direction= normalize(vec3(0.5, -0.3, 1.0)); // TODO - make daytime dependent.
const vec3 c_sky_color= vec3(1.0, 1.0, 1.5); // TODO - make daytime dependent.

void main()
{
	vec3 view_vec_normalized= normalize(f_view_vec);
	float l= (3.0 - view_vec_normalized.z) * 0.25;

	vec3 sky_color= l * uniforms.current_sky_color.rgb;

	// Use hyperbolic function for sun intensity.
	float sun_dir_angle_cos= min(1.0, dot(view_vec_normalized, c_sun_direction));
	float sun_factor= 0.01 / (1.0015 - sun_dir_angle_cos);

	vec3 result_color= sky_color + sun_factor * c_sun_color;

	out_color= vec4(result_color, 1.0);
}
