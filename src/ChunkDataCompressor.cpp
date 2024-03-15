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

} // namespace

ChunkDataCompresed ChunkDataCompressor::Compress(
	const BlockType* const blocks_data,
	const uint8_t* const blocks_auxiliar_data)
{
	ChunkDataCompresed out_data;

	// TODO - check for correctness/possible errors.
	// TODO - tune compression params.
	snappy::Compress(reinterpret_cast<const char*>(blocks_data), c_chunk_volume, &out_data.blocks);
	snappy::Compress(reinterpret_cast<const char*>(blocks_auxiliar_data), c_chunk_volume, &out_data.auxiliar_data);

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
