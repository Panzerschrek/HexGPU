#pragma once
#include "Settings.hpp"
#include <SDL_events.h>
#include <SDL_video.h>
#include <vector>

namespace HexGPU
{

class SystemWindow
{
public:
	explicit SystemWindow(Settings& settings);
	~SystemWindow();

	SDL_Window* GetSDLWindow() const;

	std::vector<SDL_Event> ProcessEvents();
	std::vector<bool> GetKeyboardState();

	void SetMouseCaptured(bool captured);

private:
	SDL_Window* window_= nullptr;
};

} // namespace HexGPU
