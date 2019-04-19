#include "Avf.h"
#include "SoyLib/src/SoyString.h"



Avf::TDeviceMeta GetMeta(AVCaptureDevice* Device)
{
	Avf::TDeviceMeta Meta;
	
	Meta.mName = Soy::NSStringToString( [Device localizedName] );
	Meta.mSerial = Soy::NSStringToString( [Device uniqueID] );
#if defined(TARGET_IOS)
	Meta.mVendor = "ios";
#else
	Meta.mVendor = Soy::NSStringToString( [Device manufacturer] );
#endif
	Meta.mIsConnected = YES == [Device isConnected];
	Meta.mHasVideo = YES == [Device hasMediaType:AVMediaTypeVideo];
	Meta.mHasAudio = YES == [Device hasMediaType:AVMediaTypeAudio];
	
	/*
	TVideoDeviceMeta Meta;
	Meta.mName = std::string([[Device localizedName] UTF8String]);
	Meta.mSerial = std::string([[Device uniqueID] UTF8String]);
	Meta.mVendor = std::string([[Device manufacturer] UTF8String]);
	Meta.mConnected = YES == [Device isConnected];
	Meta.mVideo = YES == [Device hasMediaType:AVMediaTypeVideo];
	Meta.mAudio = YES == [Device hasMediaType:AVMediaTypeAudio];
	Meta.mText = YES == [Device hasMediaType:AVMediaTypeText];
	Meta.mClosedCaption = YES == [Device hasMediaType:AVMediaTypeClosedCaption];
	Meta.mSubtitle = YES == [Device hasMediaType:AVMediaTypeSubtitle];
	Meta.mTimecode = YES == [Device hasMediaType:AVMediaTypeTimecode];
	//		Meta.mTimedMetadata = YES == [Device hasMediaType:AVMediaTypeTimedMetadata];
	Meta.mMetadata = YES == [Device hasMediaType:AVMediaTypeMetadata];
	Meta.mMuxed = YES == [Device hasMediaType:AVMediaTypeMuxed];
*/
	
	//	asleep, eg. macbook camera when lid is down
#if defined(TARGET_IOS)
	Meta.mIsSuspended = false;
#else
	Meta.mIsSuspended = false;
	try
	{
		Meta.mIsSuspended = YES == [Device isSuspended];
	}
	catch(...)
	{
		//	unsupported method on this device. case grahams: DK2 Left&Right webcam throws exception here
	}
#endif
	return Meta;
}

void Avf::EnumCaptureDevices(std::function<void(const Avf::TDeviceMeta&)> Enum)
{
	auto Devices = [AVCaptureDevice devices];
	for ( AVCaptureDevice* Device in Devices )
	{
		if ( !Device )
			continue;
		
		auto Meta = GetMeta( Device );
		if ( !Meta.mHasVideo )
			continue;
		
		Enum( Meta );
	}
}

void Avf::EnumCaptureDevices(std::function<void(const std::string&)> EnumName)
{
	auto EnumMeta = [&](const TDeviceMeta& Meta)
	{
		EnumName( Meta.mCookie );
	};
	EnumCaptureDevices( EnumMeta );
}
