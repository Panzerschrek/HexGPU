bool AlphaDither2x2(float alpha)
{
	const float[2][2] c_dither_matrix=
	float[2][2]
	(
		float[2](0.0 / 4.0, 2.0 / 4.0),
		float[2](3.0 / 4.0, 1.0 / 4.0)
	);

	ivec2 dither_coord= ivec2(gl_FragCoord.xy) & 1;

	return alpha <= c_dither_matrix[dither_coord.x][dither_coord.y];
}

bool AlphaDither4x4(float alpha)
{
	const float[4][4] c_dither_matrix=
	float[4][4]
	(
		float[4]( 0.0 / 16.0,  8.0 / 16.0,  2.0 / 16.0, 10.0 / 16.0),
		float[4](12.0 / 16.0,  4.0 / 16.0, 14.0 / 16.0,  6.0 / 16.0),
		float[4]( 3.0 / 16.0, 11.0 / 16.0,  1.0 / 16.0,  9.0 / 16.0),
		float[4](15.0 / 16.0,  7.0 / 16.0, 13.0 / 16.0,  5.0 / 16.0 )
	);

	ivec2 dither_coord= ivec2(gl_FragCoord.xy) & 3;

	return alpha <= c_dither_matrix[dither_coord.x][dither_coord.y];
}
