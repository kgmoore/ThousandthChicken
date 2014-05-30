#pragma once

#include "DWTKernel.h"

class DWTForward97 : public DWTKernel<float>
{
public:
	DWTForward97(DwtKernelInitInfo initInfo);
	virtual ~DWTForward97(void);
private:
	void dwt(int sizeX, int sizeY, int levels) ;
};

