#include "platform.cl"
#include "preprocess_constants.h"
#include "preprocess_constants.cl"



/**
 * @brief CUDA kernel for the Reversible Color Transformation (lossless) coder.
 *
 * Before colorspace transformation it performs dc level shifting to centralize data around 0. Doesn't use
 * any sophisticated algorithms for this, just subtracts 128. (so the data range is [-128 ; 128] ).
 *
 * @param img_r 1D array with RED component of the image.
 * @param img_g 1D array with GREEN component of the image.
 * @param img_b 1D array with BLUE component of the image.
 * @param size Number of pixels in each component (width x height).
 */
void KERNEL rct_kernel(GLOBAL int *img_r, GLOBAL int *img_g, GLOBAL int *img_b, const unsigned short width, const unsigned short height, const int level_shift) {

    int dcShift = 1 << level_shift;
	int index = getGlobalId(0);
	if (index >= width*height)
	    return;
    
	int r = img_r[index];
	int g = img_g[index];
	int b = img_b[index];
	
	img_r[index] = (r + 2*g + b)>>2 - dcShift;
	img_g[index] = b - g;
	img_b[index] =  r - g;

}

