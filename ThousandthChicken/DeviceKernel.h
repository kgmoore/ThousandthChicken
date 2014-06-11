// License: please see LICENSE1 file for more details.
#pragma once

#include "platform.h"
#include "ocl_util.h"
#include <string>
#include "DeviceQueue.h"

using namespace std;

class DeviceKernel
{
public:
	DeviceKernel(KernelInitInfo initInfo);
	virtual ~DeviceKernel(void);
	cl_kernel getKernel() { return myKernel;}
	tDeviceRC enqueue(int dimension,  size_t global_work_size[3], size_t local_work_size[3]);
	tDeviceRC execute(int dimension, size_t global_work_size[3],  size_t local_work_size[3]);
	tDeviceRC enqueue(int dimension, size_t global_work_offset[3], size_t global_work_size[3], size_t local_work_size[3]);
	tDeviceRC execute(int dimension, size_t global_work_offset[3], size_t global_work_size[3],  size_t local_work_size[3]);
	tDeviceRC finish() { return deviceQueue->finish();}
protected:
	int CreateAndBuildKernel(string openCLFileName, string kernelName, string buildOptions);
	cl_kernel myKernel;
	cl_command_queue queue;
	cl_program program;
	cl_ulong localMemorySize;
	cl_device_id device;
	cl_context context;
	DeviceQueue* deviceQueue;
};

