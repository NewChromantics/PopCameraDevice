#pragma once

#include "TOutputDevice.h"
#include "SoyMedia.h"



namespace CoreMedia
{
	class TOutputDevice;
}


class CoreMedia::TOutputDevice : public PopOutputDevice::TDevice
{
public:
	TOutputDevice(const std::string& DeviceName);
};
