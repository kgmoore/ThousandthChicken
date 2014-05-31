#pragma once

#include "DWTKernel.h"

class DWTForward53 : public DWTKernel<int>
{
public:
	DWTForward53(KernelInitInfoBase initInfo);
	virtual ~DWTForward53(void);
private:
	void dwt(int sizeX, int sizeY, int levels) ;
};

