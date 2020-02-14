#pragma once

#include "TCameraDevice.h"

namespace KinectAzure
{
	class TDevice;
	class TCameraDevice;
	class TDepthReader;

	void			EnumDeviceNames(std::function<void(const std::string&)> Enum);
}


class KinectAzure::TCameraDevice : public PopCameraDevice::TDevice
{
public:
	TCameraDevice(const std::string& Serial);

	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature, bool Enable) override;

private:
	std::shared_ptr<TDepthReader>	mReader;
};
