#pragma once

#include "DWTKernel.h"
#include "dwt_common.h"


template <typename T> DWTKernel<T>::DWTKernel(int impulseDiameter, 
										  KernelInitInfo initInfo) :
															GenericKernel(initInfo),
															srcMem(0),
															dstMem(0),
															dimX(0),
															dimY(0),
															ownsMemory(false),
															waveletImpulseDiameter(impulseDiameter)
{
}

template <typename T> DWTKernel<T>::~DWTKernel(void)
{
	cl_int error_code = CL_SUCCESS;
	if (ownsMemory) {

		// free memory on device
		if (srcMem) {
			error_code = clReleaseMemObject(srcMem);
			if (CL_SUCCESS != error_code)
			{
				LogError("Error: clReleaseMemObject (input) returned %s.\n", TranslateOpenCLError(error_code));
			}
		}

		if (dstMem) {
			error_code = clReleaseMemObject(dstMem);
			if (CL_SUCCESS != error_code)
			{
				LogError("Error: clReleaseMemObject (output) returned %s.\n", TranslateOpenCLError(error_code));
			}
		}
	}
}


template <typename T> cl_int DWTKernel<T>::run(T* in, int sizeX, int sizeY, int levels){

	if (!in)
		return CL_INVALID_VALUE;

	cl_int error_code;
	cl_context context  = NULL;

    // Obtain the OpenCL context from the command-queue properties
	error_code = clGetCommandQueueInfo(queue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}

	// allocate memory on device
	srcMem = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeX * sizeY * sizeof(T), in, &error_code);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clCreateBuffer (in) returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }

	dstMem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeX * sizeY * sizeof(T), NULL, &error_code);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clCreateBuffer (out) returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }
	ownsMemory = true;
	run(srcMem, dstMem, sizeX, sizeY, levels);
	return CL_SUCCESS;
}


  /// Only computes optimal number of sliding window steps, 
  /// number of threadblocks and then lanches the 5/3 FDWT kernel.
  /// @param WIN_SX  width of sliding window
  /// @param WIN_SY  height of sliding window
  /// @param in       input image
  /// @param out      output buffer
  /// @param sx       width of the input image 
  /// @param sy       height of the input image
template <typename T>  void DWTKernel<T>::launchKernel (int WIN_SX, int WIN_SY, const int sx, const int sy) {

	if (setWindowKernelArgs(WIN_SX, WIN_SY) != CL_SUCCESS)
	  return;

	cl_int error_code = setImageSizeKernelArgs(sx, sy);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: setImageSizeKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return;
	}

	// allocate local data 
    size_t localMemSize =  calcTransformDataBufferSize(WIN_SX,WIN_SY) * sizeof(T);   

	 // Dynamically allocate local memory (allocated per workgroup)
	error_code = clSetKernelArg(myKernel, 2, localMemSize, NULL);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clSetKernelArg returned %s.\n", TranslateOpenCLError(error_code));
		return;
	}

	// compute optimal number of steps of each sliding window
    const int steps = divRndUp(sy, 15 * WIN_SY);
	 error_code = clSetKernelArg(myKernel, 7, sizeof(T), &steps);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clSetKernelArg returned %s.\n", TranslateOpenCLError(error_code));
		return;
	}

    size_t global_work_size[2] = {divRndUp(sx, WIN_SX) * WIN_SX, divRndUp(sy, WIN_SY * steps)};
	size_t local_work_size[2] = {WIN_SX,1};

	GenericKernel::launchKernel(global_work_size, local_work_size);
  }

template <typename T> tDeviceInt DWTKernel<T>::copyLLBandToSrc(int LLSizeX, int LLSizeY){
	  // copy forward or reverse transformed LL band from output back into the input
	size_t bufferOffset[] = { 0, 0, 0};
	cl_int err = CL_SUCCESS;

	// The region size must be given in bytes
	size_t region[] = {LLSizeX * sizeof(T), LLSizeY, 1 };
			
	err = clEnqueueCopyBufferRect ( queue, 	//copy command will be queued
				    dstMem,		
					srcMem,		
					bufferOffset,	//offset associated with src_buffer
					bufferOffset,     //offset associated with src_buffer
					region,		//(width, height, depth) in bytes of the 2D or 3D rectangle being copied
					region[0],   //length of each row in bytes
					0, //length of each 2D slice in bytes 
					region[0] ,   //length of each row in bytes
					0, //length of each 2D slice in bytes
					0,
					NULL,
					NULL);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clEnqueueCopyBufferRect (srcMem) returned %s.\n", TranslateOpenCLError(err));
	}
	return err;

}



template <typename T> cl_int DWTKernel<T>::run(cl_mem in, cl_mem out, int sizeX, int sizeY, int levels){
	srcMem = in;
	dstMem = out;
	dimX = sizeX;
	dimY = sizeY;

	cl_int error_code = clSetKernelArg(myKernel, 3, sizeof(cl_mem), &srcMem);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clSetKernelArg returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(myKernel, 4, sizeof(cl_mem), &dstMem);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clSetKernelArg returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	dwt(sizeX, sizeY, levels);
	return CL_SUCCESS;
}

template <typename T> T* DWTKernel<T>::mapOutputBufferToHost(){
		
	cl_int error_code = CL_SUCCESS;
	void* hostPtr = clEnqueueMapBuffer(queue, dstMem, true, CL_MAP_READ, 0, dimX * dimY * sizeof(T), 0, NULL, NULL, &error_code);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clEnqueueMapBuffer return %s.\n", TranslateOpenCLError(error_code));
    }
	return (T*)hostPtr;
	

}

template <typename T> int DWTKernel<T>::calcTransformDataBufferSize(int winsizex, int winsizey) {
   int wsx = winsizex;
   int wsy = winsizey+ waveletImpulseDiameter;
   int BOUNDARY_X = (waveletImpulseDiameter + 1)/2;
   int VERTICAL_STRIDE =  BOUNDARY_X + (wsx / 2);
   int BUFFER_SIZE = VERTICAL_STRIDE * wsy;
   int PADDING = SHM_BANKS - ((BUFFER_SIZE + SHM_BANKS / 2) % SHM_BANKS);
   return 2 * BUFFER_SIZE + PADDING;
}

template <typename T> cl_int DWTKernel<T>::setImageSizeKernelArgs(int sx, int sy) {

	cl_int error_code = clSetKernelArg(myKernel, 5, sizeof(int), &sx);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clSetKernelArg returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(myKernel, 6, sizeof(int), &sy);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clSetKernelArg returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	return CL_SUCCESS;
}


template <typename T> cl_int DWTKernel<T>::setWindowKernelArgs(int WIN_SX, int WIN_SY) {
	cl_int error_code = clSetKernelArg(myKernel, 0, sizeof(int), &WIN_SX);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clSetKernelArg returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(myKernel, 1, sizeof(int), &WIN_SY);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clSetKernelArg returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	return CL_SUCCESS;
}


