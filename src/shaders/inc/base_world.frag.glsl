#include "inc/dithering.glsl"
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

void main()
{
	vec4 tex_value= HexagonFetch(texture_image, vec3(f_tex_coord, f_tex_index));

	if(DITHER_FUNC(tex_value.a))
		discard;

	// tex_value.rgb= vec3(0.5, 0.5, 0.5);
	vec3 l= CombineLight(f_light.x * c_fire_light_color, f_light.y * uniforms.sky_light_color.rgb, c_ambient_light_color);
	out_color= vec4(l * tex_value.rgb, tex_value.a);
}
