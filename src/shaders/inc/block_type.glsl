
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
	c_optical_density_air, // water
	c_optical_density_solid  // sand
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
	uint8_t(0)  // sand
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
	i16vec3( 4,  4,  4)  // sand
);

// If this changed, TexturesTable.hpp" must be changed too!
const uint c_num_textures= 11;

// "x" - scale, "y" per-block flag.
const u8vec2 c_texture_property_table[c_num_textures]=
	u8vec2[c_num_textures]
(
	u8vec2(3, 0), // brick.jpg
	u8vec2(2, 0), // fire.jpg
	u8vec2(3, 0), // grass2.jpg
	u8vec2(4, 0), // leaves3.tga
	u8vec2(1, 0), // sand.jpg
	u8vec2(4, 0), // soil.jpg
	u8vec2(4, 1), // spherical_block_2.png
	u8vec2(3, 0), // stone.jpg
	u8vec2(4, 0), // wood.jpg
	u8vec2(4, 1), // wood-end.jpg
	u8vec2(1, 0)  // water2.tga
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
