// This file contains helper functions for matrices manipulations.

mat4 MakePerspectiveProjectionMatrix(float aspect, float fov_y, float z_near, const float z_far)
{
	const float inv_half_fov_tan= 1.0 / tan(fov_y * 0.5);

	mat4 m= mat4(0.0);

	m[0][0]= inv_half_fov_tan / aspect;
	m[1][1]= inv_half_fov_tan;

	// Vulkan depth range [0; 1]
	m[3][2]= 1.0 / (1.0 / z_far - 1.0 / z_near);
	m[2][2]= 1.0 / (1.0 - z_near / z_far);
	m[2][3]= 1.0;

	m[0][1]= m[0][2]= m[0][3]= 0.0;
	m[1][0]= m[1][2]= m[1][3]= 0.0;
	m[2][0]= m[2][1]= 0.0;
	m[3][0]= m[3][1]= m[3][3]= 0.0;

	return m;
}

mat4 MakePerspectiveChangeBasisMatrix()
{
	mat4 m= mat4(1.0); // identity.
	m[1][1]= 0.0;
	m[1][2]= 1.0;
	m[2][1]= -1.0;
	m[2][2]= 0.0;
	return m;
}

mat4 MateTranslateMatrix(vec3 shift)
{
	mat4 m= mat4(1.0); // identity.
	m[3][0]= shift.x;
	m[3][1]= shift.y;
	m[3][2]= shift.z;
	return m;
}

mat4 MakeScaleMatrix(vec3 scale)
{
	mat4 m= mat4(1.0); // identity
	m[0][0]= scale.x;
	m[1][1]= scale.y;
	m[2][2]= scale.z;
	return m;
}

mat4 MakeRotationXMatrix(float angle)
{
	float s= sin(angle);
	float c= cos(angle);

	mat4 m;

	m[1][1]= c;
	m[2][1]= -s;
	m[1][2]= s;
	m[2][2]= c;

	m[0][0]= m[3][3]= 1.0;
	m[0][1]= m[0][2]= m[0][3]= 0.0;
	m[1][0]= m[1][3]= 0.0;
	m[2][0]= m[2][3]= 0.0;
	m[3][0]= m[3][1]= m[3][2]= 0.0;

	return m;
}

mat4 MakeRotationZMatrix(float angle)
{
	float s= sin(angle);
	float c= cos(angle);

	mat4 m;

	m[0][0]= c;
	m[1][0]= -s;
	m[0][1]= s;
	m[1][1]= c;

	m[2][2]= m[3][3]= 1.0;
	m[0][2]= m[0][3]= 0.0;
	m[1][2]= m[1][3]= 0.0;
	m[2][0]= m[2][1]= m[2][3]= 0.0;
	m[3][0]= m[3][1]= m[3][2]= 0.0;

	return m;
}
