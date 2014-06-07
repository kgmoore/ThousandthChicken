// License: please see LICENSE1 file for more details.
#pragma once
#include "DeviceKernel.h"
#include "platform.h"

template <typename T> class DWTKernel : public DeviceKernel
{
public:
    DWTKernel(int waveletImpulseDiameter, KernelInitInfo initInfo);
    virtual ~DWTKernel(void);
    tDeviceRC run(tDeviceMem in, tDeviceMem out, int sizeX, int sizeY, int levels);
    tDeviceRC run(T* in, int sizeX, int sizeY, int levels);
    T* mapOutputBufferToHost();
protected:
    virtual void dwt( int sizeX, int sizeY, int levels) =0;
    void enqueue (int WIN_SX, int WIN_SY, const int sx, const int sy);
    int calcTransformDataBufferSize(int winsizex, int winsizey);
    tDeviceRC setWindowKernelArgs(int WIN_SX, int WIN_SY);
    tDeviceRC setImageSizeKernelArgs(int sx, int sy);
    tDeviceRC copyLLBandToSrc(int LLSizeX, int LLSizeY);

    tDeviceMem srcMem;
    tDeviceMem dstMem;
    int dimX;
    int dimY;
    bool ownsMemory;
    int waveletImpulseDiameter;
};

