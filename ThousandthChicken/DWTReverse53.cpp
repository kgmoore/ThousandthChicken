// License: please see LICENSE1 file for more details.
#include "DWTReverse53.h"
#include "DWTKernel.cpp"

DWTReverse53::DWTReverse53(KernelInitInfoBase initInfo) : DWTKernel<int>(3,	KernelInitInfo(initInfo, "dwt_r53.cl", "run") )
{

}


DWTReverse53::~DWTReverse53(void)
{
}
    
  /// Reverse 5/3 2D DWT. See common rules (above) for more details.
  /// @param in      Input DWT coefficients. Format described in common rules.
  ///                Will not be preserved (will be overwritten).
  /// @param out     output buffer on GPU - will contain original image
  ///                in normalized range [-128, 127].
  /// @param sizeX   width of input image (in pixels)
  /// @param sizeY   height of input image (in pixels)
  /// @param levels  number of recursive DWT levels
  void DWTReverse53::dwt(  int sizeX, int sizeY, int levels) {
    if(levels > 1) {
      // let this function recursively reverse transform deeper levels first
      const int llSizeX = divRndUp(sizeX, 2);
      const int llSizeY = divRndUp(sizeY, 2);
      dwt( llSizeX, llSizeY, levels - 1);

	  tDeviceRC err = copyLLBandToSrc(llSizeX, llSizeY);
	  if (err != DeviceSuccess)
		  return;
    }
    
    // select right width of kernel for the size of the image
    if(sizeX >= 960) {
      launchKernel(192, 8, sizeX, sizeY);
    } else if (sizeX >= 480) {
      launchKernel(128, 8,sizeX, sizeY);
    } else {
      launchKernel(64, 8, sizeX, sizeY);
    }
  }

