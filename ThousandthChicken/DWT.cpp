// License: please see LICENSE4 file for more details.

#include "DWT.h"
#include "basic.h"
#include <math.h>
#include <malloc.h>


#define DWT53	0
#define DWT97	1

#define PATCHX 32
#define PATCHY 32

DWT::DWT(KernelInitInfoBase initInfo) : initInfo(initInfo)
{
	f53 = new DWTForward53(initInfo);
	r53 = new DWTReverse53(initInfo);
	f97 = new DWTForward97(initInfo);
	r97 = new DWTReverse97(initInfo);
}


DWT::~DWT(void)
{
	if (f53)
		delete f53;
	if (r53)
		delete r53;
	if (f97)
		delete f97;
	if (r97)
		delete r97;
}



/**
 * @brief Perform the inverse wavelet transform on a 2D matrix
 *
 * We assume that top left coordinates u0 and v0 input tile matrix are both even.See Annex F of ISO/EIC IS 15444-1.
 *
 * @param filter Kind of wavelet 53 | 97.
 * @param d_idata Input array.
 * @param d_odata Output array.
 * @param img_size Input image size.
 * @param step Output image size.
 * @param nlevels Number of levels.
 */
tDeviceMem DWT::iwt_2d(short filter, type_tile_comp *tile_comp) {
	/* Input data */
	tDeviceMem  d_idata = (tDeviceMem)tile_comp->img_data_d;
	/* Result data */

	int dataSize = tile_comp->parent_tile->parent_img->wavelet_type ? sizeof(type_data) : sizeof(int);

	/* Image data size */
	const unsigned int smem_size = dataSize * tile_comp->width * tile_comp->height;
	
    cl_int err = CL_SUCCESS;
	cl_context context  = NULL;

    // Obtaing the OpenCL context from the command-queue properties
	err = clGetCommandQueueInfo(initInfo.cmd_queue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(err));
		return 0;
	}

	//allocate d_odata on device and initialize it to zero
	cl_mem d_odata = clCreateBuffer(context, CL_MEM_READ_WRITE ,  smem_size, NULL, &err);
    SAMPLE_CHECK_ERRORS(err);
    if (d_odata == (cl_mem)0)
        throw Error("Failed to create d_odata Buffer!");
	cl_int pattern = 0;
	clEnqueueFillBuffer(initInfo.cmd_queue, d_odata, &pattern, dataSize, 0, smem_size, 0, NULL, NULL);
	switch(filter)
	{
		case DWT97:
			r97->run(d_idata, d_odata, tile_comp->width, tile_comp->height, tile_comp->num_dlvls);
			break;
		case DWT53:
			r53->run(d_idata, d_odata, tile_comp->width, tile_comp->height, tile_comp->num_dlvls);
			break;
	}

	err = clReleaseMemObject((cl_mem)d_idata);
    SAMPLE_CHECK_ERRORS(err);
	tile_comp->img_data_d = d_odata;
	return d_odata;
}

void DWT::iwt(type_tile *tile)
{
//	println_start(INFO);
	int i;
	type_tile_comp *tile_comp;
	type_image *img = tile->parent_img;

	/* Process components from tile */
	for(i = 0; i < tile->parent_img->num_components; i++)
	{
		tile_comp = &(tile->tile_comp[i]);
		// error condition if null
		if (!iwt_2d(img->wavelet_type ? DWT97 : DWT53, tile_comp))
			return;
	}
//	println_end(INFO);
}

