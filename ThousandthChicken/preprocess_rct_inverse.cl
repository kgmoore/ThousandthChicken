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
void KERNEL tcr_kernel(GLOBAL int *img_r, GLOBAL int *img_g, GLOBAL int *img_b, const unsigned short width, const unsigned short height, const int level_shift, const int minimum, const int maximum) {

   int dcShift = 1 << level_shift;
   int index = getGlobalId(0);
   if (index >= width*height)
	    return;
    
    
	int y = img_r[index];
	int u = img_g[index];
	int v = img_b[index];

	int g = y - ((v + u)>>2);
	int r = (v + g);
	int b = (u + g);
	
	img_r[index] = clamp( r + dcShift, minimum, maximum);
	img_g[index] =  clamp( g + dcShift, minimum, maximum);
	img_b[index] = clamp( b + dcShift, minimum, maximum);


}
