#include "inc/noise.glsl"

int GetGroundLevel(int global_x, int global_y, int seed)
{
	// HACK. If not doing this, borders parallel to world X axis are to sharply.
	int global_y_corrected= global_y - (global_x & 1);

	// Add several octaves of triangle-interpolated noise.
	// Use seed with offset to avoid fractal noise apperiance at world center (0, 0).
	int noise=
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed + 0, 6)     ) +
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed + 1, 5) >> 1) +
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed + 2, 4) >> 2) +
		(hex_TriangularInterpolatedNoiseDefault(global_y_corrected, global_x, seed + 3, 3) >> 3);

	// TODO - scale result noise depending on current biome.
	int noise_scaled= noise >> 11;

	int base_ground_value= 2; // TODO - choose base value depending on current biome.

	return max(3, min(base_ground_value + noise_scaled, c_chunk_height - 2));
}
