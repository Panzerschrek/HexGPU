#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/world_global_state.glsl"
#include "inc/world_rendering_constants.glsl"

layout(push_constant) uniform uniforms_block
{
	float time_of_day; // In range 0 - 1
	float rain_intensity;
};

layout(binding= 0, std430) buffer world_global_state_buffer
{
	WorldGlobalState world_global_state;
};

void main()
{
	// Calculate sky direction.
	// TODO - improve this code, tune sun path in the sky.
	float sun_phase= (c_pi * 2.0) * time_of_day - c_pi * 0.5;
	vec3 sun_direction= vec3(cos(sun_phase), 0.0, sin(sun_phase));

	// Use sun elevation to calculate amout of light from the sky.
	const float c_twilight_below_horizon= -0.1;
	const float c_twilight_above_horizon= 0.05;

	float daynight_k= 0.0;
	if(sun_direction.z > c_twilight_above_horizon)
		daynight_k= 1.0;
	else if(sun_direction.z < c_twilight_below_horizon)
		daynight_k= 0.0;
	else
		daynight_k= (sun_direction.z - c_twilight_below_horizon) / (c_twilight_above_horizon - c_twilight_below_horizon);

	// Make overall light dimmer when there are a lot of clouds in the sky.
	float clouds_darkening_factor= 1.0 - 0.25 * rain_intensity;

	world_global_state.sky_light_color= vec4(clouds_darkening_factor * mix(c_sky_light_nighttime_color, c_sky_light_daytime_color, daynight_k), 0.0);

	world_global_state.sky_color= vec4(mix(c_nighttime_sky_color, c_daytime_sky_color, daynight_k), 0.0);

	world_global_state.sun_direction= vec4(sun_direction, 0.0);

	world_global_state.clouds_color.rgb= clouds_darkening_factor * mix(c_nightime_clouds_color, c_daytime_clouds_color, daynight_k);
	world_global_state.clouds_color.a= 1.0 - (rain_intensity * 0.7 + 0.25);
}
