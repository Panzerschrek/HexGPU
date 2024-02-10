#pragma once
#include "SystemWindow.hpp"
#include "WindowVulkan.hpp"
#include <chrono>


namespace HexGPU
{

class Host final
{
public:
	Host();

	// Returns false on quit
	bool Loop();


private:
	using Clock= std::chrono::steady_clock;

	SystemWindow system_window_;
	WindowVulkan window_vulkan_;

	const Clock::time_point init_time_;
	Clock::time_point prev_tick_time_;

	bool quit_requested_= false;
};

} // namespace HexGPU
