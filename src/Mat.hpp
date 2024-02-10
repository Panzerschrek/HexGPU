#pragma once
#include "Vec.hpp"


namespace HexGPU
{

class m_Mat3
{
public:
	// TODO - add other stuff.
	void MakeIdentity();
	m_Mat3 GetInverseMatrix() const;

	float value[9];
};

class m_Mat4
{
public:

	m_Mat4  operator* (const m_Mat4& m) const;
	m_Mat4& operator*=(const m_Mat4& m);
	m_Vec3  operator* (const m_Vec3& v) const;

	void MakeIdentity();
	void Transpose();
	void Inverse();

	void Translate(const m_Vec3& v);

	void Scale(float scale);
	void Scale(const m_Vec3& scale);

	void RotateX(float angle);
	void RotateY(float angle);
	void RotateZ(float angle);

	void PerspectiveProjection(float aspect, float fov_y, float z_near, float z_far);
	void AxonometricProjection(float scale_x, float scale_y, float z_near, float z_far);

	/*True matrix - this
	0   1   2   3
	4   5   6   7
	8   9   10  11
	12  13  14  15
	*/

	/*OpenGL matrix
	0   4   8   12
	1   5   9   13
	2   6   10  14
	3   7   11  15
	*/

	float value[16];
};

m_Vec3 operator*(const m_Vec3& v, const m_Mat4& m);

} // namespace HexGPU
