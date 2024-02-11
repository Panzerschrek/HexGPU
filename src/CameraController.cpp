#include "CameraController.hpp"
#include <SDL_keyboard.h>
#include <algorithm>


namespace HexGPU
{

namespace
{

const float g_pi= 3.1415926535f;

} // namespace

CameraController::CameraController(const float aspect)
	: aspect_(std::move(aspect))
{}

void CameraController::SetAspect(const float aspect)
{
	aspect_= aspect;
}

void CameraController::Update(const float time_delta_s, const std::vector<bool>& keyboard_state)
{
	const float speed= 4.0f;
	const float jump_speed= 0.8f * speed;
	const float angle_speed= 1.0f;

	const m_Vec3 forward_vector(-std::sin(azimuth_), +std::cos(azimuth_), 0.0f);
	const m_Vec3 left_vector(std::cos(azimuth_), std::sin(azimuth_), 0.0f);

	m_Vec3 move_vector(0.0f, 0.0f, 0.0f);

	if(keyboard_state[size_t(SDL_SCANCODE_W)])
		move_vector+= forward_vector;
	if(keyboard_state[size_t(SDL_SCANCODE_S)])
		move_vector-= forward_vector;
	if(keyboard_state[size_t(SDL_SCANCODE_D)])
		move_vector+= left_vector;
	if(keyboard_state[size_t(SDL_SCANCODE_A)])
		move_vector-= left_vector;

	const float move_vector_length= move_vector.GetLength();
	if(move_vector_length > 0.0f)
		pos_+= move_vector * (time_delta_s * speed / move_vector_length);

	if(keyboard_state[size_t(SDL_SCANCODE_SPACE)])
		pos_.z+= time_delta_s * jump_speed;
	if(keyboard_state[size_t(SDL_SCANCODE_C)])
		pos_.z-= time_delta_s * jump_speed;

	if(keyboard_state[size_t(SDL_SCANCODE_LEFT)])
		azimuth_+= time_delta_s * angle_speed;
	if(keyboard_state[size_t(SDL_SCANCODE_RIGHT)])
		azimuth_-= time_delta_s * angle_speed;

	if(keyboard_state[size_t(SDL_SCANCODE_UP)])
		elevation_+= time_delta_s * angle_speed;
	if(keyboard_state[size_t(SDL_SCANCODE_DOWN)])
		elevation_-= time_delta_s * angle_speed;

	while(azimuth_ > +g_pi)
		azimuth_-= 2.0f * g_pi;
	while(azimuth_ < -g_pi)
		azimuth_+= 2.0f * g_pi;

	elevation_= std::max(-0.5f * g_pi, std::min(elevation_, +0.5f * g_pi));
}

m_Mat4 CameraController::CalculateViewMatrix() const
{
	const float fov_deg= 75.0;

	const float fov= fov_deg * (g_pi / 180.0f);

	const float z_near= 0.125f;
	const float z_far= 128.0f;

	m_Mat4 rotate_z, rotate_x, perspective, basis_change;
	rotate_x.RotateX(-elevation_);
	rotate_z.RotateZ(-azimuth_);

	basis_change.MakeIdentity();
	basis_change.value[5]= 0.0f;
	basis_change.value[6]= 1.0f;
	basis_change.value[9]= -1.0f;
	basis_change.value[10]= 0.0f;

	perspective.PerspectiveProjection(aspect_, fov, z_near, z_far);

	return rotate_z * rotate_x * basis_change * perspective;
}

m_Mat4 CameraController::CalculateFullViewMatrix() const
{
	m_Mat4 translate;
	translate.Translate( -pos_ );
	return translate * CalculateViewMatrix();
}

m_Vec3 CameraController::GetCameraPosition() const
{
	return pos_;
}

} // namespace HexGPU
