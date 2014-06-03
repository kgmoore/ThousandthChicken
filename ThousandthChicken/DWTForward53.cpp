#include "DWTForward53.h"
#include "DWTKernel.cpp"

DWTForward53::DWTForward53(KernelInitInfoBase initInfo) : DWTKernel<int>(3,KernelInitInfo(initInfo, "dwt_f53.cl", "run") )
{
}

DWTForward53::~DWTForward53(void)
{
}

  /// Forward 5/3 2D DWT. See common rules (above) for more details.
  /// @param in      Expected to be normalized into range [-128, 127].
  ///                Will not be preserved (will be overwritten).
  /// @param out     output buffer on GPU
  /// @param sizeX   width of input image (in pixels)
  /// @param sizeY   height of input image (in pixels)
  /// @param levels  number of recursive DWT levels
  void DWTForward53::dwt(  int sizeX, int sizeY, int levels) {
  // select right width of kernel for the size of the image
    if(sizeX >= 960) {
      launchKernel(192, 8, sizeX, sizeY);
    } else if (sizeX >= 480) {
      launchKernel(128, 8,sizeX, sizeY);
    } else {
      launchKernel(64, 8,  sizeX, sizeY);
    }
    
    // if this was not the last level, continue recursively with other levels
    if(levels > 1) {
      // copy output's LL band back into input buffer
      const int llSizeX = divRndUp(sizeX, 2);
      const int llSizeY = divRndUp(sizeY, 2);
	  
	  tDeviceRC err = copyLLBandToSrc(llSizeX, llSizeY);
	  if (err != DeviceSuccess)
		  return;  
      
      // run remaining levels of FDWT
      dwt(llSizeX, llSizeY, levels - 1);
    }
  }