#pragma once

#include "TCameraDevice.h"


class TestDevice : public PopCameraDevice::TDevice
{
public:
	static inline const char* DeviceName = "Test";
public:
	TestDevice(const std::string& Format);
	~TestDevice();

	static void											EnumDeviceNames(std::function<void(const std::string&)> Enum);

	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;
	void			GenerateFrame();

	SoyPixelsMeta	mMeta = SoyPixelsMeta(16, 16, SoyPixelsFormat::Greyscale);
	size_t			mFrameRate = 30;
	size_t			mFrameNumber = 0;
	bool			mRunning = true;
	
	std::shared_ptr<SoyThread>	mThread;
};
