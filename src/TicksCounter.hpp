#pragma once
#include <chrono>


namespace HexGPU
{

class TicksCounter final
{
public:
	using Clock= std::chrono::steady_clock;

	TicksCounter(Clock::duration frequency_calc_interval);

	void Tick(uint32_t count= 1u);
	float GetTicksFrequency() const;
	uint32_t GetTotalTicks() const;

private:
	const Clock::duration frequency_calc_interval_;

	uint32_t total_ticks_;
	float output_ticks_frequency_;
	uint32_t current_sample_ticks_;

	Clock::time_point last_update_time_;
};

} // namespace HexGPU
