#include "inc/dithering.glsl"
#include "inc/world_common.glsl"
#include "inc/world_rendering_constants.glsl"
#include "inc/world_shader_uniforms.glsl"

layout(binding= 0) uniform uniforms_block
{
	WorldShaderUniforms uniforms;
};

layout(binding= 1) uniform sampler2D texture_image;

layout(location= 0) in vec2 f_light;
layout(location= 1) in vec2 f_tex_coord;
layout(location= 2) in vec3 f_fog_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	vec4 tex_value= HexagonFetch(texture_image, f_tex_coord);

	if(DITHER_FUNC(tex_value.a))
		discard;

	vec3 l= CombineLight(f_light.x * c_fire_light_color, f_light.y * uniforms.sky_light_color.rgb, c_ambient_light_color);
	vec3 self_color= l * tex_value.rgb;

	float fog_factor= CalculateFogFactor(f_fog_coord);
	vec3 color_with_fog= mix(self_color, uniforms.fog_color.rgb, fog_factor);

	out_color= vec4(color_with_fog, tex_value.a);
}
