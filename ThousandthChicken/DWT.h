// License: please see LICENSE4 file for more details.

#pragma once

#include "platform.h"
#include "DWTForward53.h"
#include "DWTReverse53.h"
#include "DWTForward97.h"
#include "DWTReverse97.h"

#include "codestream_image_types.h"


class DWT
{
public:
	DWT(KernelInitInfoBase initInfo);
	~DWT(void);
	 void iwt(type_tile *tile);

private:
	tDeviceMem iwt_2d(short filter, type_tile_comp *tile_comp);


private:
	KernelInitInfoBase initInfo;
	DWTForward53* f53;
	DWTReverse53* r53;
	DWTForward97* f97;
	DWTReverse97* r97;
	tDeviceMem  d_idata[256];

};

