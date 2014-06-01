#include "DWT.h"
#include "basic.h"
#include <math.h>


#define DWT53	0
#define DWT97	1

#define PATCHX 32
#define PATCHY 32

DWT::DWT(KernelInitInfoBase initInfo) : initInfo(initInfo), f53(initInfo), r53(initInfo), f97(initInfo), r97(initInfo)
{
}


DWT::~DWT(void)
{
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
type_data* DWT::iwt_2d(short filter, type_tile_comp *tile_comp) {
	int i;
	int *sub_x, *sub_y ;
	/* Input data */
	type_data *d_idata = tile_comp->img_data_d;
	/* Result data */

	/* Image data size */
	const unsigned int smem_size = sizeof(type_data) * tile_comp->width * tile_comp->height;
	
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
	clEnqueueFillBuffer(initInfo.cmd_queue, d_odata, &pattern, sizeof(type_data), 0, smem_size, 0, NULL, NULL);


	cl_int2 img_size = {tile_comp->width, tile_comp->height};
	cl_int2 img_step = {tile_comp->width, tile_comp->height};

	sub_x = (int *)malloc((tile_comp->num_dlvls - 1) * sizeof(int));
	sub_y = (int *)malloc((tile_comp->num_dlvls - 1) * sizeof(int));

	for(i = 0; i < tile_comp->num_dlvls - 1; i++) {
		sub_x[i] = (img_size.s[0] % 2 == 1) ? 1 : 0;
		sub_y[i] = (img_size.s[1] % 2 == 1) ? 1 : 0;
		img_size.s[1] = (int)ceil(img_size.s[1]/2.0);
		img_size.s[0] = (int)ceil(img_size.s[0]/2.0);
	}

	for (i = 0; i < tile_comp->num_dlvls; i++) {
		/* Number of all thread blocks */
		cl_int3 grid_size = {(img_size.s[0] + (PATCHX - 1)) / (PATCHX), 
			                 (img_size.s[1] + (PATCHY - 1)) / (PATCHY),
							 1};

//		printf("OK gridx %d, ..gridy %d img_size.x %d img_size.y %d\n", grid_size.x, grid_size.y, img_size.x, img_size.y);


		switch(filter)
		{
			case DWT97:
				//iwt97<<<grid_size, dim3(BLOCKSIZEX, BLOCKSIZEY)>>>(d_idata, d_odata, img_size, stepSize);
				break;
			case DWT53:
				//iwt53_new<<<grid_size, dim3(BLOCKSIZEX, BLOCKSIZEY)>>>(d_idata, d_odata, img_size, stepSize);
				break;
		}


		/* Copy data between buffers to save previous results */
		if ((tile_comp->num_dlvls - 1) != i) {
				size_t bufferOffset[] = { 0, 0, 0};


		   // The region size must be given in bytes
			size_t region[] = { img_size.s[0] * sizeof(type_data), img_size.s[1], 1 };
			
			err = clEnqueueCopyBufferRect ( initInfo.cmd_queue, 	//copy command will be queued
				          (cl_mem)d_odata,		
						  (cl_mem)(d_idata),		
						  bufferOffset,	//offset associated with src_buffer
						  bufferOffset,     //offset associated with src_buffer
						  region,		//(width, height, depth) in bytes of the 2D or 3D rectangle being copied
						  tile_comp->width * sizeof(type_data),   //length of each row in bytes
						  0, //length of each 2D slice in bytes 
						  tile_comp->width * sizeof(type_data) ,   //length of each row in bytes
						  0, //length of each 2D slice in bytes
						  0,
						  NULL,
						  NULL);
			SAMPLE_CHECK_ERRORS(err);
						  
			img_size.s[0] = img_size.s[0] * 2 - sub_x[tile_comp->num_dlvls - 2 - i];
			img_size.s[1] = img_size.s[1] * 2 - sub_y[tile_comp->num_dlvls - 2 - i];
		}
	}

	free(sub_x);
	free(sub_y);

	err = clReleaseMemObject((cl_mem)d_idata);
    SAMPLE_CHECK_ERRORS(err);
	tile_comp->img_data_d = NULL;

	return (type_data*)d_odata;
}

void DWT::iwt(type_tile *tile)
{
//	println_start(INFO);
	int i;
	type_tile_comp *tile_comp;
	type_image *img = tile->parent_img;

//	save_img(img, "dec_dwt_before.bmp");

	/* Process components from tile */
	for(i = 0; i < tile->parent_img->num_components; i++)
	{
		tile_comp = &(tile->tile_comp[i]);

		/* Do IWT on image data. Lossy. */
		if(img->wavelet_type)
		{
			tile_comp->img_data_d = iwt_2d(DWT97, tile_comp);
		} else /* Lossless */
		{
			tile_comp->img_data_d = iwt_2d(DWT53, tile_comp);
		}
	}
//	save_img(img, "dec_dwt_after.bmp");
//	println_end(INFO);
}

