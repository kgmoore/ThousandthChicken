// License: please see LICENSE2 file for more details.
#include "platform.cl"
#include "preprocess_constants.cl"



typedef float type_data;


/**
 * @brief CUDA kernel for the Irreversible Color Transformation (lossy) decoder.
 *
 *
 * After colorspace transformation it performs dc level shifting to shift data back to it's unsigned form,
 * just adds 128. (so the data range is [0 ; 256] ).
 *
 * @param img_r 1D array with Y component of the image.
 * @param img_g 1D array with U component of the image.
 * @param img_b 1D array with V component of the image.
 * @param size Number of pixels in each component (width x height).
 */
void KERNEL tci_kernel(GLOBAL int *img_r, GLOBAL int *img_g, GLOBAL int *img_b, const unsigned short width, const unsigned short height, const int level_shift, const int min, const int max) {

    int dcShift = 1 << level_shift;
	int index = getGlobalId(0);

	int y = img_r[index];
	int u = img_g[index];
	int v = img_b[index];

	float r_tmp = v*( (1 - Wr)/Vmax );
	float b_tmp = u*( (1 - Wb)/Umax );

	int r = y + r_tmp + dcShift;
	int g = y - (Wb/Wg) * r_tmp - (Wr/Wg) * b_tmp + dcShift;
	int b = y + b_tmp + dcShift;

	img_r[index] = clamp(convert_int_sat_rte(r), min, max);
	img_g[index] = clamp(convert_int_sat_rte(g), min, max);
	img_b[index] = clamp(convert_int_sat_rte(b), min, max);

}