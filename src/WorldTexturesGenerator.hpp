#pragma once
#include "ShaderList.hpp"
#include "TaskOrganizer.hpp"
#include "WindowVulkan.hpp"

namespace HexGPU
{

class WorldTexturesGenerator
{
public:
	static constexpr uint32_t c_texture_size_log2= 7;
	static constexpr uint32_t c_texture_size= 1 << c_texture_size_log2;

public:
	WorldTexturesGenerator(WindowVulkan& window_vulkan, vk::DescriptorPool global_descriptor_pool);
	~WorldTexturesGenerator();

	void PrepareFrame(TaskOrganizer& task_organizer);

	vk::ImageView GetImageView() const;
	vk::ImageView GetWaterImageView() const;
	vk::ImageView GetFireImageView() const;
	TaskOrganizer::ImageInfo GetImageInfo() const;

private:
	// Texture indices in GLSL code must match this table!
	static constexpr ShaderNames gen_shader_table[]
	{
		ShaderNames::texture_gen_bricks_comp,
		ShaderNames::texture_gen_fire_stone_comp,
		ShaderNames::texture_gen_grass_comp,
		ShaderNames::texture_gen_foliage_comp,
		ShaderNames::texture_gen_sand_comp,
		ShaderNames::texture_gen_soil_comp,
		ShaderNames::texture_gen_spherical_block_comp,
		ShaderNames::texture_gen_stone_comp,
		ShaderNames::texture_gen_wood_comp,
		ShaderNames::texture_gen_wood_end_comp,
		ShaderNames::texture_gen_water_comp,
		ShaderNames::texture_gen_fire_comp,
		ShaderNames::texture_gen_glass_comp,
	};

	static constexpr uint32_t c_num_layers= uint32_t(std::size(gen_shader_table));

	static constexpr uint32_t c_num_mips= c_texture_size_log2 - 1; // Ignore last mip for simplicity.

	static constexpr uint32_t c_texture_num_texels= c_texture_size * c_texture_size;

	const uint32_t c_water_image_index= 10;
	const uint32_t c_fire_image_index= 11;

private:

	struct TextureGenPipelineData
	{
		vk::UniqueShaderModule shader;
		vk::UniquePipeline pipeline;
		vk::UniqueImageView image_layer_view;
		vk::DescriptorSet descriptor_set;
	};

	struct TextureGenPipelines
	{
		vk::UniqueDescriptorSetLayout descriptor_set_layout;
		vk::UniquePipelineLayout pipeline_layout;
		std::array<TextureGenPipelineData, c_num_layers> pipelines;
	};

private:
	static TextureGenPipelines CreatePipelines(
		vk::Device vk_device,
		vk::DescriptorPool global_descriptor_pool,
		vk::Image image);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;

	const vk::UniqueImage image_;
	const vk::UniqueDeviceMemory image_memory_;
	const vk::UniqueImageView image_view_;
	const vk::UniqueImageView water_image_view_;
	const vk::UniqueImageView fire_image_view_;

	const TextureGenPipelines texture_gen_pipelines_;

	bool textures_generated_= false;
};

} // namespace HexGPU
