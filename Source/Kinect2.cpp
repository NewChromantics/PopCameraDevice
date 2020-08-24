#include "Kinect2.h"
#include "SoyAssert.h"
#include "SoyMedia.h"
#include "SoyJson.h"

namespace Kinect2
{
	//	temp until we can enum devices
	const std::string DeviceName_Default = "Default";

	const std::string DeviceName_Prefix = "Kinect2:";
	const std::string DeviceName_Colour_Suffix = "_Colour";
	const std::string DeviceName_Depth_Suffix = "_Depth";

	SoyPixelsFormat::Type GetPixelFormat(int BytesPerPixel);
}


SoyPixelsFormat::Type Kinect2::GetPixelFormat(int BytesPerPixel)
{
	if (BytesPerPixel == 1)
		return SoyPixelsFormat::Greyscale;

	if (BytesPerPixel == 2)
		return SoyPixelsFormat::Depth16mm;

	std::stringstream Error;
	Error << "Unhandled kinect pixel format for BytesPerPixel=" << BytesPerPixel;
	throw Soy_AssertException(Error);
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
		throw PopCameraDevice::TInvalidNameException();

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


bool Kinect2::TDevice::ThreadIteration()
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
		return true;

	case WAIT_ABANDONED_0:
		//	device has broken?
		throw Soy::AssertException("Event Abandonded");

	case WAIT_TIMEOUT:
		//throw Soy::AssertException("Event Timeout");
		return true;

	case WAIT_FAILED:
	default:
		Platform::ThrowLastError("WaitForMultipleObjects");// WaitResult);
		break;
	}

	return true;
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

	//	get time asap
	SoyTime FrameTime(true);

	json11::Json::object Json;

	Soy::AutoReleasePtr<IFrameDescription> FrameDescription;
	TIMESPAN Time = 0;
	int Width = 0;
	int Height = 0;
	uint16_t DepthMinReliableDistance = 0;
	uint16_t DepthMaxDistance = USHRT_MAX;
	uint32_t BufferLength = 0;
	uint16_t* Buffer = NULL;
	float HorzFov = 0;
	float VertFov = 0;
	float DiagFov = 0;
	uint32_t BytesPerPixel = 0;

	//	eg: 60893123221 on 8th june 2019
	Result = Frame->get_RelativeTime(&Time);
	Platform::IsOkay(Result, "get_RelativeTime");
	Json["RelativeTime"] = static_cast<int>(Time);

	Result = Frame->get_FrameDescription(&FrameDescription.mObject);
	Platform::IsOkay(Result, "get_FrameDescription");

	Result = FrameDescription->get_Width(&Width);
	Platform::IsOkay(Result, "get_Width");

	Result = FrameDescription->get_Height(&Height);
	Platform::IsOkay(Result, "get_Height");
	
	Result = FrameDescription->get_HorizontalFieldOfView(&HorzFov);
	Platform::IsOkay(Result, "get_HorizontalFieldOfView");
	Json["HorizontalFov"] = HorzFov;

	Result = FrameDescription->get_VerticalFieldOfView(&VertFov);
	Platform::IsOkay(Result, "get_VerticalFieldOfView");
	Json["VerticalFov"] = VertFov;

	Result = FrameDescription->get_DiagonalFieldOfView(&DiagFov);
	Platform::IsOkay(Result, "get_DiagonalFieldOfView");
	Json["DiagonalFov"] = DiagFov;

	Result = FrameDescription->get_BytesPerPixel(&BytesPerPixel);
	Platform::IsOkay(Result, "get_BytesPerPixel");

	Result = Frame->get_DepthMinReliableDistance(&DepthMinReliableDistance);
	Platform::IsOkay(Result, "get_DepthMinReliableDistance");
	Json["MinReliableDistance"] = DepthMinReliableDistance;


	// Note:  If you wish to filter by reliable depth distance, uncomment the following line.
	Result = Frame->get_DepthMaxReliableDistance(&DepthMaxDistance);
	Platform::IsOkay(Result, "get_DepthMaxReliableDistance");
	Json["MaxReliableDistance"] = DepthMaxDistance;

	//	gr: scope should be with frame, but try and verify
	Result = Frame->AccessUnderlyingBuffer(&BufferLength, &Buffer);
	Platform::IsOkay(Result, "AccessUnderlyingBuffer");
	auto BufferSize = BufferLength * BytesPerPixel;
	
	//	make pixels
	auto* Buffer8 = reinterpret_cast<uint8_t*>(Buffer);
	auto PixelFormat = GetPixelFormat(BytesPerPixel);
	SoyPixelsRemote Pixels(Buffer8, Width, Height, BufferSize, PixelFormat);

	float3x3 Transform;
	std::shared_ptr<TPixelBuffer> PixelBuffer(new TDumbPixelBuffer(Pixels,Transform));
	PushFrame( PixelBuffer, Pixels.GetMeta(), FrameTime, Json );
}




