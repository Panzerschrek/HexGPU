#pragma once
#include <cmath>
#include <cstddef>


namespace HexGPU
{

class m_Vec2
{
public:
	static constexpr size_t size= 2;

public:
	m_Vec2()= default;
	m_Vec2(const m_Vec2& v)= default;
	m_Vec2(float in_x, float in_y);

	float GetLength() const;
	float GetSquareLength() const;
	float GetInvLength() const;
	float GetInvSquareLength() const;

	m_Vec2 operator+() const;
	m_Vec2 operator-() const;

	m_Vec2  operator+ (const m_Vec2& v) const;
	m_Vec2& operator+=(const m_Vec2& v);
	m_Vec2  operator- (const m_Vec2& v) const;
	m_Vec2& operator-=(const m_Vec2& v);

	m_Vec2  operator* (float a) const;
	m_Vec2& operator*=(float a);
	m_Vec2  operator/ (float a) const;
	m_Vec2& operator/=(float a);

public:
	float x;
	float y;
};

bool operator==(const m_Vec2& v1, const m_Vec2& v2);
bool operator!=(const m_Vec2& v1, const m_Vec2& v2);

float mVec2Cross(const m_Vec2& v1, const m_Vec2 v2);

class m_Vec3
{
public:
	static constexpr size_t size= 3;

	m_Vec3()= default;
	m_Vec3(const m_Vec3& v)= default;
	m_Vec3(float in_x, float in_y, float in_z);
	m_Vec3(const m_Vec2& vec2, float in_z);

	float GetLength() const;
	float GetSquareLength() const;
	float GetInvLength() const;
	float GetInvSquareLength() const;

	m_Vec3 operator+() const;
	m_Vec3 operator-() const;

	m_Vec3  operator+ (const m_Vec3& v) const;
	m_Vec3& operator+=(const m_Vec3& v);
	m_Vec3  operator- (const m_Vec3& v) const;
	m_Vec3& operator-=(const m_Vec3& v);

	m_Vec3  operator* (float a) const;
	m_Vec3& operator*=(float a);
	m_Vec3  operator/ (float a) const;
	m_Vec3& operator/=(float a);

