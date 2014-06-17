// License: please see LICENSE2 file for more details.

#include "Quantizer.h"
#include "quantizer_parameters.h"

#include "codestream_image_types.h"
#include "basic.h"


Quantizer::Quantizer(KernelInitInfoBase initInfo)  : 
	                    initInfo(initInfo),
						d_subbandCodeblockCoefficients(0)
	                   
{
	 losslessKernel = new DeviceKernel( KernelInitInfo(initInfo, "quantizer_lossless_inverse.cl", "subband_dequantization_lossless") );
	 lossyKernel = new DeviceKernel( KernelInitInfo(initInfo, "quantizer_lossy_inverse.cl", "subband_dequantization_lossy")) ;
}


Quantizer::~Quantizer(void)
{
	if (losslessKernel)
		delete losslessKernel;
	if (lossyKernel)
		delete lossyKernel;

	//release memory when finished dequant
	if (d_subbandCodeblockCoefficients) {
		cl_int err = clReleaseMemObject(d_subbandCodeblockCoefficients);
	    SAMPLE_CHECK_ERRORS(err);
	}

}



tDeviceRC Quantizer::dequantizationInit(type_subband *sb, void* coefficients)
{
	type_res_lvl *res_lvl = sb->parent_res_lvl;
	type_tile_comp *tile_comp = res_lvl->parent_tile_comp;
	type_image *img = tile_comp->parent_tile->parent_img;
	int max_res_lvl;
	unsigned int i;

	/* Lossy */
	if (img->wavelet_type)
	{
		/* Max resolution level */
		max_res_lvl = tile_comp->num_dlvls;
		/* Relative de-quantization step size. Step size is signaled relative to the wavelet coefficient bit depth. */
		sb->convert_factor = sb->step_size
				* ((type_data)(1 << (img->num_range_bits + get_exp_subband_gain(sb->orient) + max_res_lvl - res_lvl->dec_lvl_no)));
		sb->shift_bits = 31 - sb->mag_bits;

		sb->convert_factor /= ((type_data)(1 << sb->shift_bits));

//		println_var(INFO, "Lossy mag_bits:%d convert_factor:%0.16f shift_bits:%d step_size:%f subband_gain:%d", sb->mag_bits, /*sb->step_size
//				* ((type_data)(1 << (img->num_range_bits + get_exp_subband_gain(sb->orient) + max_res_lvl - res_lvl->dec_lvl_no)))*/sb->convert_factor, shift_bits, sb->step_size, get_exp_subband_gain(sb->orient));

	} else /* Lossless */
	{
		sb->shift_bits = 31 - sb->mag_bits;
		sb->convert_factor = 0; 
//		printf("%d\n", shift_bits);
	}

	
	cl_int err = CL_SUCCESS;
	cl_context context  = NULL;

    // Obtaing the OpenCL context from the command-queue properties
	err = clGetCommandQueueInfo(initInfo.cmd_queue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(err));
		return err;
	}

	//allocate device memory for coefficient data for all codeblocks from this sub band
	d_subbandCodeblockCoefficients = clCreateBuffer(context, CL_MEM_READ_WRITE ,   sb->width * sb->height * sizeof(int), NULL, &err);
    SAMPLE_CHECK_ERRORS(err);
    if (d_subbandCodeblockCoefficients == (cl_mem)0)
        throw Error("Failed to create d_decodedCoefficientsBuffers Buffer!");

	//printf("%d %d %d\n", sb->num_cblks, tile_comp->cblk_w, tile_comp->cblk_h);

	
	int* d_coefficients = (int*)coefficients;
	// fill subband coefficients buffer
	for (i = 0; i < sb->num_cblks; i++)
	{

		type_codeblock *cblk = &(sb->cblks[i]);

		//printf("%d %d %d %d %d %d %d\n", cblk->tlx, cblk->tly, cblk->width, cblk->height, sb->width, sb->height, cblk->tlx + cblk->tly * sb->width);

	    // copy decoded coefficients from code block device memory to sub band code block device memory
		size_t bufferOffsetSrc[] = { cblk->d_coefficientsOffset * sizeof(int), 0,0};
	    size_t bufferOffsetDst[] = { cblk->tlx * sizeof(int), cblk->tly,0};
	   // The region size must be given in bytes
		size_t region[] = { cblk->width * sizeof(int), cblk->height,1};
		
		err = clEnqueueCopyBufferRect ( initInfo.cmd_queue, 	//copy command will be queued
   					  (cl_mem)(d_coefficients),		
   					  d_subbandCodeblockCoefficients,		
					  bufferOffsetSrc,	//offset associated with src_buffer
					  bufferOffsetDst,     //offset associated with src_buffer
					  region,		//(width, height, depth) in bytes of the 2D or 3D rectangle being copied
					  tile_comp->cblk_w * sizeof(int),   //length of each row in bytes
					  0, //length of each 2D slice in bytes 
					  sb->width * sizeof(int),   //length of each row in bytes
					  0, //length of each 2D slice in bytes
					  0,
					  NULL,
					  NULL);
		if (CL_SUCCESS != err)
		{
			LogError("Error: clEnqueueCopyBufferRect (srcMem) returned %s.\n", TranslateOpenCLError(err));
		}
		return CL_SUCCESS;
				  
	}
	return DeviceSuccess;
}

