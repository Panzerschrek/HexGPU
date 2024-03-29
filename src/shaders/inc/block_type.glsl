
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
const uint8_t c_block_type_glass_white=					uint8_t( 12);
const uint8_t c_block_type_glass_grey=					uint8_t( 13);
const uint8_t c_block_type_glass_red=					uint8_t( 14);
const uint8_t c_block_type_glass_yellow=				uint8_t( 15);
const uint8_t c_block_type_glass_green=					uint8_t( 16);
const uint8_t c_block_type_glass_cian=					uint8_t( 17);
const uint8_t c_block_type_glass_blue=					uint8_t( 18);
const uint8_t c_block_type_glass_magenta=				uint8_t( 19);

const int c_num_block_types=									 20;

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
	c_optical_density_air, // fire
	c_optical_density_semisolid, // glass_white
	c_optical_density_semisolid, // glass_grey
	c_optical_density_semisolid, // glass_red
	c_optical_density_semisolid, // glass_yellow
	c_optical_density_semisolid, // glass_green
	c_optical_density_semisolid, // glass_cian
	c_optical_density_semisolid, // glass_blue
	c_optical_density_semisolid  // glass_magenta
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
	uint8_t(15),// fire stone
	uint8_t(0), // water
	uint8_t(0), // sand
	uint8_t(11),// fire
	uint8_t(0), // glass_white
	uint8_t(0), // glass_grey
	uint8_t(0), // glass_red
	uint8_t(0), // glass_yellow
	uint8_t(0), // glass_green
	uint8_t(0), // glass_cian
	uint8_t(0), // glass_blue
	uint8_t(0)  // glass_magenta
);

const uint8_t c_block_flammability_table[c_num_block_types]=
	uint8_t[c_num_block_types]
(
	uint8_t(0), // air
	uint8_t(0), // spherical block
	uint8_t(0), // stone
	uint8_t(0), // soil
	uint8_t(1), // wood
	uint8_t(0), // grass
	uint8_t(0), // brick
	uint8_t(1), // foliage
	uint8_t(0), // fire stone
	uint8_t(0), // water
	uint8_t(0), // sand
	uint8_t(0), // fire
	uint8_t(0), // glass_white
	uint8_t(0), // glass_grey
	uint8_t(0), // glass_red
	uint8_t(0), // glass_yellow
	uint8_t(0), // glass_green
	uint8_t(0), // glass_cian
	uint8_t(0), // glass_blue
	uint8_t(0)  // glass_magenta
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
	i16vec3( 1,  1,  1), // fire
	i16vec3(20, 20, 12), // glass_white
	i16vec3(20, 20, 13), // glass_grey
	i16vec3(20, 20, 14), // glass_red
	i16vec3(20, 20, 15), // glass_yellow
	i16vec3(20, 20, 16), // glass_green
	i16vec3(20, 20, 17), // glass_cian
	i16vec3(20, 20, 18), // glass_blue
	i16vec3(20, 20, 19)  // glass_magenta
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
