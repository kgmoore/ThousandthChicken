// License: please see LICENSE2 file for more details.

#include "Quantizer.h"

#include "codestream_image_types.h"
#include "basic.h"


Quantizer::Quantizer(KernelInitInfoBase initInfo)  : 
	                    initInfo(initInfo)
	                   
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
}



type_subband* Quantizer::dequantization(type_subband *sb, void* coefficients)
{
	type_res_lvl *res_lvl = sb->parent_res_lvl;
	type_tile_comp *tile_comp = res_lvl->parent_tile_comp;
	type_image *img = tile_comp->parent_tile->parent_img;
	float convert_factor;
	int shift_bits;
	int max_res_lvl;
	unsigned int i;

	/* Lossy */
	if (img->wavelet_type)
	{
		/* Max resolution level */
		max_res_lvl = tile_comp->num_dlvls;
		/* Relative de-quantization step size. Step size is signaled relative to the wavelet coefficient bit depth. */
		convert_factor = sb->step_size
				* ((type_data)(1 << (img->num_range_bits + get_exp_subband_gain(sb->orient) + max_res_lvl - res_lvl->dec_lvl_no)));
		shift_bits = 31 - sb->mag_bits;

		convert_factor = convert_factor / ((type_data)(1 << shift_bits));

		sb->convert_factor = convert_factor;

//		println_var(INFO, "Lossy mag_bits:%d convert_factor:%0.16f shift_bits:%d step_size:%f subband_gain:%d", sb->mag_bits, /*sb->step_size
//				* ((type_data)(1 << (img->num_range_bits + get_exp_subband_gain(sb->orient) + max_res_lvl - res_lvl->dec_lvl_no)))*/sb->convert_factor, shift_bits, sb->step_size, get_exp_subband_gain(sb->orient));

	} else /* Lossless */
	{
		shift_bits = 31 - sb->mag_bits;
		sb->convert_factor = 1 << shift_bits;
//		printf("%d\n", shift_bits);
	}

	
	cl_int err = CL_SUCCESS;
	cl_context context  = NULL;

    // Obtaing the OpenCL context from the command-queue properties
	err = clGetCommandQueueInfo(initInfo.cmd_queue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(err));
		return 0;
	}

	//allocate device memory for coefficient data for all codeblocks from this sub band
	cl_mem d_subbandCodeblockCoefficients = clCreateBuffer(context, CL_MEM_READ_WRITE ,   sb->width * sb->height * sizeof(int), NULL, &err);
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
		size_t bufferOffset[] = { 0, 0};
	   // The region size must be given in bytes
		size_t region[] = { cblk->width * sizeof(int), cblk->height * sizeof(int) };

		clEnqueueCopyBufferRect ( initInfo.cmd_queue, 	//copy command will be queued
   					  (cl_mem)(d_coefficients + cblk->d_coefficientsOffset),		
   					  (cl_mem)((int*)d_subbandCodeblockCoefficients + cblk->tlx + cblk->tly * sb->width),		
					  bufferOffset,	//offset associated with src_buffer
					  bufferOffset,     //offset associated with src_buffer
					  region,		//(width, height, depth) in bytes of the 2D or 3D rectangle being copied
					  tile_comp->cblk_w * sizeof(int),   //length of each row in bytes
					  0, //length of each 2D slice in bytes 
					  sb->width * sizeof(int),   //length of each row in bytes
					  0, //length of each 2D slice in bytes
					  0,
					  NULL,
					  NULL);
				  
	}

	int odataOffset = sb->tlx + sb->tly * tile_comp->width;

	cl_int2 isize = {sb->width, sb->height};
	cl_int2 osize = {tile_comp->width, tile_comp->height};
	cl_int2 cblk_size = {tile_comp->cblk_w, tile_comp->cblk_h};

	DeviceKernel* quant = img->wavelet_type ? lossyKernel : losslessKernel;
	cl_kernel quantKernel =  quant->getKernel();

	/////////////////////////////////////
	//set kernel arguments
	err = clSetKernelArg(quantKernel, 0, sizeof(cl_mem), (void *) &d_subbandCodeblockCoefficients);
    SAMPLE_CHECK_ERRORS(err);
	
	err = clSetKernelArg(quantKernel, 1, sizeof(cl_int2), (void *) &isize);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(quantKernel, 2, sizeof(cl_mem), (void *)&tile_comp->img_data_d);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(quantKernel, 3, sizeof(int), (void *)&odataOffset);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(quantKernel, 4, sizeof(cl_int2), (void *) &osize);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(quantKernel, 5, sizeof(cl_int2), (void *) &cblk_size);
    SAMPLE_CHECK_ERRORS(err);

	if (img->wavelet_type) {
		err = clSetKernelArg(quantKernel, 6, sizeof(float), (void *) &sb->convert_factor);
	} else {
		err = clSetKernelArg(quantKernel, 6, sizeof(int), (void *) &shift_bits);
	}
	 SAMPLE_CHECK_ERRORS(err);

	/////////////////////////////////////////////////////////////////////////////////

	size_t global_work_size[3] = {sb->num_xcblks * BLOCKSIZEX,   sb->num_ycblks * BLOCKSIZEY,1};
	size_t local_work_size[3] = {BLOCKSIZEX, BLOCKSIZEY,1};
    // execute kernel
	quant->execute(2,global_work_size, local_work_size);
    SAMPLE_CHECK_ERRORS(err);

	err = clReleaseMemObject(d_subbandCodeblockCoefficients);
    SAMPLE_CHECK_ERRORS(err);

	return sb;
}

/**
 * @brief Do dequantization for every subbands from tile.
 * @param tile
 */
void Quantizer::dequantize_tile(type_tile *tile)
{
	//	println_start(INFO);

	type_image *img = tile->parent_img;
	type_tile_comp *tile_comp;
	type_res_lvl *res_lvl;
	type_subband *sb;
	int i, j, k;

	for (i = 0; i < img->num_components; i++)
	{
		tile_comp = &(tile->tile_comp[i]);
		for (j = 0; j < tile_comp->num_rlvls; j++)
		{
			res_lvl = &(tile_comp->res_lvls[j]);
			for (k = 0; k < res_lvl->num_subbands; k++)
			{
				sb = &(res_lvl->subbands[k]);
				dequantization(sb, tile->coefficients);
			}
		}
	}

	//release decoded coefficients buffer
	cl_int err = clReleaseMemObject((cl_mem)tile->coefficients);
    SAMPLE_CHECK_ERRORS(err);

	//	println_end(INFO);
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



