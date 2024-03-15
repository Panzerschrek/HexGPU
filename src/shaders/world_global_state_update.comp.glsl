#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/world_global_state.glsl"

layout(binding= 0, std430) buffer world_global_state_buffer
{
	WorldGlobalState world_global_state;
};

const vec3 c_daytime_sky_color= vec3(1.0, 1.0, 1.5);

void main()
{
	world_global_state.current_sky_color= vec4(c_daytime_sky_color, 0.0);
}
