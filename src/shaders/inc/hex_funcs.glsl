#include "constants.glsl"

int ChunkBlockAddress(int x, int y, int z)
{
	return z + (y << c_chunk_height_log2) + (x << (c_chunk_width_log2 + c_chunk_height_log2));
}

bool IsInWorldBorders(ivec3 pos)
{
	return
		pos.x >= 0 && pos.x < c_chunk_width * c_chunk_matrix_size[0] &&
		pos.y >= 0 && pos.y < c_chunk_width * c_chunk_matrix_size[1] &&
		pos.z >= 0 && pos.z < c_chunk_height;
}

// Block must be in world borders!
int GetBlockFullAddress(ivec3 pos)
{
	int chunk_x= pos.x >> c_chunk_width_log2;
	int chunk_y= pos.y >> c_chunk_width_log2;
	int local_x= pos.x & (c_chunk_width - 1);
	int local_y= pos.y & (c_chunk_width - 1);

	int chunk_index= chunk_x + chunk_y * c_chunk_matrix_size[0];
	int chunk_data_offset= chunk_index * c_chunk_volume;

	return chunk_data_offset + ChunkBlockAddress(local_x, local_y, pos.z);
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
