#include "DeviceQueue.h"
#include "ocl_util.h"

DeviceQueue::DeviceQueue(QueueInfo info) : queue(info.cmd_queue)
{
}


DeviceQueue::~DeviceQueue(void)
{
}


tDeviceRC DeviceQueue::finish(void)
{
    // Wait until the end of the execution
    cl_int error_code = clFinish(queue);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clFinish returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }
    return CL_SUCCESS;
}

tDeviceRC DeviceQueue::flush(void)
{
    // Wait until the end of the execution
    cl_int error_code = clFlush(queue);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clFinish returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }
    return CL_SUCCESS;
}