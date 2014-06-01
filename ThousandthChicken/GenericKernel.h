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
protected:
	int CreateAndBuildKernel(string openCLFileName, string kernelName, string buildOptions);

	cl_int launchKernel(size_t global_work_size[2], size_t local_work_size[2]);

	cl_kernel myKernel;
	cl_command_queue queue;
	cl_program program;
	cl_ulong localMemorySize;
};

