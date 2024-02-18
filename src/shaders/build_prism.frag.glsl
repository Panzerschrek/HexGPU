#version 450

layout(location = 0) in float f_alpha;

layout(location = 0) out vec4 out_color;

void main()
{
	out_color= vec4(0.1, 0.2, 0.3, f_alpha);
}
