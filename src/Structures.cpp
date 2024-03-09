#include "Structures.hpp"

namespace HexGPU
{

Structures GenStructures()
{
	StructureDescription descriptiuon;
	descriptiuon.size[0]= 2;
	descriptiuon.size[1]= 3;
	descriptiuon.size[2]= 4;

	const auto volume= descriptiuon.size[0] * descriptiuon.size[1] * descriptiuon.size[2];

	Structures structures;
	structures.data.resize(structures.data.size() + volume, BlockType::Wood);
	structures.descriptions.push_back(descriptiuon);

	return structures;
}

} // namespace HexGPU
