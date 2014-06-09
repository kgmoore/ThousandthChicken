#pragma once


#include "platform.h"

class DeviceQueue
{
public:
	DeviceQueue(QueueInfo info);
	~DeviceQueue(void);
	tDeviceRC finish();
	tDeviceRC flush();
private:
	cl_command_queue queue;
};

