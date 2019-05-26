#include "Kinect2.h"
#include "SoyAssert.h"
#include "SoyAutoReleasePtr.h"


namespace Kinect2
{
	//	temp until we can enum devices
	const std::string DeviceName_Default = "Default";

	const std::string DeviceName_Prefix = "Kinect2:";
	const std::string DeviceName_Colour_Suffix = "_Colour";
	const std::string DeviceName_Depth_Suffix = "_Depth";
}


void Kinect2::EnumDeviceNames(std::function<void(const std::string&)> Enum)
{
	//	todo: find real device names & their support
	Enum(DeviceName_Prefix + DeviceName_Default + DeviceName_Colour_Suffix);
	Enum(DeviceName_Prefix + DeviceName_Default + DeviceName_Depth_Suffix);
}

Kinect2::TDevice::TDevice(const std::string& DeviceName)
{
	std::string Serial = DeviceName;
	if ( !Soy::StringTrimLeft(Serial,DeviceName_Prefix,true) )
	{
		std::stringstream Error;
		Error << "Device name (" << DeviceName << ") doesn't begin with " << DeviceName_Prefix;
		throw Soy_AssertException(Error);
	}

	bool Colour = true;

	if ( Soy::StringTrimRight( Serial, DeviceName_Colour_Suffix, true ) )
	{
		//	is colour
		Colour = true;
	}
	else if ( Soy::StringTrimRight( Serial, DeviceName_Depth_Suffix, true ) )
	{
		//	is depth
		Colour = false;
	}
	else
	{
		std::stringstream Error;
		Error << "Device name (" << DeviceName << ") doesn't end with " << DeviceName_Colour_Suffix << " or " << DeviceName_Depth_Suffix;
		throw Soy_AssertException(Error);
	}

	//	now we have a serial
	if ( Serial != DeviceName_Default)
	{
		std::stringstream Error;
		Error << "Todo: find kinect device with serial " << Serial << ". Currently only supporting " << DeviceName_Default;
		throw Soy_AssertException(Error);
	}

	auto Result = GetDefaultKinectSensor(&mSensor);
	Platform::IsOkay(Result, "GetDefaultKinectSensor");

	try
	{
		Result = mSensor->Open();
		Platform::IsOkay(Result, "Open Sensor");

		//	get source
		IDepthFrameSource* pFrameSource = nullptr;
		Soy::AutoReleasePtr<IDepthFrameSource> pFrameSource;
		Result = mSensor->get_DepthFrameSource(&pFrameSource.mObject);
		Platform::IsOkay(Result, "get_DepthFrameSource");

		Result = pFrameSource->OpenReader(&mDepthReader);
		Platform::IsOkay(Result, "OpenReader");

		//SafeRelease(pDepthFrameSource);
	}
	catch(std::exception& e)
	{
		Release();
		throw;
	}
}

Kinect2::TDevice::~TDevice()
{
	Release();
}

void Kinect2::TDevice::Release()
{
	if ( mSensor )
	{
		auto Result = mSensor->Close();
		mSensor->Release();
		mSensor = nullptr;
	}
}

void Kinect2::TDevice::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	throw Soy_AssertException("Feature not supported");
}