type_subband* Quantizer::dequantization(type_subband *sb, void** coefficients)
{
	type_res_lvl *res_lvl = sb->parent_res_lvl;
	type_tile_comp *tile_comp = res_lvl->parent_tile_comp;
	int odataOffset = sb->tlx + sb->tly * tile_comp->width;

	cl_int2 isize = {sb->width, sb->height};
	cl_int2 osize = {tile_comp->width, tile_comp->height};
	cl_int2 cblk_size = {tile_comp->cblk_w, tile_comp->cblk_h};

	type_image *img = tile_comp->parent_tile->parent_img;
	DeviceKernel* quant = img->wavelet_type ? lossyKernel : losslessKernel;
	cl_kernel quantKernel =  quant->getKernel();

	/////////////////////////////////////
	//set kernel arguments
	int argNum = 0;
	tDeviceRC err = clSetKernelArg(quantKernel, argNum++, sizeof(cl_mem), coefficients);
    SAMPLE_CHECK_ERRORS(err);
	
	err = clSetKernelArg(quantKernel, argNum++, sizeof(cl_int2),  &isize);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(quantKernel, argNum++, sizeof(cl_mem), &tile_comp->img_data_d);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(quantKernel, argNum++, sizeof(int), &odataOffset);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(quantKernel, argNum++, sizeof(cl_int2), &osize);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(quantKernel, argNum++, sizeof(cl_int2),  &cblk_size);
    SAMPLE_CHECK_ERRORS(err);

	if (img->wavelet_type) {
		err = clSetKernelArg(quantKernel, argNum++, sizeof(float),  &sb->convert_factor);
	} else {
		err = clSetKernelArg(quantKernel, argNum++, sizeof(int),  &sb->shift_bits);
	}
	SAMPLE_CHECK_ERRORS(err);

	size_t global_work_size[3] = {sb->num_xcblks * BLOCKSIZEX,   sb->num_ycblks * BLOCKSIZEY,1};
	size_t local_work_size[3] = {BLOCKSIZEX, BLOCKSIZEY,1};
    // execute kernel
	quant->enqueue(2,global_work_size, local_work_size);
    SAMPLE_CHECK_ERRORS(err);

	return sb;
}

/**
 * @brief Do dequantization for every subbands from tile.
 * @param tile
 */
void Quantizer::dequantize_tile(type_tile *tile)
{
	type_image *img = tile->parent_img;
	for (int i = 0; i < img->num_components; i++)
	{
		type_tile_comp *tile_comp = tile->tile_comp + i;
		for (int j = 0; j < tile_comp->num_rlvls; j++)
		{
			type_res_lvl *res_lvl = tile_comp->res_lvls + j;
			for (int k = 0; k < res_lvl->num_subbands; k++)
			{
				type_subband *sb = res_lvl->subbands + k;
				if (dequantizationInit(sb, tile->coefficients) != DeviceSuccess)
					return;
			}
		}
	}
	clFinish(initInfo.cmd_queue);
	for (int i = 0; i < img->num_components; i++)
	{
		type_tile_comp *tile_comp = tile->tile_comp + i;
		for (int j = 0; j < tile_comp->num_rlvls; j++)
		{
			type_res_lvl *res_lvl = tile_comp->res_lvls + j;
			for (int k = 0; k < res_lvl->num_subbands; k++)
			{
				type_subband *sb = res_lvl->subbands + k;
				dequantization(sb, &tile->coefficients);
			}
		}
	}
	//release decoded coefficients buffer
	//TODO: release memory when finished dequant
	//cl_int err = clReleaseMemObject((cl_mem)tile->coefficients);
    //SAMPLE_CHECK_ERRORS(err);
}

/**
 * @brief Gets the base 2 exponent of the subband gain.
 * @param orient
 * @return
 */
int Quantizer::get_exp_subband_gain(int orient)
{
	return (orient & 1) + ((orient >> 1) & 1);
}



