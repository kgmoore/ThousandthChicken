#include "Preprocessor.h"
#include "codestream_image_types.h"
#include "preprocess_constants.h"
#include "logger.h"

Preprocessor::Preprocessor(KernelInitInfoBase initInfo) :
											initInfo(initInfo),
											ict(new GenericKernel( KernelInitInfo(initInfo, "preprocess_ict.cl", "ict_kernel") )),
											ictInverse(new GenericKernel( KernelInitInfo(initInfo, "preprocess_ict_inverse.cl", "tci_kernel") )),
											rct(new GenericKernel( KernelInitInfo(initInfo, "preprocess_rct.cl", "rct_kernel") )),
											rctInverse(new GenericKernel( KernelInitInfo(initInfo, "preprocess_rct_inverse.cl", "tcr_kernel") )),
											dcShift(new GenericKernel( KernelInitInfo(initInfo, "preprocess_dc_level_shift.cl", "fdc_level_shift_kernel") )),
											dcShiftInverse(new GenericKernel( KernelInitInfo(initInfo, "preprocess_dc_level_shift_inverse.cl", "idc_level_shift_kernel") ))

{
}


Preprocessor::~Preprocessor(void)
{
	if (ict)
		delete ict;
	if (ictInverse)
		delete ictInverse;
	if (rct)
		delete rct;
	if (rctInverse)
		delete rctInverse;
	if (dcShift)
		delete dcShift;
	if (dcShiftInverse)
		delete dcShiftInverse;
}

/**
 * @brief Main function of color transformation flow. Should not be called directly though. Use four wrapper functions color_[de]coder_loss[y|less] instead.
 *
 * @param img type_image to will be transformed.
 * @param type Type of color transformation that should be performed. The types are detailed in color_trans_type.
 *
 *
 * @return Returns 0 on success.
 */
int Preprocessor::color_trans_gpu(type_image *img, color_trans_type type) {
	if(img->num_components != 3) {
		println(INFO, "Error: Color transformation not possible. The number of components != 3.");
		return -1;
	}

	unsigned int tile_size = 0, i;
	type_tile *tile;
	int level_shift = img->num_range_bits - 1;

	int min = img->sign == SIGNED ? -(1 << (img->num_range_bits - 1)) : 0;
	int max = img->sign == SIGNED ? (1 << (img->num_range_bits - 1)) - 1 : (1 << img->num_range_bits) - 1;

	// http://www.jpeg.org/.demo/FAQJpeg2k/functionalities.htm#What is the RCT?
	switch(type) {
	case RCT:
///		println_var(INFO, "start: RCT");
		for(i = 0; i < img->num_tiles; i++) {
			tile = &(img->tile[i]);
			int* comp_a = (int*)(&(tile->tile_comp[0]))->img_data_d;
			int* comp_b = (int*)(&(tile->tile_comp[1]))->img_data_d;
			int* comp_c = (int*)(&(tile->tile_comp[2]))->img_data_d;
			setColourTransformKernelArgs<int>(rct, comp_a, comp_b, comp_c, tile->width, tile->height, level_shift);

			size_t local_work_size[3] = {64,1,1};
			size_t global_work_size[3] = {tile->width * tile->height, 1,1};
			rct->launchKernel(1,global_work_size, local_work_size);

		}
		break;

	case TCR:
//		println_var(INFO, "start: TCR");
			for(i = 0; i < img->num_tiles; i++) {
				tile = &(img->tile[i]);
				int* comp_a = (int*)(&(tile->tile_comp[0]))->img_data_d;
				int* comp_b = (int*)(&(tile->tile_comp[1]))->img_data_d;
				int* comp_c = (int*)(&(tile->tile_comp[2]))->img_data_d;
				setColourTransformInverseKernelArgs<int>(rctInverse, comp_a, comp_b, comp_c, tile->width, tile->height, level_shift, min, max);

				size_t local_work_size[3] = {64,1,1};
				size_t global_work_size[3] = {tile->width * tile->height, 1,1};

				rctInverse->launchKernel(1,global_work_size, local_work_size);
			}
		break;

	case ICT:
//		println_var(INFO, "start: ICT");
		for(i = 0; i < img->num_tiles; i++) {
			tile = &(img->tile[i]);
			int* comp_a = (int*)(&(tile->tile_comp[0]))->img_data_d;
			int* comp_b = (int*)(&(tile->tile_comp[1]))->img_data_d;
			int* comp_c = (int*)(&(tile->tile_comp[2]))->img_data_d;
			setColourTransformKernelArgs<int>(ict, comp_a, comp_b, comp_c, tile->width, tile->height, level_shift);

			size_t local_work_size[3] = {64,1,1};
			size_t global_work_size[3] = {tile->width * tile->height, 1,1};

			ict->launchKernel(1,global_work_size, local_work_size);
		}

		break;

	case TCI:
//		println_var(INFO, "start: TCI");
		for(i = 0; i < img->num_tiles; i++) {
			tile = &(img->tile[i]);
			int* comp_a = (int*)(&(tile->tile_comp[0]))->img_data_d;
			int* comp_b = (int*)(&(tile->tile_comp[1]))->img_data_d;
			int* comp_c = (int*)(&(tile->tile_comp[2]))->img_data_d;
			setColourTransformInverseKernelArgs<int>(ictInverse, comp_a, comp_b, comp_c, tile->width, tile->height, level_shift, min, max);

			size_t local_work_size[3] = {64,1,1};
			size_t global_work_size[3] = {tile->width * tile->height, 1,1};

			ictInverse->launchKernel(1,global_work_size, local_work_size);
		}
		break;

	}
//	println_end(INFO);
	return 0;
}


