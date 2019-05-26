#pragma once

#include "TCameraDevice.h"

#include <Kinect.h>
#pragma comment(lib, "kinect20.lib")


namespace Kinect2
{
	//	gr: we'll probably need to split this into device & source for the different streams so we can attach to depth/colour at will
	class TDevice;

	void			EnumDeviceNames(std::function<void(const std::string&)> Enum);
}


class Kinect2::TDevice : public PopCameraDevice::TDevice
{
public:
	TDevice(const std::string& Serial);
	
	virtual void	EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable) override;

private:
	void			Release();

public:
	IKinectSensor*			mSensor = nullptr;
	IDepthFrameReader*		mDepthReader = nullptr;
	IColorFrameReader*		mColourReader = nullptr;
};




