#pragma once
#include "Mat.hpp"
#include <vector>


namespace HexGPU
{

class CameraController final
{
public:
	CameraController(float aspect);

	void SetAspect(float aspect);

	void Update(float time_delta_s, const std::vector<bool>& keyboard_state);

	// Returns rotation + aspect
	m_Mat4 CalculateViewMatrix() const;
	m_Mat4 CalculateFullViewMatrix() const;
	m_Vec3 GetCameraPosition() const;
	m_Vec3 GetCameraDirection() const;
	m_Vec2 GetCameraAngles() const;

private:
	float aspect_;

	m_Vec3 pos_= m_Vec3(0.0f, 0.0f, 32.0f);
	float azimuth_= 0.0f;
	float elevation_= 0.0f;
};

} // namespace HexGPU
