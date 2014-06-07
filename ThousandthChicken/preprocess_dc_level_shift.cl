// License: please see LICENSE2 file for more details.
#include "platform.cl"
#include "preprocess_constants.cl"


/**
 * @brief CUDA kernel for DC level shifting coder.
 *
 * It performs dc level shifting to centralize data around 0. Doesn't use
 * any sophisticated algorithms for this, just subtracts 128. (so the data range is [-128 ; 128] ).
 *
 * @param img The image data.
 * @param size Number of pixels in each component (width x height).
 * @param level_shift Level shift.
 */
void KERNEL fdc_level_shift_kernel(GLOBAL int *idata, const unsigned short width, const unsigned short height, const int level_shift) {
	idata[getGlobalId(0)] -=  1 << level_shift;
}
