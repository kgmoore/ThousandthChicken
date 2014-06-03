#pragma once

#include <CL/cl.h>
#include <string>

using namespace std;

typedef cl_mem tDeviceMem;
typedef cl_int tDeviceRC;
#define DeviceSuccess CL_SUCCESS

struct KernelInitInfoBase {

	KernelInitInfoBase(cl_command_queue queue, string bldOptions) :
		                                 cmd_queue(queue), 
										 buildOptions(bldOptions)
	{}
	KernelInitInfoBase(const KernelInitInfoBase& other) : 
		                                 cmd_queue(other.cmd_queue),
										 buildOptions(other.buildOptions)
	{
	}
	cl_command_queue cmd_queue;
	string buildOptions;
};

struct KernelInitInfo : KernelInitInfoBase {
	KernelInitInfo(cl_command_queue queue,
					string progName,
					string knlName,
					string bldOptions) : KernelInitInfoBase(queue, bldOptions), 
					                     programName(progName),
										 kernelName(knlName)
	{}
	KernelInitInfo(KernelInitInfoBase dwtInfo,
					string progName,
					string knlName) : KernelInitInfoBase(dwtInfo), 
					                     programName(progName),
										 kernelName(knlName)
	{}
	string programName;
	string kernelName;
};
