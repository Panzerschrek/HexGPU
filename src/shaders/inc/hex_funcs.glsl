#include "constants.glsl"

int ChunkBlockAddress(ivec3 coord)
{
	return coord.z + (coord.y << c_chunk_height_log2) + (coord.x << (c_chunk_width_log2 + c_chunk_height_log2));
}

bool IsInWorldBorders(ivec3 pos, ivec2 world_size_chunks)
{
	return
		pos.x >= 0 && pos.x < c_chunk_width * world_size_chunks.x &&
		pos.y >= 0 && pos.y < c_chunk_width * world_size_chunks.y &&
		pos.z >= 0 && pos.z < c_chunk_height;
}

// Block must be in world borders!
int GetBlockFullAddress(ivec3 pos, ivec2 world_size_chunks)
{
	int chunk_x= pos.x >> c_chunk_width_log2;
	int chunk_y= pos.y >> c_chunk_width_log2;
	int local_x= pos.x & (c_chunk_width - 1);
	int local_y= pos.y & (c_chunk_width - 1);

	int chunk_index= chunk_x + chunk_y * world_size_chunks.x;
	int chunk_data_offset= chunk_index * c_chunk_volume;

	return chunk_data_offset + ChunkBlockAddress(ivec3(local_x, local_y, pos.z));
}

ivec2 GetMaxGlobalCoord(ivec2 world_size_chunks)
{
	return ivec2(world_size_chunks.x * c_chunk_width - 1, world_size_chunks.y * c_chunk_width - 1);
}

// Returns coordinates of a hexagon in given point.
ivec2 GetHexogonCoord(vec2 pos)
{
	float transformed_x= pos.x / c_space_scale_x;
	float floor_x= floor(transformed_x);
	int nearest_x= int(floor_x);
	float dx= transformed_x - floor_x;

	float transformed_y= pos.y - 0.5f * float((nearest_x ^ 1) & 1);
	float floor_y= floor(transformed_y);
	int nearest_y= int(floor_y);
	float dy= transformed_y - floor_y;

	// Upper part   y=  0.5 + 1.5 * x
	// Lower part   y=  0.5 - 1.5 * x
	/*
	_____________
	|  /         |\
	| /          | \
	|/           |  \
	|\           |  /
	| \          | /
	|__\_________|/
	*/
	if(dy > 0.5f + 1.5f * dx)
	{
		return ivec2(nearest_x - 1, nearest_y + ((nearest_x ^ 1) & 1));
	}
	else if(dy < 0.5f - 1.5f * dx)
	{
		return ivec2(nearest_x - 1, nearest_y - (nearest_x & 1));;
	}
	else
	{
		return ivec2(nearest_x, nearest_y);
	}
}
