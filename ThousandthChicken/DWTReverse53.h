#pragma once

#include "DWTKernel.h"

class DWTReverse53 : public DWTKernel<int>
{
public:
	DWTReverse53(DwtKernelInitInfo initInfo);
	virtual ~DWTReverse53(void);
private:
	void dwt( int sizeX, int sizeY, int levels) ;
};

