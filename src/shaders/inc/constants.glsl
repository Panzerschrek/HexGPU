// If this file changed, corresponding C++ file must be changed too!

const int c_chunk_width_log2= 4;
const int c_chunk_width= 1 << c_chunk_width_log2;

const int c_chunk_height_log2= 7;
const int c_chunk_height= 1 << c_chunk_height_log2;

// In blocks.
const int c_chunk_volume= c_chunk_width * c_chunk_width * c_chunk_height;

int ChunkBlockAddress(int x, int y, int z)
{
	return z + (y << c_chunk_height_log2) + (x << (c_chunk_width_log2 + c_chunk_height_log2));
}
