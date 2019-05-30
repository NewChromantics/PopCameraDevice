#include "Kinect2.h"
#include "SoyAssert.h"


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

Kinect2::TDevice::TDevice(const std::string& DeviceName) :
	SoyThread	( DeviceName )
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
		Soy::AutoReleasePtr<IDepthFrameSource> pFrameSource;
		Result = mSensor->get_DepthFrameSource(&pFrameSource.mObject);
		Platform::IsOkay(Result, "get_DepthFrameSource");

		Result = pFrameSource->OpenReader(&mDepthReader.mObject);
		Platform::IsOkay(Result, "OpenReader");
	}
	catch(std::exception& e)
	{
		Release();
		throw;
	}

	//	create event for when we get new frames
	Result = mDepthReader->SubscribeFrameArrived(&mSubscribeEvent);
	Platform::IsOkay(Result, "SubscribeFrameArrived");
	Start();
}

Kinect2::TDevice::~TDevice()
{
	Stop(true);
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

void Kinect2::TDevice::Thread()
{
	try
	{
		while ( true )
		{
			Iteration();
		}
	}
	catch(std::exception& e)
	{
		OnError(e.what());
		Stop(false);
	}

}


void Kinect2::TDevice::Iteration()
{
	//	block!
	//auto Waitms = INFINITE;
	auto Waitms = 1000;
	auto EventHandle = reinterpret_cast<HANDLE>(mSubscribeEvent);
	auto WaitResult = WaitForSingleObject(EventHandle, Waitms);

	switch ( WaitResult )
	{
	case WAIT_OBJECT_0:
		GetNextFrame();
		break;

	case WAIT_ABANDONED_0:
		//	device has broken?
		throw Soy::AssertException("Event Abandonded");

	case WAIT_TIMEOUT:
		//throw Soy::AssertException("Event Timeout");
		return;

	case WAIT_FAILED:
	default:
		Platform::ThrowLastError("WaitForMultipleObjects");// WaitResult);
		break;
	}
}

void Kinect2::TDevice::GetNextFrame()
{
	//	grab event meta
	//	gr: adding this seems to make the event wait properly (maybe THIS is blocking)
	Soy::AutoReleasePtr<IDepthFrameArrivedEventArgs> FrameMeta;
	auto Result = mDepthReader->GetFrameArrivedEventData(mSubscribeEvent, &FrameMeta.mObject);
	Platform::IsOkay(Result, "GetFrameArrivedEventData");

	Soy::AutoReleasePtr<IDepthFrame> Frame;
	Result = mDepthReader->AcquireLatestFrame(&Frame.mObject);
	if ( Result == E_PENDING )
	{
		std::Debug << "Kinect frame not ready yet" << std::endl;
		return;
	}
	Platform::IsOkay(Result, "AcquireLatestFrame");

	INT64 nTime = 0;
	Soy::AutoReleasePtr<IFrameDescription> FrameDescription;
	int nWidth = 0;
	int nHeight = 0;
	USHORT nDepthMinReliableDistance = 0;
	USHORT nDepthMaxDistance = 0;
	UINT nBufferSize = 0;
	UINT16 *pBuffer = NULL;

	Result = Frame->get_RelativeTime(&nTime);
	Platform::IsOkay(Result, "get_RelativeTime");
	std::Debug << "got frame time " << nTime << std::endl;

	Result = Frame->get_FrameDescription(&FrameDescription.mObject);
	Platform::IsOkay(Result, "get_FrameDescription");

	Result = FrameDescription->get_Width(&nWidth);
	Platform::IsOkay(Result, "get_Width");

	Result = FrameDescription->get_Height(&nHeight);
	Platform::IsOkay(Result, "get_Height");

	Result = Frame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
	Platform::IsOkay(Result, "get_DepthMinReliableDistance");

	// In order to see the full range of depth (including the less reliable far field depth)
	// we are setting nDepthMaxDistance to the extreme potential depth threshold
	nDepthMaxDistance = USHRT_MAX;

	// Note:  If you wish to filter by reliable depth distance, uncomment the following line.
	//// hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
	
	Result = Frame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);
	Platform::IsOkay(Result, "AccessUnderlyingBuffer");

	//ProcessDepth(nTime, pBuffer, nWidth, nHeight, nDepthMinReliableDistance, nDepthMaxDistance);
}

void Kinect2::TDevice::OnError(const std::string& Error)
{
	std::Debug << "Kinect error:" << Error << std::endl;
	mError = Error;
	Stop(false);
}



