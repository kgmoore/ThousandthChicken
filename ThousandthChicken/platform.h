// License: please see LICENSE1 file for more details.
#pragma once

#include <CL/cl.h>
#include <string>

using namespace std;

typedef cl_mem tDeviceMem;
typedef cl_int tDeviceRC;
#define DeviceSuccess CL_SUCCESS

struct QueueInfo {
	QueueInfo(cl_command_queue queue) :  cmd_queue(queue)
	{}

	cl_command_queue cmd_queue;

};

struct KernelInitInfoBase : QueueInfo {

	KernelInitInfoBase(cl_command_queue queue, string bldOptions) :
		                                 QueueInfo(queue), 
										 buildOptions(bldOptions)
	{}
	KernelInitInfoBase(const KernelInitInfoBase& other) : 
		                                 QueueInfo(other.cmd_queue),
										 buildOptions(other.buildOptions)
	{
	}

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
