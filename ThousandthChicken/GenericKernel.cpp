// License: please see LICENSE1 file for more details.
#include "GenericKernel.h"

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

using namespace cv;


GenericKernel::GenericKernel(KernelInitInfo initInfo) : myKernel(0),
									queue(initInfo.cmd_queue),
	                                program(0),
									device(0),
									context(0)
{
	CreateAndBuildKernel(initInfo.programName, initInfo.kernelName, initInfo.buildOptions);
}

GenericKernel::~GenericKernel(void)
{
	if (myKernel)
		clReleaseKernel(myKernel);
	if (program)
		clReleaseProgram(program);
}



// Upload the OpenCL C source code to output argument source
// The memory resource is implictly allocated in the function
// and should be deallocated by the caller
int ReadSourceFromFile(const char* fileName, char** source, size_t* sourceSize)
{
    int errorCode = CL_SUCCESS;

    FILE* fp = NULL;
    fopen_s(&fp, fileName, "rb");
    if (fp == NULL)
    {
        LogError("Error: Couldn't find program source file '%s'.\n", fileName);
        errorCode = CL_INVALID_VALUE;
    }
    else {
        fseek(fp, 0, SEEK_END);
        *sourceSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        *source = new char[*sourceSize];
        if (*source == NULL)
        {
            LogError("Error: Couldn't allocate %d bytes for program source from file '%s'.\n", *sourceSize, fileName);
            errorCode = CL_OUT_OF_HOST_MEMORY;
        }
        else {
            fread(*source, 1, *sourceSize, fp);
        }
    }
    return errorCode;
}


// Create and build the OpenCL program and create the kernel
// The kernel returns in ocl
int GenericKernel::CreateAndBuildKernel(string openCLFileName, string kernelName, string buildOptions)
{
    cl_int error_code;
    size_t src_size = 0;

    // Obtaing the OpenCL context from the command-queue properties
	error_code = clGetCommandQueueInfo(queue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(error_code));
		goto Finish;
	}

    // Obtain the OpenCL device from the command-queue properties
	error_code = clGetCommandQueueInfo(queue, CL_QUEUE_DEVICE, sizeof(cl_device_id), &device, NULL);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clGetCommandQueueInfo (CL_QUEUE_DEVICE) returned %s.\n", TranslateOpenCLError(error_code));
		goto Finish;
	}


	error_code = clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &localMemorySize, 0);
	if (CL_SUCCESS != error_code)
	{
		LogError("Error: clGetDeviceInfo (CL_DEVICE_LOCAL_MEM_SIZE) returned %s.\n", TranslateOpenCLError(error_code));
		goto Finish;
	}

    // Upload the OpenCL C source code from the input file to source
    // The size of the C program is returned in sourceSize
	char* source = NULL;
    error_code = ReadSourceFromFile(openCLFileName.c_str(), &source, &src_size);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: ReadSourceFromFile returned %s.\n", TranslateOpenCLError(error_code));
        goto Finish;
    }

    // Create program object from the OpenCL C code
    program = clCreateProgramWithSource(context, 1, (const char**)&source, &src_size, &error_code);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clCreateProgramWithSource returned %s.\n", TranslateOpenCLError(error_code));
        goto Finish;
    }

    // Build (compile & link) the OpenCL C code
	error_code = clBuildProgram(program, 1, &device,  buildOptions.c_str(), NULL, NULL);
    if (error_code != CL_SUCCESS)
    {
        LogError("Error: clBuildProgram() for source program returned %s.\n", TranslateOpenCLError(error_code));

        // In case of error print the build log to the standard output
        // First check the size of the log
        // Then allocate the memory and obtain the log from the program
        size_t log_size = 0;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

        char* build_log = new char[log_size];
        clGetProgramBuildInfo (program, device, CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);

        printf("Build Fail Log: \n\t%s\n", build_log);

        delete[] build_log;
        goto Finish;
    }

    // Create the required kernel
	myKernel = clCreateKernel(program, kernelName.c_str(), &error_code);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clCreateKernel returned %s.\n", TranslateOpenCLError(error_code));
        goto Finish;
    }
Finish:
    if (source)
    {
        delete[] source;
        source = NULL;
    }
    return error_code;
}

/*
cl_int GenericKernel::launchKernel(size_t global_work_size[2]){

    size_t lwx[3] = {0};

    // Obtains the local work-group size specified by the __attribute__((reqd_work_group_size(X, Y, Z))) qualifier
	cl_int error_code = clGetKernelWorkGroupInfo(myKernel, device, CL_KERNEL_COMPILE_WORK_GROUP_SIZE, sizeof(lwx), lwx, NULL);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clGetKernelWorkGroupInfo (CL_KERNEL_COMPILE_WORK_GROUP_SIZE) returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
    }
	size_t local_work_size[2] = {lwx[0],lwx[1]};
	return launchKernelInternal(global_work_size, local_work_size);

}
*/


cl_int GenericKernel::launchKernel(int dimension, size_t global_work_size[3], size_t local_work_size[3]){
	// call kernel
	
    // Enqueue the command to synchronously execute the kernel on the device
    // The number of dimensions to be used by the global work-items and by work-items in the work-group is 2
    // The global IDs start at offset (0, 0)
    // The command should be executed immediately (without conditions)
    cl_int error_code = clEnqueueNDRangeKernel(queue, myKernel, dimension, NULL, global_work_size, local_work_size, 0, NULL, NULL);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clEnqueueNDRangeKernel returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }

     // Wait until the end of the execution
    error_code = clFinish(queue);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clFinish returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }
	return CL_SUCCESS;
}
