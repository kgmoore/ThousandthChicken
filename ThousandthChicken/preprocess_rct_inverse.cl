#include "platform.cl"
#include "preprocess_constants.h"
#include "preprocess_constants.cl"


/**
 * @brief CUDA kernel for the Reversible Color Transformation (lossless) decoder.
 *
 *
 * After colorspace transformation it performs dc level shifting to shift data back to it's unsigned form,
 * just adds 128. (so the data range is [0 ; 256] ).
 *
 * @param img_r 1D array with V component of the image.
 * @param img_g 1D array with U component of the image.
 * @param img_b 1D array with Y component of the image.
 * @param size Number of pixels in each component (width x height).
 */
//void __global__ tcr_kernel(type_data *img_r, type_data *img_g, type_data *img_b, long int size) {
void KERNEL tcr_kernel(GLOBAL int *img_r, GLOBAL int *img_g, GLOBAL int *img_b, const unsigned short width, const unsigned short height, const int level_shift, const int min, const int max) {
	    int i =getLocalId(0);
		int j = getLocalId(1);
		int n = i + getGroupId(0) * TILE_SIZEX;
		int m = j + getGroupId(1) * TILE_SIZEY;
		int idx = n + m * width;
		int r, g, b;
		int y, u, v;

		while(j < TILE_SIZEY && m < height)
		{
			while(i < TILE_SIZEX && n < width)
			{
				y = img_r[idx];
				u = img_g[idx];
				v = img_b[idx];


				g = y - ((v + u)>>2);
				r = (v + g);
				b = (u + g);

				b = b + (1 << level_shift);
				g = g + (1 << level_shift);
				r = r + (1 << level_shift);

				img_r[idx] = clamp_val(r, min, max);
				img_b[idx] = clamp_val(g, min, max);
				img_g[idx] = clamp_val(b, min, max);

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
