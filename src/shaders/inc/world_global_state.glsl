// This struct must match the same struct in C++ code!
struct WorldGlobalState
{
	vec4 sky_light_color;
	vec4 sky_color;
	vec4 sun_direction;
	vec4 clouds_color; // a - clouds edge
	vec4 base_fog_color;
};
