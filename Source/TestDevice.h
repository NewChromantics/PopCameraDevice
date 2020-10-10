#pragma once

#include "TCameraDevice.h"



class TTestDeviceParams : public PopCameraDevice::TCaptureParams
{
public:
	TTestDeviceParams(json11::Json& Options);

	SoyPixelsFormat::Type	mColourFormat = SoyPixelsFormat::Invalid;
	size_t					mFrameRate = 30;
	size_t					mWidth = 200;
	size_t					mHeight = 100;
};


class TestDevice : public PopCameraDevice::TDevice
{
public:
	static inline const char* DeviceName = "Test";
public:
	TestDevice(json11::Json& Options);
	~TestDevice();

	static void		EnumDeviceNames(std::function<void(const std::string&)> Enum);

	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;
	void			GenerateFrame();

	TTestDeviceParams	mParams;
	size_t				mFrameNumber = 0;
	bool				mRunning = true;
	
	std::shared_ptr<SoyThread>	mThread;
};
