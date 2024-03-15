#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/world_global_state.glsl"

layout(binding= 0, std430) buffer world_global_state_buffer
{
	WorldGlobalState world_global_state;
};

void main()
{
	world_global_state.current_sky_color= vec3(1.0, 0.0, 1.0);
}
