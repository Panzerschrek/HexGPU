struct SkyShaderUniforms
{
	mat4 view_matrix;
	vec4 sky_color;
	vec4 sun_direction; // should be normalized 3d vec
};
