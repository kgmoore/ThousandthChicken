#include "platform.cl"
#include "preprocess_constants.h"
#include "preprocess_constants.cl"



typedef float type_data;

/**
 * @brief CUDA kernel for the Irreversible Color Transformation (lossy) coder.
 *
 * Before colorspace transformation it performs dc level shifting to centralize data around 0. Doesn't use
 * any sophisticated algorithms for this, just subtracts 128. (so the data range is [-128 ; 128] ).
 *
 * @param img_r 1D array with RED component of the image.
 * @param img_g 1D array with GREEN component of the image.
 * @param img_b 1D array with BLUE component of the image.
 * @param size Number of pixels in each component (width x height).
 */
void KERNEL ict_kernel(GLOBAL type_data *img_r, GLOBAL type_data *img_g, GLOBAL type_data *img_b, const unsigned short width, const unsigned short height, const int level_shift) {

	int i = getLocalId(0);
	int j = getLocalId(1);
	int n = i + getGroupId(0) * TILE_SIZEX;
	int m = j + getGroupId(1) * TILE_SIZEY;
	int idx = n + m * width;
	type_data r, g, b;
	type_data y, u, v;

	while(j < TILE_SIZEY && m < height)
	{
		while(i < TILE_SIZEX && n < width)
		{
			b = img_r[idx] - (1 << level_shift);
			g = img_g[idx] - (1 << level_shift);
			r = img_b[idx] - (1 << level_shift);

			y = Wr*r + Wg*g + Wb*b;
			u = -0.16875f * r - 0.33126f * g + 0.5f * b;
//			u = (Umax * ((b - y) / (1 - Wb)));
			v = 0.5f * r - 0.41869f * g - 0.08131f * b;
//			v = (Vmax * ((r - y) / (1 - Wr)));

			img_r[idx] = y;
			img_g[idx] = u;
			img_b[idx] = v;

/*			img_r[idx] = y;
			img_g[idx] = u;
			img_b[idx] = v;*/

			i += BLOCK_SIZE;
			n = i + getGroupId(0) * TILE_SIZEX;
			idx = n + m * width;
		}
		i = getLocalId(0);
		j += BLOCK_SIZE;
		n = i + getGroupId(0) * TILE_SIZEX;
		m = j + getGroupId(1) * TILE_SIZEY;
		idx = n + m * width;
	}
}
