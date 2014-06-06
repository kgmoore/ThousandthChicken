// License: please see LICENSE1 file for more details.
#include "DWTForward97.h"
#include "DWTKernel.cpp"

DWTForward97::DWTForward97(KernelInitInfoBase initInfo) : DWTKernel<float>(7, KernelInitInfo(initInfo, "dwt_f97.cl", "run") )
{
}

DWTForward97::~DWTForward97(void)
{
}

  /// Forward 9/7 2D DWT. See common rules (above) for more details.
  /// @param in      Expected to be normalized into range [-0.5, 0.5].
  ///                Will not be preserved (will be overwritten).
  /// @param out     output buffer on GPU
  /// @param sizeX   width of input image (in pixels)
  /// @param sizeY   height of input image (in pixels)
  /// @param levels  number of recursive DWT levels
  void DWTForward97::dwt(  int sizeX, int sizeY, int levels) {
  // select right width of kernel for the size of the image
    if(sizeX >= 960) {
      launchKernel(192, 8, sizeX, sizeY);
    } else if (sizeX >= 480) {
      launchKernel(128, 6,sizeX, sizeY);
    } else {
      launchKernel(64, 6,  sizeX, sizeY);
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