
// If this changed, "BlockTypeList.hpp" must be changed too!
const uint8_t c_block_type_air=							uint8_t(  0);
const uint8_t c_block_type_spherical_block=				uint8_t(  1);
const uint8_t c_block_type_stone=						uint8_t(  2);
const uint8_t c_block_type_soil=						uint8_t(  3);
const uint8_t c_block_type_wood=						uint8_t(  4);
const uint8_t c_block_type_grass=						uint8_t(  5);
const uint8_t c_block_type_brick=						uint8_t(  6);
const uint8_t c_block_type_foliage=						uint8_t(  7);
const uint8_t c_block_type_fire_stone=					uint8_t(  8);
const uint8_t c_block_type_water=						uint8_t(  9);
const uint8_t c_block_type_sand=						uint8_t( 10);
const int c_num_block_types=									 11;

const uint8_t c_optical_density_solid= uint8_t(0);
const uint8_t c_optical_density_semisolid= uint8_t(1);
const uint8_t c_optical_density_air= uint8_t(2);

const uint8_t c_block_optical_density_table[c_num_block_types]=
	uint8_t[c_num_block_types]
(
	c_optical_density_air, // air
	c_optical_density_solid, // spherical block
	c_optical_density_solid, // stone
	c_optical_density_solid, // soil
	c_optical_density_solid, // wood
	c_optical_density_solid, // grass
	c_optical_density_solid, // brick
	c_optical_density_semisolid, // foliage
	c_optical_density_solid, // fire stone
	c_optical_density_semisolid, // water
	c_optical_density_solid // sand
);
