// License: please see LICENSE2 file for more details.
#include "Preprocessor.h"
#include "codestream_image_types.h"
#include "logger.h"

Preprocessor::Preprocessor(KernelInitInfoBase initInfo) :
											initInfo(initInfo),
											ict(new DeviceKernel( KernelInitInfo(initInfo, "preprocess_ict.cl", "ict_kernel") )),
											ictInverse(new DeviceKernel( KernelInitInfo(initInfo, "preprocess_ict_inverse.cl", "tci_kernel") )),
											rct(new DeviceKernel( KernelInitInfo(initInfo, "preprocess_rct.cl", "rct_kernel") )),
											rctInverse(new DeviceKernel( KernelInitInfo(initInfo, "preprocess_rct_inverse.cl", "tcr_kernel") )),
											dcShift(new DeviceKernel( KernelInitInfo(initInfo, "preprocess_dc_level_shift.cl", "fdc_level_shift_kernel") )),
											dcShiftInverse(new DeviceKernel( KernelInitInfo(initInfo, "preprocess_dc_level_shift_inverse.cl", "idc_level_shift_kernel") ))

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

	/////////////////////////////////////////////////////////////////////////////
	// http://www.jpeg.org/.demo/FAQJpeg2k/functionalities.htm#What is the RCT?

	bool isInverse = false;
	DeviceKernel* targetKernel = NULL;
	switch(type) {
	case RCT:
		targetKernel = rct;
		break;
	case TCR:
		isInverse = true;
		targetKernel = ictInverse;
		break;
	case ICT:
		targetKernel = ict;
		break;
	case TCI:
		isInverse = true;
		targetKernel = ictInverse;
		break;
	}
	for(i = 0; i < img->num_tiles; i++) {
			tile = &(img->tile[i]);
			int* comp_a = (int*)(&(tile->tile_comp[0]))->img_data_d;
			int* comp_b = (int*)(&(tile->tile_comp[1]))->img_data_d;
			int* comp_c = (int*)(&(tile->tile_comp[2]))->img_data_d;
			if (isInverse)
				setColourTransformInverseKernelArgs<int>(targetKernel, comp_a, comp_b, comp_c, tile->width, tile->height, level_shift, min, max);
			else
				setColourTransformKernelArgs<int>(targetKernel, comp_a, comp_b, comp_c, tile->width, tile->height, level_shift);

			size_t local_work_size[3] = {64,1,1};
			size_t global_work_size[3] = {tile->width * tile->height, 1,1};
			targetKernel->execute(1,global_work_size, local_work_size);
	}
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
	int *idata;
	int min = 0;
	int max = (1 << img->num_range_bits) - 1;
	int level_shift = img->num_range_bits - 1;

	for(i = 0; i < img->num_tiles; i++)
	{
		tile = &(img->tile[i]);
		size_t local_work_size[3] = {64,1,1};
		size_t global_work_size[3] = {tile->width * tile->height, 1,1};
		for(j = 0; j < img->num_components; j++)
		{
			idata = (int*)(&(tile->tile_comp[j]))->img_data_d;
			if(sign < 0)
			{
				setDCShiftKernelArgs<int>(dcShift,idata, tile->width, tile->height, level_shift);
    			ict->enqueue(1,global_work_size, local_work_size);

			} else
			{
				setDCShiftInverseKernelArgs<int>(dcShiftInverse,idata, tile->width, tile->height, level_shift, min, max);
				ictInverse->enqueue(1,global_work_size, local_work_size);

			}
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

template <class T>  tDeviceRC Preprocessor::setColourTransformKernelArgs(DeviceKernel* myKernel,
																     T *img_r, T *img_g, T *img_b, 
																	 const unsigned short width, const unsigned short height,
																	 const int level_shift)
{
	cl_int error_code =  DeviceSuccess;
	cl_kernel targetKernel = myKernel->getKernel();
	int argNum = 0;

	 // Dynamically allocate local memory (allocated per workgroup)
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(cl_mem), &img_r);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(cl_mem), &img_g);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(cl_mem), &img_b);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(unsigned short), &width);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(unsigned short), &height);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel,argNum++, sizeof(int), &level_shift);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	return DeviceSuccess;
}

template <class T>  tDeviceRC Preprocessor::setColourTransformInverseKernelArgs(DeviceKernel* myKernel,
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
	int argNum = 6;
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(int), &minimum);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(int), &maximum);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setColourTransformKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	return DeviceSuccess;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template <class T>  tDeviceRC Preprocessor::setDCShiftKernelArgs(DeviceKernel* myKernel,
																     T *input, 
																	 const unsigned short width, const unsigned short height,
																	 const int level_shift)
{
	cl_int error_code =  DeviceSuccess;
	cl_kernel targetKernel = myKernel->getKernel();
	int argNum = 0;

	 // Dynamically allocate local memory (allocated per workgroup)
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(cl_mem), &input);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setDCShiftKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(unsigned short), &width);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setDCShiftKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(unsigned short), &height);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setDCShiftKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel,argNum++, sizeof(int), &level_shift);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setDCShiftKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	return DeviceSuccess;
}

template <class T>  tDeviceRC Preprocessor::setDCShiftInverseKernelArgs(DeviceKernel* myKernel,
																     T *input, 
																	 const unsigned short width, const unsigned short height,
																	 const int level_shift,
																	 const int minimum,
																	 const int maximum)
{
	cl_int error_code = setDCShiftKernelArgs(myKernel, input, width, height, level_shift);
	if (DeviceSuccess != error_code)
	{
		return error_code;
	}
	cl_kernel targetKernel = myKernel->getKernel();
	int argNum = 4;
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(int), &minimum);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setDCShiftInverseKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(int), &maximum);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setDCShiftInverseKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	return DeviceSuccess;

}
