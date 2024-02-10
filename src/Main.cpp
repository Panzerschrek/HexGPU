#include "Log.hpp"
#include "Host.hpp"

namespace HexGPU
{

extern "C" int main()
{
	try
	{
		Host host;
		while(!host.Loop()){}
	}
	catch(const std::exception& ex)
	{
		Log::FatalError("Exception throwed: ", ex.what());
	}
}

} // namespace HexGPU
