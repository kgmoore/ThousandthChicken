// License: please see LICENSE1 file for more details.
#pragma once


#include <string>
#include "CL/cl.h"

extern bool quiet;

struct data_args_d_t
{
    char* vendorName;                   // preferred OpenCL platform vendor name
    bool  preferCpu;                    // indicator to create context with CPU device
    bool  preferGpu;                    // indicator to create context with GPU device
};

struct ocl_args_d_t
{
    ocl_args_d_t();
    ~ocl_args_d_t();

    cl_context       context;           // hold the context handler
    cl_device_id     device;            // hold the selected device handler
    cl_command_queue commandQueue;      // hold the commands-queue handler
};

// Print usefull information to the default output. Same usage as with printf
void LogInfo(const char* str, ...);

// Print error notification to the default output. Same usage as with printf
void LogError(const char* str, ...);

// Find an OpenCL platform from the preferredVendor with the preferred device(s)
// One and only one of the flags is allowed to be set to true
cl_platform_id FindPlatformId(const char* preferredVendor = NULL, bool preferCpu = false, bool preferGpu = false, bool preferShared = false);

// Create an OpenCL context using the preferred devices available on the OpenCL platform platformId
cl_context CreateContext(cl_platform_id platformId, bool preferCpu = false, bool preferGpu = false, bool preferShared = false);

// Create an OpenCL context with the CPU device available on the OpenCL platform platformId
cl_context CreateCPUContext(cl_platform_id platformId);

// Create an OpenCL context with the GPU device available on the OpenCL platform platformId
cl_context CreateGPUContext(cl_platform_id platformId);

// Create an OpenCL shared context with both CPU and GPU devices available on the OpenCL platform platformId
cl_context CreateSharedContext(cl_platform_id platformId);

// Translate OpenCL numberic error code (errorCode) to a meaningful error string
const char* TranslateOpenCLError(cl_int errorCode);

// Initialize OpenCL environment - Find required platform and create a context and commands-queue
int InitOpenCL(ocl_args_d_t* ocl, data_args_d_t* data);

// Returns host time in (ms)
unsigned long long HostTime();