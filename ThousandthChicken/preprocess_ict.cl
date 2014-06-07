// License: please see LICENSE2 file for more details.
#include "platform.cl"
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
void KERNEL ict_kernel(GLOBAL int *img_r, GLOBAL int *img_g, GLOBAL int *img_b, const unsigned short width, const unsigned short height, const int level_shift) {

    int dcShift = 1 << level_shift;
	int index = getGlobalId(0);

	int r = img_r[index] - dcShift;
	int g = img_g[index] - dcShift;
	int b = img_b[index] - dcShift;

	float y = Wr*r + Wg*g + Wb*b;
	float u = -0.16875f * r - 0.33126f * g + 0.5f * b;
	float v = 0.5f * r - 0.41869f * g - 0.08131f * b;

	img_r[index] = convert_int_sat_rte(y);
    img_g[index] = convert_int_sat_rte(u);
    img_b[index] = convert_int_sat_rte(v);

	
}
