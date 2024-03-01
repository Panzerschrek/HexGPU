#include "SystemWindow.hpp"
#include "Assert.hpp"
#include "Log.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstring>


namespace HexGPU
{

SystemWindow::SystemWindow(Settings& settings)
{
	// TODO - check errors.
	SDL_Init(SDL_INIT_VIDEO);

	const Uint32 window_flags= SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN;

	const int width = std::max(640, std::min(int(settings.GetInt("r_window_width" , 960)), 4096)) & ~3u;
	const int height= std::max(480, std::min(int(settings.GetInt("r_window_height", 720)), 4096)) & ~3u;

	settings.SetInt("r_window_width" , width );
	settings.SetInt("r_window_height", height);

	window_=
		SDL_CreateWindow(
			"⬡⬢⬡⬢ HexGPU ⬢⬡⬢⬡",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			width, height,
			window_flags);

	if(window_ == nullptr)
		Log::FatalError("Could not create window");

	Log::Info("Window created with size ", width, "x", height);
}

SystemWindow::~SystemWindow()
{
	Log::Info("Destroying system window");
	SDL_DestroyWindow(window_);
}

SDL_Window* SystemWindow::GetSDLWindow() const
{
	return window_;
}

std::vector<SDL_Event> SystemWindow::ProcessEvents()
{
	std::vector<SDL_Event> result_events;

	SDL_Event event;
	while( SDL_PollEvent(&event) != 0 )
		result_events.push_back( event );

	return result_events;
}

std::vector<bool> SystemWindow::GetKeyboardState()
{
	int key_count= 0;
	const Uint8* const keyboard_state= SDL_GetKeyboardState(&key_count);

	std::vector<bool> result;
	result.resize(size_t(key_count));

	for(int i= 0; i < key_count; ++i)
		result[size_t(i)]= keyboard_state[i] != 0;

	return result;
}

} // namespace HexGPU
