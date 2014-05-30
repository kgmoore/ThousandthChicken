#pragma once

#include <CL/cl.h>
#include <string>

using namespace std;

typedef cl_mem tDeviceMem;
typedef cl_int tDeviceInt;
#define DeviceSuccess CL_SUCCESS

struct DwtKernelInitInfo {

	DwtKernelInitInfo(cl_command_queue queue, string bldOptions) :
		                                 cmd_queue(queue), 
										 buildOptions(bldOptions)
	{}
	DwtKernelInitInfo(const DwtKernelInitInfo& other) : 
		                                 cmd_queue(other.cmd_queue),
										 buildOptions(other.buildOptions)
	{
	}
	cl_command_queue cmd_queue;
	string buildOptions;
};

struct KernelInitInfo : DwtKernelInitInfo {
	KernelInitInfo(cl_command_queue queue,
					string progName,
					string knlName,
					string bldOptions) : DwtKernelInitInfo(queue, bldOptions), 
					                     programName(progName),
										 kernelName(knlName)
	{}
	KernelInitInfo(DwtKernelInitInfo dwtInfo,
					string progName,
					string knlName) : DwtKernelInitInfo(dwtInfo), 
					                     programName(progName),
										 kernelName(knlName)
	{}
	string programName;
	string kernelName;
};
