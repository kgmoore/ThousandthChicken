// License: please see LICENSE2 file for more details.
#pragma once

#include "DeviceKernel.h"

typedef struct type_image type_image;


typedef enum {
	RCT, ///Reversible Color Transformation. Encoder part of the lossless flow.
	TCR, ///Decoder part of the lossless flow.
	ICT, ///Irreversible Color Transformation. Encoder part of the lossy flow.
	TCI  ///Decoder part of the lossy flow.
} color_trans_type;


class Preprocessor
{
public:
	Preprocessor(KernelInitInfoBase initInfo);
	~Preprocessor(void);
	void idc_level_shifting(type_image *img);
	int color_decoder_lossy(type_image *img);
	int color_decoder_lossless(type_image *img);

private:
	void dc_level_shifting(type_image *img, int sign);
	int color_trans_gpu(type_image *img, color_trans_type type) ;
	template <class T>  tDeviceRC setColourTransformKernelArgs(DeviceKernel* myKernel,
		                                                       T *img_r, T *img_g, T *img_b,
															   const unsigned short width, const unsigned short height, 
															   const int level_shift);
	template <class T>  tDeviceRC setColourTransformInverseKernelArgs(DeviceKernel* myKernel,
		                                                       T *img_r, T *img_g, T *img_b,
															   const unsigned short width, const unsigned short height, 
															   const int level_shift,
															   const int minimum,
															   const int maximum);

	template <class T>  tDeviceRC setDCShiftKernelArgs(DeviceKernel* myKernel,
		                                                       T *input,
															   const unsigned short width, const unsigned short height, 
															   const int level_shift);
	template <class T>  tDeviceRC setDCShiftInverseKernelArgs(DeviceKernel* myKernel,
		                                                       T *input,
															   const unsigned short width, const unsigned short height, 
															   const int level_shift,
															   const int minimum,
															   const int maximum);

	KernelInitInfoBase initInfo;

	DeviceKernel* ict;
	DeviceKernel* ictInverse;

	DeviceKernel* rct;
	DeviceKernel* rctInverse;

	DeviceKernel* dcShift;
	DeviceKernel* dcShiftInverse;



};

