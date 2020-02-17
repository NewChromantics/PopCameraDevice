#pragma once

#include "TCameraDevice.h"
#include "SoyAutoReleasePtr.h"
#include <SoyThread.h>

#include <Kinect.h>
#pragma comment(lib, "kinect20.lib")


namespace Kinect2
{
	//	gr: we'll probably need to split this into device & source for the different streams so we can attach to depth/colour at will
	class TDevice;

	void			EnumDeviceNames(std::function<void(const std::string&)> Enum);
}


class Kinect2::TDevice : public PopCameraDevice::TDevice, SoyThread
{
public:
	TDevice(const std::string& Serial);
	~TDevice();
	
	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;

private:
	void			Release();
	virtual bool	ThreadIteration() override;
	void			GetNextFrame();
	
public:
	IKinectSensor*			mSensor = nullptr;
	Soy::AutoReleasePtr<IDepthFrameReader>	mDepthReader;
	Soy::AutoReleasePtr<IColorFrameReader>	mColourReader;
	WAITABLE_HANDLE			mSubscribeEvent = 0;
};




