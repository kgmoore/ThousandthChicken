#pragma once

#include "DWTKernel.h"

class DWTReverse97 : public DWTKernel<float>
{
public:
	DWTReverse97(DwtKernelInitInfo initInfo);
	virtual ~DWTReverse97(void);
private:
	void dwt( int sizeX, int sizeY, int levels) ;
};

