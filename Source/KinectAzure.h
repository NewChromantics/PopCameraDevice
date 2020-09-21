#pragma once

#include "TCameraDevice.h"

namespace KinectAzure
{
	class TDevice;
	class TCameraDevice;
	class TPixelReader;
	class TCaptureParams;

	void	EnumDeviceNameAndFormats(std::function<void(const std::string&, ArrayBridge<std::string>&&) > Enum);
}


class KinectAzure::TCaptureParams
{
public:
	TCaptureParams(json11::Json& Options);
	
	size_t					mFrameRate = 30;	//	0 = dont set
	SoyPixelsFormat::Type	mColourFormat = SoyPixelsFormat::Invalid;
	SoyPixelsFormat::Type	mDepthFormat = SoyPixelsFormat::Depth16mm;
	bool					mVerboseDebug = false;
	bool					mSyncMaster = false;
	bool					mSyncSub = false;
};


class KinectAzure::TCameraDevice : public PopCameraDevice::TDevice
{
public:
	TCameraDevice(const std::string& Serial,json11::Json& Options);
	~TCameraDevice();

	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature, bool Enable) override;


private:
	std::shared_ptr<TPixelReader>	mReader;
};
