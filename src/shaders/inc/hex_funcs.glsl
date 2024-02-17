#include "constants.glsl"

int ChunkBlockAddress(int x, int y, int z)
{
	return z + (y << c_chunk_height_log2) + (x << (c_chunk_width_log2 + c_chunk_height_log2));
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
