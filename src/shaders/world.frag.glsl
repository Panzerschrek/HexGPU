#version 450

layout(binding= 1) uniform sampler2DArray texture_image;

layout(location= 0) in vec2 f_light;
layout(location= 1) in vec2 f_tex_coord;
layout(location= 2) in flat float f_tex_index;

layout(location = 0) out vec4 out_color;

const vec3 c_fire_light_color= vec3(1.4, 1.2, 0.8);
const vec3 c_sky_light_color= vec3(0.95, 0.95, 1.0); // TODO - make depending on daytime, moon visibility, weather, etc.
// const vec3 c_sky_light_color= vec3(0.2, 0.2, 0.2);
const vec3 c_ambient_light_color= vec3(0.06f, 0.06f, 0.06f);

vec3 CombineLight(vec3 fire_light, vec3 sky_light, vec3 ambient_light)
{
	// Use special formula to make one light source weaker when other like source value is big.
	const vec3 c_one= vec3(1.0, 1.0, 1.0);
	const float c_mul= 0.28;
	return
		fire_light * (c_one - c_mul * sky_light) +
		sky_light * (c_one - c_mul * fire_light) +
		ambient_light;
}

void main()
{
	vec4 tex_value= texture(texture_image, vec3(f_tex_coord, f_tex_index));

	if(tex_value.a < 0.5)
		discard;

	// tex_value.rgb= vec3(0.5, 0.5, 0.5);
	vec3 l= CombineLight(f_light.x * c_fire_light_color, f_light.y * c_sky_light_color, c_ambient_light_color);
	out_color= vec4(l * tex_value.rgb, 1.0);
}
