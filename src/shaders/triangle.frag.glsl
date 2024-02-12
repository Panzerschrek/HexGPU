#version 450

layout(location= 0) in vec4 f_color;
layout(location= 1) in vec2 f_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	vec2 fract_lo= fract(f_tex_coord);
	vec2 fract_hi= fract(f_tex_coord / 8.0);
	out_color= vec4(fract_lo, abs(fract_hi.x - fract_hi.y), 1.0);
}
