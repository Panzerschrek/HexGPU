// Basic 2-dimensional noise function with possible seed.
// Returns value in range [0; 65536).
int hex_Noise2(int x, int y, int seed)
{
	const int X_NOISE_GEN   =  1619;
	const int Y_NOISE_GEN   = 31337;
	const int Z_NOISE_GEN   =  6971;
	const int SEED_NOISE_GEN=  1013;

	int n= (
		X_NOISE_GEN * x +
		Y_NOISE_GEN * y +
		Z_NOISE_GEN * 0 +
		SEED_NOISE_GEN * seed )
		& 0x7fffffff;

	n= ( n >> 13 ) ^ n;
	return ( ( n * ( n * n * 60493 + 19990303 ) + 1376312589 ) & 0x7fffffff ) >> 15;
}

// Returns noise in a triangular grid,
// where random values are defined for triangle vertices and result is interpolated between them.
int hex_TriangularInterpolatedNoise(int x, int y, int seed, int shift, int coord_mask)
{
	int X= x >> shift, Y= y >> shift;
	int shift_pow2= 1 << shift;
	int mask= shift_pow2 - 1;

	int dx= x & mask, dy= y & mask;
	int dy1= shift_pow2 - dy, dx1= shift_pow2 - dx;

	int noise[3];

	if((Y & 1) != 0)
	{
		// left side: y >= 2 * x
		// right side: y >= shift_pow2 * 2 - 2* x
		/*
			+----------+
			|    /\    |
			|   /  \   |
			|  /    \  | shift_pow2
			| /      \ |
			|/        \|
			+----------+
			 shift_pow2
		*/

		if(dy >= 2 * dx)
		{
			noise[0]= hex_Noise2((X  ) & coord_mask, (Y  ) & coord_mask, seed);
			noise[1]= hex_Noise2((X  ) & coord_mask, (Y+1) & coord_mask, seed);
			noise[2]= hex_Noise2((X+1) & coord_mask, (Y+1) & coord_mask, seed);

			dx-= (dy1>>1) - (shift_pow2>>1);
			dx1= shift_pow2 - dy1 - dx;

			return (
					noise[0] * dy1 +
					noise[1] * dx1 +
					noise[2] * dx
				) >> shift;
		}
		else if(dy >= shift_pow2 * 2 - 2 * dx)
		{
			noise[0]= hex_Noise2((X+1) & coord_mask, (Y  ) & coord_mask, seed);
			noise[1]= hex_Noise2((X+1) & coord_mask, (Y+1) & coord_mask, seed);
			noise[2]= hex_Noise2((X+2) & coord_mask, (Y+1) & coord_mask, seed);

			dx-= (shift_pow2>>1) + (dy1>>1);
			dx1= shift_pow2 - dy1 - dx;

			return (
					noise[0] * dy1 +
					noise[2] * dx +
					noise[1] * dx1
				) >> shift;
		}
		else
		{
			noise[0]= hex_Noise2((X  ) & coord_mask, (Y  ) & coord_mask, seed);
			noise[1]= hex_Noise2((X+1) & coord_mask, (Y  ) & coord_mask, seed);
			noise[2]= hex_Noise2((X+1) & coord_mask, (Y+1) & coord_mask, seed);

			dx -= dy >> 1;
			dx1= shift_pow2 - dy - dx;

			return (
					noise[2] * dy +
					noise[0] * dx1 +
					noise[1] * dx
				) >> shift;
		}
	}
	else
	{
		// left side: y <= shift_pow2 - 2 * x
		// right side: y <= 2 * x - shift_pow2
		/*
			+----------+
			|\        /|
			| \      / |
			|  \    /  | shift_pow2
			|   \  /   |
			|    \/    |
			+----------+
			 shift_pow2
		*/
		if(dy <= shift_pow2 - 2 * dx)
		{
			noise[0]= hex_Noise2((X  ) & coord_mask, (Y  ) & coord_mask, seed);
			noise[1]= hex_Noise2((X+1) & coord_mask, (Y  ) & coord_mask, seed);
			noise[2]= hex_Noise2((X  ) & coord_mask, (Y+1) & coord_mask, seed);

			dx+= (shift_pow2>>1) - (dy>>1);
			dx1= shift_pow2 - dy - dx;

			return (
					noise[2] * dy +
					noise[0] * dx1 +
					noise[1] * dx
				) >> shift;
		}
		else if(dy <= 2 * dx - shift_pow2)
		{
			noise[0]= hex_Noise2((X+1) & coord_mask, (Y  ) & coord_mask, seed);
			noise[1]= hex_Noise2((X+2) & coord_mask, (Y  ) & coord_mask, seed);
			noise[2]= hex_Noise2((X+1) & coord_mask, (Y+1) & coord_mask, seed);

			dx-= (shift_pow2>>1) + (dy>>1);
			dx1= shift_pow2 - dy - dx;

			return (
					noise[2] * dy +
					noise[0] * dx1 +
					noise[1] * dx
				) >> shift;
		}
		else
		{
			noise[0]= hex_Noise2((X+1) & coord_mask, (Y  ) & coord_mask, seed);
			noise[1]= hex_Noise2((X  ) & coord_mask, (Y+1) & coord_mask, seed);
			noise[2]= hex_Noise2((X+1) & coord_mask, (Y+1) & coord_mask, seed);

			dx-= dy1 >> 1;
			dx1= shift_pow2 - dy1 - dx;

			return (
					noise[0] * dy1 +
					noise[1] * dx1 +
					noise[2] * dx
				) >> shift;
		}
	}
}

int hex_TriangularInterpolatedNoiseDefault(int x, int y, int seed, int shift)
{
	return hex_TriangularInterpolatedNoise(x, y, seed, shift, 0xFFFFFFFF);
}
