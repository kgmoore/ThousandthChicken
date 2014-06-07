// License: please see LICENSE2 file for more details.

#include "platform.cl"


#define BLOCKSIZEX 16
#define BLOCKSIZEY 16

typedef float type_data;

/**
 * @brief Subband dequantization.
 *
 * @param idata Input tile_comp_data.
 * @param size Width and height of subbnad.
 * @param step_size Step size(deltab).
 */
KERNEL  void subband_dequantization_lossless(GLOBAL int *idata, int2 isize, GLOBAL int *odata, int odataOffset, int2 osize, int2 cblk_size, const int shift_bits)
{
	int i = getLocalId(0);
	int j = getLocalId(1);
	int n = i + getGroupId(0) * cblk_size.x;
	int m = j + getGroupId(1) * cblk_size.y;
	int in = n + m * isize.x;
	int out = n + m * osize.x;

	odata += odataOffset;

	while (j < cblk_size.y && m < isize.y)
	{
		while (i < cblk_size.x && n < isize.x)
		{
			odata[out] = idata[in] >= 0 ? idata[in] >> shift_bits : -((idata[in] & 0x7FFFFFFF) >> shift_bits);
			i += BLOCKSIZEX;
			n = i +  getGroupId(0) * cblk_size.x;
			in = n + m * isize.x;
			out = n + m * osize.x;
		}
		i = getLocalId(0);
		j += BLOCKSIZEY;
		n = i + getGroupId(0) * cblk_size.x;
		m = j + getGroupId(1) * cblk_size.y;
		in = n + m * isize.x;
		out = n + m * osize.x;
	}
}