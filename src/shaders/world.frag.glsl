#version 450

layout(binding= 1) uniform sampler2DArray texture_image;

layout(location= 0) in vec2 f_light;
layout(location= 1) in vec2 f_tex_coord;
layout(location= 2) in flat float f_tex_index;

layout(location = 0) out vec4 out_color;

void main()
{
	vec4 tex_value= texture(texture_image, vec3(f_tex_coord, f_tex_index));
	// tex_value.rgb= vec3(0.5, 0.5, 0.5);
	// TODO - use different scales for fire and sky light.
	vec3 total_light= vec3(1.0, 1.0, 1.0) * f_light.x + vec3(1.0, 1.0, 1.0) * f_light.y;
	out_color= vec4(total_light * tex_value.rgb, 1.0);
}
