// License: please see LICENSE1 file for more details.
#pragma once

#include "DWTKernel.h"

class DWTForward97 : public DWTKernel<float>
{
public:
	DWTForward97(KernelInitInfoBase initInfo);
	virtual ~DWTForward97(void);
private:
	void dwt(int sizeX, int sizeY, int levels) ;
};

