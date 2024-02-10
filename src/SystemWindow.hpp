#pragma once
#include <SDL_video.h>

namespace HexGPU
{

class SystemWindow final
{
public:
	SystemWindow();
	~SystemWindow();

	SDL_Window* GetSDLWindow() const;

private:
	SDL_Window* window_= nullptr;
};

} // namespace HexGPU
