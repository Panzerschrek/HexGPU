struct SkyShaderUniforms
{
	mat4 view_matrix;
	mat4 stars_matrix;
	vec4 sky_color; // last component - overall brightness
	vec4 sun_direction; // should be normalized 3d vec
	vec4 clouds_color; // a - clouds edge
	float stars_brightness;
};
