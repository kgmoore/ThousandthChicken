// License: please see LICENSE2 file for more details.
#include "platform.cl"
#include "preprocess_constants.h"
#include "preprocess_constants.cl"



typedef float type_data;


/**
 * @brief CUDA kernel for the Irreversible Color Transformation (lossy) decoder.
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
void KERNEL tci_kernel(GLOBAL int *img_r, GLOBAL int *img_g, GLOBAL int *img_b, const unsigned short width, const unsigned short height, const int level_shift, const int min, const int max) {
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
			y = img_b[idx];
			u = img_g[idx];
			v = img_r[idx];

			type_data r_tmp = v*( (1 - Wr)/Vmax );
			type_data b_tmp = u*( (1 - Wb)/Umax );

			r = y + r_tmp;
			b = y + b_tmp;
			g = y - (Wb/Wg) * r_tmp - (Wr/Wg) * b_tmp;

			b = (type_data)b + (1 << level_shift);
			g = (type_data)g + (1 << level_shift);
			r = (type_data)r + (1 << level_shift);

			img_b[idx] = clamp((int)b, min, max);
			img_g[idx] = clamp((int)g, min, max);
			img_r[idx] = clamp((int)r, min, max);

//			img_b[idx] = b + (1 << level_shift);
//			img_g[idx] = g + (1 << level_shift);
//			img_r[idx] = r + (1 << level_shift);

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