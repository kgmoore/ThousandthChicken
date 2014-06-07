#pragma once


#include "platform.h"

class DeviceQueue
{
public:
	DeviceQueue(QueueInfo info);
	~DeviceQueue(void);
	tDeviceRC finish();
private:
	cl_command_queue queue;
};

