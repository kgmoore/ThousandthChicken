#pragma once
#include "GenericKernel.h"
#include "platform.h"

template <typename T> class DWTKernel : public GenericKernel
{
public:
	DWTKernel(int waveletImpulseDiameter, KernelInitInfo initInfo);
	virtual ~DWTKernel(void);
	tDeviceInt run(tDeviceMem in, tDeviceMem out, int sizeX, int sizeY, int levels);
	tDeviceInt run(T* in, int sizeX, int sizeY, int levels);
	T* mapOutputBufferToHost();
protected:
	virtual void dwt( int sizeX, int sizeY, int levels) =0;
    void launchKernel (int WIN_SX, int WIN_SY, const int sx, const int sy);
	int calcTransformDataBufferSize(int winsizex, int winsizey);
	tDeviceInt setWindowKernelArgs(int WIN_SX, int WIN_SY);
	tDeviceInt setImageSizeKernelArgs(int sx, int sy);
	tDeviceInt copyLLBandToSrc(int LLSizeX, int LLSizeY);

	tDeviceMem srcMem;
	tDeviceMem dstMem;
	int dimX;
	int dimY;
	bool ownsMemory;
	int waveletImpulseDiameter;
};

