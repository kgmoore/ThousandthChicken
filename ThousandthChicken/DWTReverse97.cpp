#include "DWTReverse97.h"
#include "DWTKernel.cpp"

DWTReverse97::DWTReverse97(KernelInitInfoBase initInfo) : DWTKernel<float>(7, KernelInitInfo(initInfo, "dwt_r97.cl", "run") )
{

}


DWTReverse97::~DWTReverse97(void)
{
}
    
  /// Reverse 9/7 2D DWT. See common rules (above) for more details.
  /// @param in      Input DWT coefficients. Format described in common rules.
  ///                Will not be preserved (will be overwritten).
  /// @param out     output buffer on GPU - will contain original image
  ///                in normalized range [-0.5, 0.5].
  /// @param sizeX   width of input image (in pixels)
  /// @param sizeY   height of input image (in pixels)
  /// @param levels  number of recursive DWT levels
  void DWTReverse97::dwt(  int sizeX, int sizeY, int levels) {
    if(levels > 1) {
      // let this function recursively reverse transform deeper levels first
      const int llSizeX = divRndUp(sizeX, 2);
      const int llSizeY = divRndUp(sizeY, 2);
      dwt( llSizeX, llSizeY, levels - 1);

	  tDeviceInt err = copyLLBandToSrc(llSizeX, llSizeY);
	  if (err != DeviceSuccess)
		  return;
    }
    
    // select right width of kernel for the size of the image
    if(sizeX >= 960) {
      launchKernel(192, 8, sizeX, sizeY);
    } else if (sizeX >= 480) {
      launchKernel(128, 6,sizeX, sizeY);
    } else {
      launchKernel(64, 6, sizeX, sizeY);
    }
  }

