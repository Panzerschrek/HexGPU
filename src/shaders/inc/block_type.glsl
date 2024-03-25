
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
const uint8_t c_block_type_fire=						uint8_t( 11);

const int c_num_block_types=									 12;

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
	c_optical_density_air, // water
	c_optical_density_solid, // sand
	c_optical_density_air // fire
);

const uint8_t c_block_own_light_table[c_num_block_types]=
	uint8_t[c_num_block_types]
(
	uint8_t(0), // air
	uint8_t(0), // spherical block
	uint8_t(0), // stone
	uint8_t(0), // soil
	uint8_t(0), // wood
	uint8_t(0), // grass
	uint8_t(0), // brick
	uint8_t(0), // foliage
	uint8_t(15), // fire stone
	uint8_t(0), // water
	uint8_t(0),  // sand
	uint8_t(7)  // fire
);

// If this changed, TexturesTable.hpp" must be changed too!
// "r" - top, "g" - bottom, "b" - sides.
// TODO - support more precise mapping - individually for all 6 sides?
const i16vec3 c_block_texture_table[c_num_block_types]=
	i16vec3[c_num_block_types]
(
	i16vec3( 0,  0,  0), // air
	i16vec3( 6,  6,  6), // spherical block
	i16vec3( 7,  7,  7), // stone
	i16vec3( 5,  5,  5), // soil
	i16vec3( 9,  9,  8), // wood
	i16vec3( 2,  5,  5), // grass
	i16vec3( 0,  0,  0), // brick
	i16vec3( 3,  3,  3), // foliage
	i16vec3( 1,  1,  1), // fire stone
	i16vec3(10, 10, 10), // water
	i16vec3( 4,  4,  4), // sand
	i16vec3( 1,  1,  1)  // fire
);

// If this changed, C++ code must be changed too!
const uint8_t c_direction_up= uint8_t(0);
const uint8_t c_direction_down= uint8_t(1);
const uint8_t c_direction_north= uint8_t(2);
const uint8_t c_direction_south= uint8_t(3);
const uint8_t c_direction_north_east= uint8_t(4);
const uint8_t c_direction_south_east= uint8_t(5);
const uint8_t c_direction_north_west= uint8_t(6);
const uint8_t c_direction_south_west= uint8_t(7);
