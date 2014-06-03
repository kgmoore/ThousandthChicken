#include "platform.cl"
#include "preprocess_constants.h"
#include "preprocess_constants.cl"


/**
 * @brief CUDA kernel for DC level shifting decoder.
 *
 * It performs dc level shifting to centralize data around 0. Doesn't use
 * any sophisticated algorithms for this, just adds 128. (so the data range is [-128 ; 128] ).
 *
 * @param img The image data.
 * @param size Number of pixels in each component (width x height).
 * @param level_shift Level shift.
 */
void KERNEL idc_level_shift_kernel(GLOBAL int *idata, const unsigned short width, const unsigned short height, const int level_shift, const int min, const int max) {
	int i = getLocalId(0);
	int j = getLocalId(1);
	int n = i + getGroupId(0) * TILE_SIZEX;
	int m = j + getGroupId(1) * TILE_SIZEY;
	int idx = n + m * width;
	int cache;

	while(j < TILE_SIZEY && m < height)
	{
		while(i < TILE_SIZEX && n < width)
		{
			cache = idata[idx] + (1 << level_shift);
			idata[idx] = clamp_val(cache, min, max);
//			idata[idx] = idata[idx] + (1 << level_shift);
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
