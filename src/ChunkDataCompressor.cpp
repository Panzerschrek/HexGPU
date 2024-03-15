#include "ChunkDataCompressor.hpp"
#include "Assert.hpp"
#include "Constants.hpp"
#include "Log.hpp"
#include <snappy.h>
#include <cstring>

namespace HexGPU
{

namespace
{

bool UncompressRawBlocksArray(const std::string& in, char* const out)
{
	size_t uncompressed_length= 0;
	if(!snappy::GetUncompressedLength(in.data(), in.size(), &uncompressed_length))
	{
		Log::Info("Can't gen uncompressed length");
		return false;
	}

	if(uncompressed_length != c_chunk_volume)
	{
		Log::Info("Unexpected uncompressed length, expected ", c_chunk_volume, " got ", uncompressed_length);
		return false;
	}

	if(!snappy::RawUncompress(in.data(), in.size(), out))
	{
		Log::Info("Uncompress failed");
		return false;
	}

	return true;
}

std::string CompressRawBlocksArray(const char* const in, std::string& temp_buffer)
{
	if(!snappy::Compress(in, c_chunk_volume, &temp_buffer))
		Log::Info("Failed to compress");

	// Create copy of the temp buffer.
	// Reuse internal storage of the temp buffer for later usage.
	return temp_buffer;
}

} // namespace

ChunkDataCompresed ChunkDataCompressor::Compress(
	const BlockType* const blocks_data,
	const uint8_t* const blocks_auxiliar_data)
{
	ChunkDataCompresed out_data;

	// Reuse temp buffer for compression, because "snappy" reserves a lot of memory inside it (more than uncompressed size)
	// and we don't whant to return strings with too much memory reserved.
	// Doing so we keep only this buffer with large storage and result buffers only of necessary size.
	out_data.blocks=
		CompressRawBlocksArray(reinterpret_cast<const char*>(blocks_data), temp_compress_buffer_);
	out_data.auxiliar_data=
		CompressRawBlocksArray(reinterpret_cast<const char*>(blocks_auxiliar_data), temp_compress_buffer_);

	return out_data;
}

bool ChunkDataCompressor::Decompress(
	const ChunkDataCompresed& data_compressed,
	BlockType* const blocks_data,
	uint8_t* const blocks_auxiliar_data)
{
	if(!UncompressRawBlocksArray(data_compressed.blocks, reinterpret_cast<char*>(blocks_data)))
	{
		Log::Info("Can't decompress blocks data");
		return false;
	}

	if(!UncompressRawBlocksArray(data_compressed.auxiliar_data, reinterpret_cast<char*>(blocks_auxiliar_data)))
	{
		Log::Info("Can't decompress blocks auxiliar data");
		return false;
	}

	return true;
}

} // namespace HexGPU
