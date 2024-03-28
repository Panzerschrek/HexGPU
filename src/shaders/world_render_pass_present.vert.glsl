#version 450

const vec2 vertices[3]= vec2[3]( vec2(-5.0, -2.0), vec2(5.0, -2.0), vec2(0.0, 3.0));

void main()
{
	vec2 pos= vertices[gl_VertexIndex];
	gl_Position=vec4(pos, 0.0, 1.0);
}
