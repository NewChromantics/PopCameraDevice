#pragma once

#include "TCameraDevice.h"


namespace Freenect2
{
	class TDevice;
	class TContext;

	TContext&		GetContext();
	
	void			EnumDeviceNames(std::function<void(const std::string&)> Enum);
}


class Freenect2::TDevice : public PopCameraDevice::TDevice
{
public:
	TDevice(const std::string& Serial);
	
	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;
};




