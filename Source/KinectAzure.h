#pragma once

#include "TCameraDevice.h"

namespace KinectAzure
{
	class TDevice;
	class TCameraDevice;
	class TDepthReader;

	void	EnumDeviceNameAndFormats(std::function<void(const std::string&, ArrayBridge<std::string>&&) > Enum);
}


class KinectAzure::TCameraDevice : public PopCameraDevice::TDevice
{
public:
	TCameraDevice(const std::string& Serial);
	~TCameraDevice();

	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature, bool Enable) override;

private:
	void			OnFrame(const SoyPixelsImpl& Pixels,SoyTime Time);

private:
	std::shared_ptr<TDepthReader>	mReader;
};
