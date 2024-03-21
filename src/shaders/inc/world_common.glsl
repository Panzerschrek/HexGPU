const float c_hexagon_fetch_lod_edge= 0.5;

ivec2 GetHexagonTexCoordY(vec2 tex_coord)
{
	vec2 tex_coord_dransformed;
	vec2 tex_coord_transformed_floor;
	ivec2 nearest_cell;
	vec2 d;

	tex_coord_dransformed.y= tex_coord.x;
	tex_coord_transformed_floor.y= floor(tex_coord_dransformed.y);
	nearest_cell.y= int(tex_coord_transformed_floor.y);
	d.y= tex_coord_dransformed.y - tex_coord_transformed_floor.y;

	tex_coord_dransformed.x= tex_coord.y - 0.5 * float((nearest_cell.y ^ 1) & 1);
	tex_coord_transformed_floor.x= floor( tex_coord_dransformed.x );
	nearest_cell.x= int( tex_coord_transformed_floor.x );
	d.x= tex_coord_dransformed.x - tex_coord_transformed_floor.x;

	if(d.x > 0.5 + 1.5 * d.y)
		return ivec2(nearest_cell.x + ((nearest_cell.y ^ 1) & 1), nearest_cell.y - 1).yx;

	else
	if(d.x < 0.5 - 1.5 * d.y)
		return ivec2(nearest_cell.x - (nearest_cell.y & 1), nearest_cell.y - 1).yx;

	else
		return nearest_cell.yx;
}

vec4 HexagonFetch(in sampler2DArray tex, vec3 tex_coord)
{
	float lod= textureQueryLod(tex, tex_coord.xy).x;

	if(lod <= c_hexagon_fetch_lod_edge)
	{
		ivec2 tex_size= textureSize( tex, 0 ).xy;
		int texture_layer= int(tex_coord.z + 0.49);

		return texelFetch(
			tex,
			ivec3(
				mod(GetHexagonTexCoordY(tex_coord.xy * vec2(tex_size) - vec2(0.25, 0.0)), tex_size),
				texture_layer),
			0);
	}
	else
		return texture(tex, tex_coord);
}

vec4 HexagonFetch(in sampler2D tex, vec2 tex_coord)
{
	float lod= textureQueryLod(tex, tex_coord).x;

	if(lod <= c_hexagon_fetch_lod_edge)
	{
		ivec2 tex_size= textureSize(tex, 0);

		return texelFetch(
			tex,
			ivec2(mod(GetHexagonTexCoordY(tex_coord * vec2(tex_size) - vec2(0.25, 0.0)), tex_size)),
			0);
	}
	else
		return texture(tex, tex_coord);
}

vec3 CombineLight(vec3 fire_light, vec3 sky_light, vec3 ambient_light)
{
	// Use special formula to make one light source weaker when other like source value is big.
	const vec3 c_one= vec3(1.0, 1.0, 1.0);
	const float c_mul= 0.28;
	return
		fire_light * (c_one - c_mul * sky_light) +
		sky_light * (c_one - c_mul * fire_light) +
		ambient_light;
}
