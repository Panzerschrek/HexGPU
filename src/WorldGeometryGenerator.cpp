#include "WorldGeometryGenerator.hpp"
#include "Assert.hpp"
#include "Constants.hpp"
#include "GlobalDescriptorPool.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"

namespace HexGPU
{

namespace
{

namespace GeometrySizeCalculatePrepareShaderBindings
{
	const ShaderBindingIndex chunk_draw_info_buffer= 0;
}

namespace GeometrySizeCalculateShaderBindings
{
	const ShaderBindingIndex chunk_data_buffer= 0;
	const ShaderBindingIndex chunk_draw_info_buffer= 1;
}

namespace GeometryAllocateShaderBindings
{
	const ShaderBindingIndex chunk_draw_info_buffer= 0;
	const ShaderBindingIndex allocator_data_buffer= GPUAllocator::c_allocator_buffer_binding;
}

namespace GeometryGenShaderBindings
{
	const ShaderBindingIndex vertices_buffer= 0;
	const ShaderBindingIndex chunk_data_buffer= 1;
	const ShaderBindingIndex chunk_light_buffer= 2;
	const ShaderBindingIndex chunk_draw_info_buffer= 3;
}

// Size limit of this struct is 128 bytes.
// 128 bytes is guaranted maximum size of push constants uniform block.
struct ChunkPositionUniforms
{
	int32_t world_size_chunks[2]{};
	int32_t chunk_position[2]{}; // Position relative current loaded region.
	int32_t chunk_global_position[2]{}; // Global position.
};

struct GeometrySizeCalculatePrepareUniforms
{
	int32_t world_size_chunks[2];
};

constexpr uint32_t c_max_chunks_to_allocate= 62;

struct GeometryAllocateUniforms
{
	uint32_t num_chunks_to_allocate= 0;
	uint16_t chunks_to_allocate_list[c_max_chunks_to_allocate]{};
};

// This struct should be not bigger than minimum PushUniforms size.
static_assert(sizeof(GeometryAllocateUniforms) == 128, "Invalid size!");

// This should match the same constant in GLSL code!
const uint32_t c_allocation_unut_size_quads= 512;

// Assuming that amount of total required vertex memory is proportional to total number of chunks,
// multiplied by some factor.
// Assuming that in worst cases (complex geometry) and taking allocator fragmentation into account
// we need to have enough space for so much vertices.
const uint32_t c_max_average_quads_per_chunk= 6144;

uint32_t GetTotalVertexBufferQuads(const WorldSizeChunks& world_size)
{
	return c_max_average_quads_per_chunk * world_size[0] * world_size[1];
}

uint32_t GetTotalVertexBufferUnits(const WorldSizeChunks& world_size)
{
	// Number of allocation units is based on number of quads with rounding upwards.
	return (GetTotalVertexBufferQuads(world_size) + (c_allocation_unut_size_quads - 1)) / c_allocation_unut_size_quads;
}

ComputePipeline CreateGeometrySizeCalculatePreparePipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::geometry_size_calculate_prepare_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			GeometrySizeCalculatePrepareShaderBindings::chunk_draw_info_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(GeometrySizeCalculatePrepareUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreateGeometrySizeCalculatePipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::geometry_size_calculate_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			GeometrySizeCalculateShaderBindings::chunk_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			GeometrySizeCalculateShaderBindings::chunk_draw_info_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(ChunkPositionUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreateGeometryAllocatePipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::geometry_allocate_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			GeometryAllocateShaderBindings::chunk_draw_info_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			GeometryAllocateShaderBindings::allocator_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(GeometryAllocateUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

ComputePipeline CreateGeometryGenPipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::geometry_gen_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			GeometryGenShaderBindings::vertices_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			GeometryGenShaderBindings::chunk_data_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			GeometryGenShaderBindings::chunk_light_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			GeometryGenShaderBindings::chunk_draw_info_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(ChunkPositionUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

} // namespace

WorldGeometryGenerator::WorldGeometryGenerator(
	WindowVulkan& window_vulkan,
	const WorldProcessor& world_processor,
	const vk::DescriptorPool global_descriptor_pool)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, world_processor_(world_processor)
	, world_size_(world_processor.GetWorldSize())
	, chunk_draw_info_buffer_(
		window_vulkan,
		world_size_[0] * world_size_[1] * uint32_t(sizeof(ChunkDrawInfo)),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, vertex_buffer_num_quads_(GetTotalVertexBufferQuads(world_size_))
	, vertex_buffer_(
		window_vulkan,
		vertex_buffer_num_quads_ * uint32_t(sizeof(QuadVertices)),
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, vertex_memory_allocator_(window_vulkan, GetTotalVertexBufferUnits(world_size_))
	, geometry_size_calculate_prepare_pipeline_(CreateGeometrySizeCalculatePreparePipeline(vk_device_))
	, geometry_size_calculate_prepare_descriptor_set_(
		CreateDescriptorSet(
			vk_device_,
			global_descriptor_pool,
			*geometry_size_calculate_prepare_pipeline_.descriptor_set_layout))
	, geometry_size_calculate_pipeline_(CreateGeometrySizeCalculatePipeline(vk_device_))
	, geometry_size_calculate_descriptor_sets_{
		CreateDescriptorSet(
			vk_device_,
			global_descriptor_pool,
			*geometry_size_calculate_pipeline_.descriptor_set_layout),
		CreateDescriptorSet(
			vk_device_,
			global_descriptor_pool,
			*geometry_size_calculate_pipeline_.descriptor_set_layout)}
	, geometry_allocate_pipeline_(CreateGeometryAllocatePipeline(vk_device_))
	, geometry_allocate_descriptor_set_(
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *geometry_allocate_pipeline_.descriptor_set_layout))
	, geometry_gen_pipeline_(CreateGeometryGenPipeline(vk_device_))
	, geometry_gen_descriptor_sets_{
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *geometry_gen_pipeline_.descriptor_set_layout),
		CreateDescriptorSet(vk_device_, global_descriptor_pool, *geometry_gen_pipeline_.descriptor_set_layout)}
{
	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_draw_info_buffer_info(
			chunk_draw_info_buffer_.GetBuffer(),
			0u,
			chunk_draw_info_buffer_.GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					geometry_size_calculate_prepare_descriptor_set_,
					GeometrySizeCalculatePrepareShaderBindings::chunk_draw_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_draw_info_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			world_processor_.GetChunkDataBuffer(i),
			0u,
			world_processor_.GetChunkDataBufferSize());

		const vk::DescriptorBufferInfo descriptor_chunk_draw_info_buffer_info(
			chunk_draw_info_buffer_.GetBuffer(),
			0u,
			chunk_draw_info_buffer_.GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					geometry_size_calculate_descriptor_sets_[i],
					GeometrySizeCalculateShaderBindings::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					geometry_size_calculate_descriptor_sets_[i],
					GeometrySizeCalculateShaderBindings::chunk_draw_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_draw_info_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_draw_info_buffer_info(
			chunk_draw_info_buffer_.GetBuffer(),
			0u,
			chunk_draw_info_buffer_.GetSize());

		const vk::DescriptorBufferInfo descriptor_allocator_data_buffer_info(
			vertex_memory_allocator_.GetAllocatorDataBuffer(),
			0u,
			vertex_memory_allocator_.GetAllocatorDataBufferSize());

		vk_device_.updateDescriptorSets(
			{
				{
					geometry_allocate_descriptor_set_,
					GeometryAllocateShaderBindings::chunk_draw_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_draw_info_buffer_info,
					nullptr
				},
				{
					geometry_allocate_descriptor_set_,
					GeometryAllocateShaderBindings::allocator_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_allocator_data_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update descriptor sets.
	for(uint32_t i= 0; i < 2; ++i)
	{
		const vk::DescriptorBufferInfo descriptor_vertex_buffer_info(
			vertex_buffer_.GetBuffer(),
			0u,
			vertex_buffer_.GetSize());

		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			world_processor_.GetChunkDataBuffer(i),
			0u,
			world_processor_.GetChunkDataBufferSize());

		const vk::DescriptorBufferInfo descriptor_light_buffer_info(
			world_processor_.GetLightDataBuffer(i),
			0u,
			world_processor_.GetLightDataBufferSize());

		const vk::DescriptorBufferInfo descriptor_chunk_draw_info_buffer_info(
			chunk_draw_info_buffer_.GetBuffer(),
			0u,
			chunk_draw_info_buffer_.GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					geometry_gen_descriptor_sets_[i],
					GeometryGenShaderBindings::vertices_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_vertex_buffer_info,
					nullptr
				},
				{
					geometry_gen_descriptor_sets_[i],
					GeometryGenShaderBindings::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
				{
					geometry_gen_descriptor_sets_[i],
					GeometryGenShaderBindings::chunk_light_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_light_buffer_info,
					nullptr
				},
				{
					geometry_gen_descriptor_sets_[i],
					GeometryGenShaderBindings::chunk_draw_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_draw_info_buffer_info,
					nullptr
				},
			},
			{});
	}
}

WorldGeometryGenerator::~WorldGeometryGenerator()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void WorldGeometryGenerator::Update(const vk::CommandBuffer command_buffer)
{
	vertex_memory_allocator_.EnsureInitialized(command_buffer);
	InitialFillBuffers(command_buffer);
	BuildChunksToUpdateList();
	PrepareGeometrySizeCalculation(command_buffer);
	CalculateGeometrySize(command_buffer);
	AllocateMemoryForGeometry(command_buffer);
	GenGeometry(command_buffer);

	++frame_counter_;
}

vk::Buffer WorldGeometryGenerator::GetVertexBuffer() const
{
	return vertex_buffer_.GetBuffer();
}

vk::Buffer WorldGeometryGenerator::GetChunkDrawInfoBuffer() const
{
	return chunk_draw_info_buffer_.GetBuffer();
}

vk::DeviceSize WorldGeometryGenerator::GetChunkDrawInfoBufferSize() const
{
	return chunk_draw_info_buffer_.GetSize();
}

void WorldGeometryGenerator::InitialFillBuffers(const vk::CommandBuffer command_buffer)
{
	if(buffers_initially_filled_)
		return;
	buffers_initially_filled_= true;

	// Fill initially vertex buffer with zeros.
	// Do this only to supress warnings.
	command_buffer.fillBuffer(vertex_buffer_.GetBuffer(), 0, vertex_buffer_.GetSize(), 0);

	// Fill initially chunk draw info buffer with zeros.
	command_buffer.fillBuffer(chunk_draw_info_buffer_.GetBuffer(), 0, chunk_draw_info_buffer_.GetSize(), 0);

	const vk::BufferMemoryBarrier barriers[]
	{
		{
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			vertex_buffer_.GetBuffer(),
			0,
			VK_WHOLE_SIZE
		},
		{
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			chunk_draw_info_buffer_.GetBuffer(),
			0,
			VK_WHOLE_SIZE
		},
	};

	command_buffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eComputeShader,
		vk::DependencyFlags(),
		0, nullptr,
		uint32_t(std::size(barriers)), barriers,
		0, nullptr);
}

void WorldGeometryGenerator::BuildChunksToUpdateList()
{
	chunks_to_update_.clear();

	if(frame_counter_ == 0)
	{
		// On first frame update all chunks.
		for(uint32_t y= 0; y < world_size_[1]; ++y)
		for(uint32_t x= 0; x < world_size_[0]; ++x)
			chunks_to_update_.push_back({x, y});
	}
	else
	{
		// Update each frame only 1 / N of all chunks.
		// TODO - use some fixed period, indipendnent on framerate?
		const uint32_t update_period= 64;
		const uint32_t update_period_fast= 4;

		const uint32_t center_x= world_size_[0] / 2;
		const uint32_t center_y= world_size_[1] / 2;
		const uint32_t center_radius= 1;

		for(uint32_t y= 0; y < world_size_[1]; ++y)
		for(uint32_t x= 0; x < world_size_[0]; ++x)
		{
			const uint32_t chunk_index= x + y * world_size_[0];
			const uint32_t chunk_index_frame_shifted= chunk_index + frame_counter_;

			if( x >= center_x - center_radius && x <= center_x + center_radius &&
				y >= center_y - center_radius && y <= center_y + center_radius)
			{
				// Update chunks in the center with more frequently.
				// Do this because the player should be somewhere in the middle of the world
				// and we need to show player actions (build/destroy) results faster.
				if(chunk_index_frame_shifted % update_period_fast == 0)
					chunks_to_update_.push_back({x, y});
			}
			else
			{
				if(chunk_index_frame_shifted % update_period == 0)
					chunks_to_update_.push_back({x, y});
			}
		}
	}
}

void WorldGeometryGenerator::PrepareGeometrySizeCalculation(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *geometry_size_calculate_prepare_pipeline_.pipeline);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*geometry_size_calculate_prepare_pipeline_.pipeline_layout,
		0u,
		{geometry_size_calculate_prepare_descriptor_set_},
		{});

	GeometrySizeCalculatePrepareUniforms uniforms;
	uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
	uniforms.world_size_chunks[1]= int32_t(world_size_[1]);

	command_buffer.pushConstants(
		*geometry_size_calculate_prepare_pipeline_.pipeline_layout,
		vk::ShaderStageFlagBits::eCompute,
		0,
		sizeof(GeometrySizeCalculatePrepareUniforms), static_cast<const void*>(&uniforms));

	// Dispatch a thread for each chunk.
	command_buffer.dispatch(world_size_[0], world_size_[1], 1);

	// Create barrier between update chunk draw info buffer and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			chunk_draw_info_buffer_.GetBuffer(),
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

void WorldGeometryGenerator::CalculateGeometrySize(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *geometry_size_calculate_pipeline_.pipeline);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*geometry_size_calculate_pipeline_.pipeline_layout,
		0u,
		{geometry_size_calculate_descriptor_sets_[world_processor_.GetActualBuffersIndex()]},
		{});

	const WorldOffsetChunks world_offset= world_processor_.GetWorldOffset();

	// Execute geometry size calculation for all chunks.
	// TODO - do this no each frame and not for each chunk.
	for(const auto& chunk_to_update : chunks_to_update_)
	{
		ChunkPositionUniforms chunk_position_uniforms;
		chunk_position_uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
		chunk_position_uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
		chunk_position_uniforms.chunk_position[0]= int32_t(chunk_to_update[0]);
		chunk_position_uniforms.chunk_position[1]= int32_t(chunk_to_update[1]);
		chunk_position_uniforms.chunk_global_position[0]= world_offset[0] + int32_t(chunk_to_update[0]);
		chunk_position_uniforms.chunk_global_position[1]= world_offset[1] + int32_t(chunk_to_update[1]);

		command_buffer.pushConstants(
			*geometry_size_calculate_pipeline_.pipeline_layout,
			vk::ShaderStageFlagBits::eCompute,
			0,
			sizeof(ChunkPositionUniforms), static_cast<const void*>(&chunk_position_uniforms));

		// This constant should match workgroup size in shader!
		constexpr uint32_t c_workgroup_size[]{4, 4, 8};
		static_assert(c_chunk_width % c_workgroup_size[0] == 0, "Wrong workgroup size!");
		static_assert(c_chunk_width % c_workgroup_size[1] == 0, "Wrong workgroup size!");
		static_assert(c_chunk_height % c_workgroup_size[2] == 0, "Wrong workgroup size!");

		command_buffer.dispatch(
			c_chunk_width / c_workgroup_size[0],
			c_chunk_width / c_workgroup_size[1],
			c_chunk_height / c_workgroup_size[2]);
	}

	// Create barrier between update chunk draw info buffer and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			chunk_draw_info_buffer_.GetBuffer(),
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

void WorldGeometryGenerator::AllocateMemoryForGeometry(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *geometry_allocate_pipeline_.pipeline);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*geometry_allocate_pipeline_.pipeline_layout,
		0u,
		{geometry_allocate_descriptor_set_},
		{});

	// We have limited uniform size for chunks to update list.
	// So, perform several updates if necessary.
	for(size_t offset= 0; offset < chunks_to_update_.size(); offset+= c_max_chunks_to_allocate)
	{
		GeometryAllocateUniforms uniforms;
		uniforms.num_chunks_to_allocate= uint32_t(std::min(size_t(c_max_chunks_to_allocate), chunks_to_update_.size() - offset));
		for(uint32_t i= 0; i < uniforms.num_chunks_to_allocate; ++i)
		{
			const auto& chunk_to_update= chunks_to_update_[uint32_t(offset) + i];
			const uint32_t chunk_index= chunk_to_update[0] + chunk_to_update[1] * world_size_[0];
			uniforms.chunks_to_allocate_list[i]= uint16_t(chunk_index);
		}

		command_buffer.pushConstants(
			*geometry_allocate_pipeline_.pipeline_layout,
			vk::ShaderStageFlagBits::eCompute,
			0,
			sizeof(GeometryAllocateUniforms), static_cast<const void*>(&uniforms));

		// Use single thread for allocation.
		command_buffer.dispatch(1, 1 , 1);

		// Create barrier between update allocation buffer and its later usage.
		// TODO - check this is correct.
		{
			const vk::BufferMemoryBarrier barrier(
				vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
				queue_family_index_, queue_family_index_,
				vertex_memory_allocator_.GetAllocatorDataBuffer(),
				0,
				vertex_memory_allocator_.GetAllocatorDataBufferSize());

			command_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eComputeShader,
				vk::PipelineStageFlagBits::eComputeShader,
				vk::DependencyFlags(),
				0, nullptr,
				1, &barrier,
				0, nullptr);
		}
	}

