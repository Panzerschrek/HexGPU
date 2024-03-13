#include "TicksCounter.hpp"


namespace HexGPU
{

TicksCounter::TicksCounter(const Clock::duration frequency_calc_interval)
	: frequency_calc_interval_(frequency_calc_interval)
	, total_ticks_(0u)
	, output_ticks_frequency_(0.0f)
	, current_sample_ticks_(0u)
	, last_update_time_(Clock::now())
{}

void TicksCounter::Tick(const uint32_t count)
{
	total_ticks_+= count;
	current_sample_ticks_+= count;

	const Clock::time_point current_time= Clock::now();
	const Clock::duration dt= current_time - last_update_time_;

	if(dt >= frequency_calc_interval_)
	{
		output_ticks_frequency_=
			float(current_sample_ticks_) * float(Clock::duration::period::den) /
			(float(dt.count()) * float(Clock::duration::period::num));

		current_sample_ticks_= 0u;
		last_update_time_+= dt;
	}
}

float TicksCounter::GetTicksFrequency() const
{
	return output_ticks_frequency_;
}

uint32_t TicksCounter::GetTotalTicks() const
{
	return total_ticks_;
}

} // namespace HexGPU
