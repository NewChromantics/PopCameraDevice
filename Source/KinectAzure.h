#pragma once

#include "TCameraDevice.h"

namespace KinectAzure
{
	class TDevice;
	class TCameraDevice;
	class TPixelReader;

	void	EnumDeviceNameAndFormats(std::function<void(const std::string&, ArrayBridge<std::string>&&) > Enum);
}


class KinectAzure::TCameraDevice : public PopCameraDevice::TDevice
{
public:
	TCameraDevice(const std::string& Serial,json11::Json::options& Options);
	~TCameraDevice();

	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature, bool Enable) override;


private:
	std::shared_ptr<TPixelReader>	mReader;
};