	// Create barrier between update chunk draw info buffer and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			chunk_draw_info_buffer_.GetBuffer(),
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

void WorldGeometryGenerator::GenGeometry(const vk::CommandBuffer command_buffer)
{
	// Update geometry, count number of quads.

	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *geometry_gen_pipeline_.pipeline);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*geometry_gen_pipeline_.pipeline_layout,
		0u,
		{geometry_gen_descriptor_sets_[world_processor_.GetActualBuffersIndex()]},
		{});

	const WorldOffsetChunks world_offset= world_processor_.GetWorldOffset();

	// Execute geometry generation for all chunks.
	// TODO - do this no each frame and not for each chunk.
	for(const auto& chunk_to_update : chunks_to_update_)
	{
		ChunkPositionUniforms chunk_position_uniforms;
		chunk_position_uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
		chunk_position_uniforms.world_size_chunks[1]= int32_t(world_size_[1]);
		chunk_position_uniforms.chunk_position[0]= int32_t(chunk_to_update[0]);
		chunk_position_uniforms.chunk_position[1]= int32_t(chunk_to_update[1]);
		chunk_position_uniforms.chunk_global_position[0]= world_offset[0] + int32_t(chunk_to_update[0]);
		chunk_position_uniforms.chunk_global_position[1]= world_offset[1] + int32_t(chunk_to_update[1]);

		command_buffer.pushConstants(
			*geometry_gen_pipeline_.pipeline_layout,
			vk::ShaderStageFlagBits::eCompute,
			0,
			sizeof(ChunkPositionUniforms), static_cast<const void*>(&chunk_position_uniforms));

		// This constant should match workgroup size in shader!
		constexpr uint32_t c_workgroup_size[]{4, 4, 8};
		static_assert(c_chunk_width % c_workgroup_size[0] == 0, "Wrong workgroup size!");
		static_assert(c_chunk_width % c_workgroup_size[1] == 0, "Wrong workgroup size!");
		static_assert(c_chunk_height % c_workgroup_size[2] == 0, "Wrong workgroup size!");

		command_buffer.dispatch(
			c_chunk_width / c_workgroup_size[0],
			c_chunk_width / c_workgroup_size[1],
			c_chunk_height / c_workgroup_size[2]);
	}

	// Create barrier between update vertex buffer and its usage for rendering.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			vertex_buffer_.GetBuffer(),
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eVertexShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}

	// Create barrier between update chunk draw info buffer and its later usage.
	// TODO - check this is correct.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			queue_family_index_, queue_family_index_,
			chunk_draw_info_buffer_.GetBuffer(),
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

} // namespace HexGPU
