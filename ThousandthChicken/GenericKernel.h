// License: please see LICENSE1 file for more details.
#pragma once

#include "platform.h"
#include "ocl_util.h"
#include <string>

using namespace std;

class GenericKernel
{
public:
	GenericKernel(KernelInitInfo initInfo);
	virtual ~GenericKernel(void);
	cl_kernel getKernel() { return myKernel;}
	cl_int launchKernel(int dimension, size_t global_work_size[3], size_t local_work_size[3]);
protected:
	int CreateAndBuildKernel(string openCLFileName, string kernelName, string buildOptions);
	cl_kernel myKernel;
	cl_command_queue queue;
	cl_program program;
	cl_ulong localMemorySize;
	cl_device_id device;
	cl_context context;
};

