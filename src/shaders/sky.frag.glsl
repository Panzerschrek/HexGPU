#version 450

layout(location = 0) in vec3 f_view_vec;

layout(location = 0) out vec4 out_color;

const vec3 c_sky_color= vec3(1.0, 1.0, 1.5);

void main()
{
	vec3 view_vec_normalized= normalize(f_view_vec);
	float l= (3.0 - view_vec_normalized.z) * 0.25;

	out_color= vec4(c_sky_color * l, 1.0);
}
