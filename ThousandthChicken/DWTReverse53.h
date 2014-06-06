// License: please see LICENSE1 file for more details.
#pragma once

#include "DWTKernel.h"

class DWTReverse53 : public DWTKernel<int>
{
public:
	DWTReverse53(KernelInitInfoBase initInfo);
	virtual ~DWTReverse53(void);
private:
	void dwt( int sizeX, int sizeY, int levels) ;
};

