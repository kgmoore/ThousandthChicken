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

	int i = getLocalId(0);
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
			b = img_b[idx] - (1 << level_shift);
			g = img_g[idx] - (1 << level_shift);
			r = img_r[idx] - (1 << level_shift);

			y = (r + 2*g + b)>>2;
			u = b - g;
			v = r - g;

			img_r[idx] = y;
			img_b[idx] = u;
			img_g[idx] = v;

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



/*

// The kernel is forced to work with local work size of 16x8x1 -
// each work-group includes 128 pixels or 32 work-items (each work-item deals with 4 pixels).
// The input and output buffer (src and dst) are defined as uchar4 to ensure 32-bit alignment
__attribute__((reqd_work_group_size(8, 8, 1)))
__kernel void ConvertRGB2YUV(__global uchar4* src, __global uchar4* dst, uint width, uint height, uint srcStride, uint dstStride)
{
    // Conversion constants
    const float WR = 0.299f;
    const float WG = 0.587f;
    const float WB = 0.114f;
    const float UMax = 0.492f;
    const float VMax = 0.877f;
    const int Delta = 128;

    const size_t numberOfPixelsPerWorkItem = 4;
    const size_t srcElementSize = 4;                //each pixel has 4 channels
    const size_t dstElementSize = 4;                //each pixel has 4 channels

    // To find the x-address in bytes, we take the x-index of the first pixel in the work-item
    // and multiply it by the number of bytes (channels) per pixel
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint srcX = x * numberOfPixelsPerWorkItem * srcElementSize;
    const uint dstX = x * numberOfPixelsPerWorkItem * dstElementSize;

    // Calculter the byte-offset of the first pixel in the image = y * stride + x
    // y * stride give us the offset in bytes of the y-line
    size_t srcIndex = mad24(y, srcStride, srcX);
    size_t dstIndex = mad24(y, dstStride, dstX);

    // Since the images are 4-channels images, we're using char4 for each pixel
    global uchar4 *srcPtr = (global uchar4 *)((global uchar *)src + srcIndex);
    global uchar4 *dstPtr = (global uchar4 *)((global uchar *)dst + dstIndex);

    // Read RGB channels for 4 pixels and aggregate them in channel vectors
    // Here we're taking advantage of the fact that we're working with char4 pointer
    // So, in each vector s0 ir the R-channel, s1 is the G-channel and s2 is the B-channel
    uchar4 ru = (uchar4)(srcPtr[0].s0, srcPtr[1].s0, srcPtr[2].s0, srcPtr[3].s0);
    uchar4 gu = (uchar4)(srcPtr[0].s1, srcPtr[1].s1, srcPtr[2].s1, srcPtr[3].s1);
    uchar4 bu = (uchar4)(srcPtr[0].s2, srcPtr[1].s2, srcPtr[2].s2, srcPtr[3].s2);

    // To do the conversion we must use float values
    float4 rf = convert_float4(ru);
    float4 gf = convert_float4(gu);
    float4 bf = convert_float4(bu);

    // Do the actual conversion
    float4 yf = WR*rf + WG*gf + WB*bf;
    float4 uf = UMax*(bf-yf) + Delta;
    float4 vf = VMax*(rf-yf) + Delta;

    // The output image data-type is uchar, so we're converting the results back
    uchar4 yu = convert_uchar4_sat_rte(yf);
    uchar4 uu = convert_uchar4_sat_rte(uf);
    uchar4 vu = convert_uchar4_sat_rte(vf);

    // Write the pixels to the destination image
    // Since we're working on 4 pixels with uchar4 vectors
    // The s0 members are the Y-channels, s1 are the U-channels, and s2 are the V-channels
    dstPtr[0] = (uchar4)(yu.s0, uu.s0, vu.s0, 0);
    dstPtr[1] = (uchar4)(yu.s1, uu.s1, vu.s1, 0);
    dstPtr[2] = (uchar4)(yu.s2, uu.s2, vu.s2, 0);
    dstPtr[3] = (uchar4)(yu.s3, uu.s3, vu.s3, 0);
}

*/