/**
 * Lossy color transformation YCbCr -> RGB. Decoder of the color_coder_lossy output.
 *
 * @param img Image to be color-transformated
 * @return Returns the color transformated image. It's just the pointer to the same structure passed in img parameter.
 */
int Preprocessor::color_decoder_lossy(type_image *img) {
	return color_trans_gpu(img, TCI);
}


/**
 * Lossless color transformation YUV -> RGB. Decoder of the color_coder_lossless output.
 *
 * @param img Image to be color-transformated
 * @return Returns the color transformated image. It's just the pointer to the same structure passed in img parameter.
 */
int Preprocessor::color_decoder_lossless(type_image *img) {
	return color_trans_gpu(img, TCR);
}

void Preprocessor::dc_level_shifting(type_image *img, int sign)
{
	unsigned int i, j;
	type_tile *tile;
	void *idata;
	int min = 0;
	int max = (1 << img->num_range_bits) - 1;

	for(i = 0; i < img->num_tiles; i++)
	{
		tile = &(img->tile[i]);
		for(j = 0; j < img->num_components; j++)
		{
			idata = (&(tile->tile_comp[j]))->img_data_d;
#ifdef CUDA
			dim3 dimGrid((tile->width + (TILE_SIZEX - 1))/TILE_SIZEX, (tile->height + (TILE_SIZEY - 1))/TILE_SIZEY);
			dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE);
			int level_shift = img->num_range_bits - 1;

			if(sign < 0)
			{
				fdc_level_shift_kernel<<<dimGrid, dimBlock>>>( idata, tile->width, tile->height, level_shift);
			} else
			{
				idc_level_shift_kernel<<<dimGrid, dimBlock>>>( idata, tile->width, tile->height, level_shift, min, max);
			}

			cudaThreadSynchronize();

			checkCUDAError("dc_level_shifting");
#endif
		}
	}
}

/**
 * @brief Inverse DC level shifting.
 * @param img
 * @param type
 */
void Preprocessor::idc_level_shifting(type_image *img)
{
	dc_level_shifting(img, 1);
}

template <class T>  tDeviceRC Preprocessor::setColourTransformKernelArgs(GenericKernel* myKernel,
																     T *img_r, T *img_g, T *img_b, 
																	 const unsigned short width, const unsigned short height,
																	 const int level_shift)
{
	cl_int error_code =  DeviceSuccess;
	cl_kernel targetKernel = myKernel->getKernel();

	 // Dynamically allocate local memory (allocated per workgroup)
	error_code = clSetKernelArg(targetKernel, 0, sizeof(cl_mem), &img_r);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, 1, sizeof(cl_mem), &img_g);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, 2, sizeof(cl_mem), &img_b);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, 3, sizeof(unsigned short), &width);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, 4, sizeof(unsigned short), &height);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, 5, sizeof(int), &level_shift);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	return DeviceSuccess;
}

template <class T>  tDeviceRC Preprocessor::setColourTransformInverseKernelArgs(GenericKernel* myKernel,
																     T *img_r, T *img_g, T *img_b, 
																	 const unsigned short width, const unsigned short height,
																	 const int level_shift,
																	 const int minimum,
																	 const int maximum)
{
	cl_int error_code = setColourTransformKernelArgs(myKernel, img_r, img_g, img_b, width, height, level_shift);
	if (DeviceSuccess != error_code)
	{
		return error_code;
	}
	cl_kernel targetKernel = myKernel->getKernel();
	error_code = clSetKernelArg(targetKernel, 6, sizeof(int), &minimum);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, 7, sizeof(int), &maximum);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	return DeviceSuccess;

}

