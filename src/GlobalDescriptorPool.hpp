#include "HexGPUVulkan.hpp"

namespace HexGPU
{

// Creates descriptor pool situable for allocating descriptors which are created once and live the whole life of the applocation.
// It contains large enough number of descriptors.
vk::UniqueDescriptorPool CreateGlobalDescriptorPool(vk::Device vk_device);

} // namespace HexGPU
