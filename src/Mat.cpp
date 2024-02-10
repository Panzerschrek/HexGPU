#include "Mat.hpp"
#include <cstring>
#include <utility>


namespace HexGPU
{

namespace
{

float Mat3Det(
	const float m00, const float m10, const float m20,
	const float m01, const float m11, const float m21,
	const float m02, const float m12, const float m22)
{
	return
		m00 * (m11 * m22 - m21 * m12) -
		m10 * (m01 * m22 - m21 * m02) +
		m20 * (m01 * m12 - m11 * m02);
}

} // namespace

void m_Mat3::MakeIdentity()
{
	static constexpr float c_identity_data[9]
	{
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	};
	std::memcpy(value, c_identity_data, sizeof(c_identity_data));
}

m_Mat3 m_Mat3::GetInverseMatrix() const
{
	m_Mat3 r;

	const float det=
		value[0] * (value[4] * value[8] - value[7] * value[5]) -
		value[1] * (value[3] * value[8] - value[6] * value[5]) +
		value[2] * (value[3] * value[7] - value[6] * value[4]);

	if(det == 0.0f)
	{
		r.MakeIdentity();
		return r;
	}

	const float inv_det= 1.0f / det;

	r.value[0]= (value[4] * value[8] - value[7] * value[5]) * inv_det;
	r.value[1]= (value[2] * value[7] - value[1] * value[8]) * inv_det;
	r.value[2]= (value[1] * value[5] - value[2] * value[4]) * inv_det;

	r.value[3]= (value[5] * value[6] - value[3] * value[8]) * inv_det;
	r.value[4]= (value[0] * value[8] - value[2] * value[6]) * inv_det;
	r.value[5]= (value[2] * value[3] - value[0] * value[5]) * inv_det;

	r.value[6]= (value[3] * value[7] - value[4] * value[6]) * inv_det;
	r.value[7]= (value[1] * value[6] - value[0] * value[7]) * inv_det;
	r.value[8]= (value[0] * value[4] - value[1] * value[3]) * inv_det;

	return r;
}

m_Mat4 m_Mat4::operator*(const m_Mat4& m) const
{
	m_Mat4 r;

	for(size_t i= 0u; i < 16u; i+= 4u)
	for(size_t j= 0u; j < 4u; ++j)
	{
		r.value[i + j]=
			value[i  ] * m.value[j   ] +
			value[i+1] * m.value[j+ 4] +
			value[i+2] * m.value[j+ 8] +
			value[i+3] * m.value[j+12];
	}

	return r;
}

m_Mat4& m_Mat4::operator*=(const m_Mat4& m)
{
	*this= (*this) * m;
	return *this;
}

m_Vec3 m_Mat4::operator*(const m_Vec3& v) const
{
	return m_Vec3(
		value[0] * v.x + value[1] * v.y + value[2] * v.z + value[3],
		value[4] * v.x + value[5] * v.y + value[6] * v.z + value[7],
		value[8] * v.x + value[9] * v.y + value[10]* v.z + value[11]);
}

void m_Mat4::MakeIdentity()
{
	static constexpr float c_identity_data[16u]=
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	std::memcpy(value, c_identity_data, sizeof(c_identity_data));
}

void m_Mat4::Transpose()
{
	std::swap(value[ 1], value[ 4]);
	std::swap(value[ 2], value[ 8]);
	std::swap(value[ 6], value[ 9]);
	std::swap(value[ 3], value[12]);
	std::swap(value[ 7], value[13]);
	std::swap(value[11], value[14]);
}

void m_Mat4::Inverse()
{
	m_Mat4 m;

	m.value[0 ]= +Mat3Det(
	value[5 ], value[6 ], value[7 ],
	value[9 ], value[10], value[11],
	value[13], value[14], value[15]);//
	m.value[1 ]= -Mat3Det(
	value[4 ], value[6 ], value[7 ],
	value[8 ], value[10], value[11],
	value[12], value[14], value[15]);//
	m.value[2 ]= +Mat3Det(
	value[4 ], value[5 ], value[7 ],
	value[8 ], value[9 ], value[11],
	value[12], value[13], value[15]);//
	m.value[3 ]= -Mat3Det(
	value[4 ], value[5 ], value[6 ],
	value[8 ], value[9 ], value[10],
	value[12], value[13], value[14]);//

	m.value[4 ]= -Mat3Det(
	value[1 ], value[2 ], value[3 ],
	value[9 ], value[10], value[11],
	value[13], value[14], value[15]);//
	m.value[5 ]= +Mat3Det(
	value[0 ], value[2 ], value[3 ],
	value[8 ], value[10], value[11],
	value[12], value[14], value[15]);//
	m.value[6 ]= -Mat3Det(
	value[0 ], value[1 ], value[3 ],
	value[8 ], value[9 ], value[11],
	value[12], value[13], value[15]);//
	m.value[7 ]= +Mat3Det(
	value[0 ], value[1 ], value[2 ],
	value[8 ], value[9 ], value[10],
	value[12], value[13], value[14]);//

	m.value[8 ]= +Mat3Det(
	value[1 ], value[2 ], value[3 ],
	value[5 ], value[6 ], value[7 ],
	value[13], value[14], value[15]);//
	m.value[9 ]= -Mat3Det(
	value[0 ], value[2 ], value[3 ],
	value[4 ], value[6 ], value[7 ],
	value[12], value[14], value[15]);//
	m.value[10]= +Mat3Det(
	value[0 ], value[1 ], value[3 ],
	value[4 ], value[5 ], value[7 ],
	value[12], value[13], value[15]);//
	m.value[11]= -Mat3Det(
	value[0 ], value[1 ], value[2 ],
	value[4 ], value[5 ], value[6 ],
	value[12], value[13], value[14]);//

	m.value[12]= -Mat3Det(
	value[1 ], value[2 ], value[3 ],
	value[5 ], value[6 ], value[7 ],
	value[9 ], value[10], value[11]);//
	m.value[13]= +Mat3Det(
	value[0 ], value[2 ], value[3 ],
	value[4 ], value[6 ], value[7 ],
	value[8 ], value[10], value[11]);//
	m.value[14]= -Mat3Det(
	value[0 ], value[1 ], value[3 ],
	value[4 ], value[5 ], value[7 ],
	value[8 ], value[9 ], value[11]);//
	m.value[15]= +Mat3Det(
	value[0 ], value[1 ], value[2 ],
	value[4 ], value[5 ], value[6 ],
	value[8 ], value[9 ], value[10]);//

	const float det= m.value[0] * value[0] + m.value[1] * value[1] + m.value[2] * value[2] + m.value[3] * value[3];
	const float inv_det= 1.0f / det;

	value[0 ]= m.value[0 ] * inv_det;
	value[5 ]= m.value[5 ] * inv_det;
	value[10]= m.value[10] * inv_det;
	value[15]= m.value[15] * inv_det;

	value[1 ]= m.value[4 ] * inv_det;
	value[2 ]= m.value[8 ] * inv_det;
	value[3 ]= m.value[12] * inv_det;
	value[4 ]= m.value[1 ] * inv_det;
	value[6 ]= m.value[9 ] * inv_det;
	value[7 ]= m.value[13] * inv_det;
	value[8 ]= m.value[2 ] * inv_det;
	value[9 ]= m.value[6 ] * inv_det;
	value[11]= m.value[14] * inv_det;
	value[12]= m.value[3 ] * inv_det;
	value[13]= m.value[7 ] * inv_det;
	value[14]= m.value[11] * inv_det;
}

void m_Mat4::Translate(const m_Vec3& v)
{
	MakeIdentity();
	value[12]= v.x;
	value[13]= v.y;
	value[14]= v.z;
}

void m_Mat4::Scale(float scale)
{
	MakeIdentity();
	value[0 ]= scale;
	value[5 ]= scale;
	value[10]= scale;
}

void m_Mat4::Scale(const m_Vec3& scale)
{
	MakeIdentity();
	value[0 ]= scale.x;
	value[5 ]= scale.y;
	value[10]= scale.z;
}

void m_Mat4::RotateX(const float angle)
{
	const float s= std::sin(angle);
	const float c= std::cos(angle);

	value[5 ]= c;
	value[9 ]= -s;
	value[6 ]= s;
	value[10]= c;

	value[0]= value[15]= 1.0f;
	value[1]= value[2]= value[3]= 0.0f;
	value[4]= value[7]= 0.0f;
	value[8]= value[11]= 0.0f;
	value[12]= value[13]= value[14]= 0.0f;
}

void m_Mat4::RotateY(const float angle)
{
	const float s= std::sin(angle);
	const float c= std::cos(angle);

	value[0]= c;
	value[8]= s;
	value[2]= -s;
	value[10]= c;

	value[5]= value[15]= 1.0f;
	value[1]= value[3]= 0.0f;
	value[4]= value[6]= value[7]= 0.0f;
	value[9]= value[11]= 0.0f;
	value[12]= value[13]= value[14]= 0.0f;
}

void m_Mat4::RotateZ(const float angle)
{
	const float s= std::sin(angle);
	const float c= std::cos(angle);

	value[0]= c;
	value[4]= -s;
	value[1]= s;
	value[5]= c;

	value[10]= value[15]= 1.0f;
	value[2]= value[3]= 0.0f;
	value[6]= value[7]= 0.0f;
	value[8]= value[9]= value[11]= 0.0f;
	value[12]= value[13]= value[14]= 0.0f;
}

void m_Mat4::PerspectiveProjection(const float aspect, const float fov_y, const float z_near, const float z_far)
{
	const float inv_half_fov_tan= 1.0f / std::tan(fov_y * 0.5f);

	value[0]= inv_half_fov_tan / aspect;
	value[5]= inv_half_fov_tan;

	// Vulkan depth range [0; 1]
	value[14]= 1.0f / (1.0f / z_far - 1.0f / z_near);
	value[10]= 1.0f / (1.0f - z_near / z_far);
	value[11]= 1.0f;

	value[1]= value[2]= value[3]= 0.0f;
	value[4]= value[6]= value[7]= 0.0f;
	value[8]= value[9]= 0.0f;
	value[12]= value[13]= value[15]= 0.0f;
}

void m_Mat4::AxonometricProjection(const float scale_x, const float scale_y, const float z_near, const float z_far)
{
	value[0]= scale_x;
	value[5]= scale_y;
	value[10]= 2.0f / (z_far - z_near);
	value[14]= 1.0f - value[10] * z_far;

	value[1 ]= value[2 ]= value[3 ]= 0.0f;
	value[4 ]= value[6 ]= value[7 ]= 0.0f;
	value[8 ]= value[9 ]= value[11]= 0.0f;
	value[12]= value[13]= value[15]= 0.0f;
	value[15]= 1.0f;
}

m_Vec3 operator*(const m_Vec3& v, const m_Mat4& m)
{
	return m_Vec3(
		v.x * m.value[0] + v.y * m.value[4] + v.z * m.value[ 8] + m.value[12],
		v.x * m.value[1] + v.y * m.value[5] + v.z * m.value[ 9] + m.value[13],
		v.x * m.value[2] + v.y * m.value[6] + v.z * m.value[10] + m.value[14]);
}

} // namespace HexGPU
