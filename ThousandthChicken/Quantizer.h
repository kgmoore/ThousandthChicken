// License: please see LICENSE2 file for more details.

#pragma once
#include "DeviceKernel.h"


struct type_subband;
struct type_tile;

class Quantizer 
{
public:
	Quantizer(KernelInitInfoBase initInfo);
	virtual ~Quantizer(void);
	void dequantize_tile(type_tile *tile);

private:
	type_subband* dequantization(type_subband *sb, void** coefficients);
	tDeviceRC dequantizationInit(type_subband *sb, void* coefficients);
	int get_exp_subband_gain(int orient);
	KernelInitInfoBase initInfo;
	DeviceKernel* lossyKernel;
	DeviceKernel* losslessKernel;
	cl_mem d_subbandCodeblockCoefficients;

};