	m_Vec2 xy() const;
	m_Vec2 xz() const;
	m_Vec2 yz() const;

public:
	float x;
	float y;
	float z;
};

bool operator==(const m_Vec3& v1, const m_Vec3& v2);
bool operator!=(const m_Vec3& v1, const m_Vec3& v2);

float mVec3Dot(const m_Vec3& v1, const m_Vec3& v2);
m_Vec3 mVec3Cross(const m_Vec3& v1, const m_Vec3& v2);

/*
m_Vec2
*/

inline m_Vec2::m_Vec2(const float in_x, const float in_y)
	: x(in_x), y(in_y)
{}

inline float m_Vec2::GetLength() const
{
	return std::sqrt(GetSquareLength());

}
inline float m_Vec2::GetSquareLength() const
{
	return x * x + y * y;
}

inline float m_Vec2::GetInvLength() const
{
	return 1.0f / GetLength();
}

inline float m_Vec2::GetInvSquareLength() const
{
	return 1.0f / GetSquareLength();
}

inline m_Vec2 m_Vec2::operator+() const
{
	return *this;
}

inline m_Vec2 m_Vec2::operator-() const
{
	return m_Vec2(-x, -y);
}

inline m_Vec2 m_Vec2::operator+(const m_Vec2& v) const
{
	return m_Vec2(x + v.x, y + v.y);
}

inline m_Vec2& m_Vec2::operator+=(const m_Vec2& v)
{
	x+= v.x;
	y+= v.y;
	return *this;
}

inline m_Vec2 m_Vec2::operator-(const m_Vec2& v) const
{
	return m_Vec2(x - v.x, y - v.y);
}

inline m_Vec2& m_Vec2::operator-=(const m_Vec2& v)
{
	x-= v.x;
	y-= v.y;
	return *this;
}

inline m_Vec2  m_Vec2::operator*(const float a) const
{
	return m_Vec2(x * a, y * a);
}

inline m_Vec2& m_Vec2::operator*=(const float a)
{
	x*= a;
	y*= a;
	return *this;
}

inline m_Vec2 m_Vec2::operator/(const float a) const
{
	return m_Vec2(x / a, y / a);
}

inline m_Vec2& m_Vec2::operator/=(const float a)
{
	x/= a;
	y/= a;
	return *this;
}

inline bool operator==(const m_Vec2& v0, const m_Vec2& v1)
{
	return v0.x == v1.x && v0.y == v1.y;
}

inline bool operator!=(const m_Vec2& v0, const m_Vec2& v1)
{
	return !(v0 == v1);
}

inline float mVec2Cross(const m_Vec2& v0, const m_Vec2& v1)
{
	return v0.x * v1.y - v0.y * v1.x;
}

/*
m_Vec3
*/

inline m_Vec3::m_Vec3(const float in_x, const float in_y, const float in_z)
	: x(in_x), y(in_y), z(in_z)
{}

inline m_Vec3::m_Vec3(const m_Vec2& vec2, const float in_z)
	: x(vec2.x), y(vec2.y), z(in_z)
{}

inline float m_Vec3::GetLength() const
{
	return std::sqrt(GetSquareLength());
}

inline float m_Vec3::GetSquareLength() const
{
	return x * x + y * y + z * z;
}

inline float m_Vec3::GetInvLength() const
{
	return 1.0f / GetLength();
}

inline float m_Vec3::GetInvSquareLength() const
{
	return 1.0f / GetInvSquareLength();
}

inline m_Vec3 m_Vec3::operator+() const
{
	return *this;
}

inline m_Vec3 m_Vec3::operator-() const
{
	return m_Vec3(-x, -y, -z);
}

inline m_Vec3 m_Vec3::operator+(const m_Vec3& v) const
{
	return m_Vec3(x + v.x, y + v.y, z + v.z);
}

inline m_Vec3& m_Vec3::operator+=(const m_Vec3& v)
{
	x+= v.x;
	y+= v.y;
	z+= v.z;
	return *this;
}

inline m_Vec3 m_Vec3::operator-(const m_Vec3& v) const
{
	return m_Vec3(x - v.x, y - v.y, z - v.z);
}

inline m_Vec3& m_Vec3::operator-=(const m_Vec3& v)
{
	x-= v.x;
	y-= v.y;
	z-= v.z;
	return *this;
}

inline m_Vec3 m_Vec3::operator*(const float a) const
{
	return m_Vec3(x * a, y * a, z * a);
}

inline m_Vec3& m_Vec3::operator*=(const float a)
{
	x*= a;
	y*= a;
	z*= a;
	return *this;
}

inline m_Vec3 m_Vec3::operator/(const float a) const
{
	return m_Vec3( x / a, y / a, z / a);
}

inline m_Vec3& m_Vec3::operator/=(float a)
{
	x/= a;
	y/= a;
	z/= a;
	return *this;
}

inline m_Vec2 m_Vec3::xy() const
{
	return m_Vec2(x, y);
}

inline m_Vec2 m_Vec3::xz() const
{
	return m_Vec2(x, z);
}

inline m_Vec2 m_Vec3::yz() const
{
	return m_Vec2(y, z);
}

inline bool operator==(const m_Vec3& v0, const m_Vec3& v1)
{
	return v0.x == v0.x && v0.y == v1.y && v0.z == v1.z;
}

inline bool operator!=(const m_Vec3& v0, const m_Vec3& v1)
{
	return !(v0 == v1);
}

inline float mVec3Dot(const m_Vec3& v1, const m_Vec3& v2)
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

inline m_Vec3 mVec3Cross(const m_Vec3& v1, const m_Vec3& v2)
{
	return m_Vec3(
		v1.y * v2.z - v1.z * v2.y,
		v1.z * v2.x - v1.x * v2.z,
		v1.x * v2.y - v1.y * v2.x);
}

} // namespace HexGPU
