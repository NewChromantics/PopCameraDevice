#include "Avf.h"
#include "SoyLib/src/SoyString.h"
#include "SoyAvf.h"
#include "TCameraDevice.h"


namespace Avf
{
	TCaptureFormatMeta	GetMeta(AVCaptureDeviceFormat* Format);
	vec2f				GetMinMaxFrameRate(AVCaptureDeviceFormat* Format);
}


vec2f Avf::GetMinMaxFrameRate(AVCaptureDeviceFormat* Format)
{
	if ( !Format )
		throw Soy::AssertException("GetMinMaxFrameRate(): Format null");
	
	//	these are in pairs, but seems okay to just get min & max of both
	Array<float> MinFrameRates;
	Array<float> MaxFrameRates;
	auto* FrameRateRanges = Format.videoSupportedFrameRateRanges;
	auto EnumRange = [&](AVFrameRateRange* Range)
	{
		MinFrameRates.PushBack(Range.maxFrameRate);
		MaxFrameRates.PushBack(Range.minFrameRate);
	};
	Platform::NSArray_ForEach<AVFrameRateRange*>(FrameRateRanges,EnumRange);
	
	if ( MinFrameRates.IsEmpty() )
		throw Soy::AssertException("Got no frame rates from camera format");
	
	//std::Debug << "Camera fps min: " << Soy::StringJoin(GetArrayBridge(MinFrameRates),",") << " max: " << Soy::StringJoin(GetArrayBridge(MaxFrameRates),",") << std::endl;
	
	float Min = MinFrameRates[0];
	float Max = MaxFrameRates[0];
	for ( auto i=0;	i<MinFrameRates.GetSize();	i++ )
	{
		Min = std::min( Min, MinFrameRates[i] );
		Max = std::max( Max, MaxFrameRates[i] );
	}
	
	return vec2f( Min, Max );
}


namespace Avf
{
	std::ostream& operator<<(std::ostream& out,const Avf::TCaptureFormatMeta& in)
	{
		out << "MaxSize=" << in.mPixelMeta << ", ";
		out << "Fps=" << in.mMinFps << "..." << in.mMaxFps << ", ";
	
		return out;
	}
}

Avf::TCaptureFormatMeta Avf::GetMeta(AVCaptureDeviceFormat* Format)
{
	if ( !Format )
		throw Soy::AssertException("GetMeta AVCaptureDeviceFormat null format");
	
	//	lots of work already in here
	auto StreamMeta = Avf::GetStreamMeta( Format.formatDescription );

	TCaptureFormatMeta Meta;
	Meta.mPixelMeta = StreamMeta.mPixelMeta;

	try
	{
		auto FrameRateRange = GetMinMaxFrameRate( Format );
		Meta.mMinFps = FrameRateRange.x;
		Meta.mMaxFps = FrameRateRange.y;
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
	
	return Meta;
}





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
	
	//	get formats
	auto EnumFormat = [&](AVCaptureDeviceFormat* Format)
	{
		auto FormatMeta = Avf::GetMeta(Format);
		Meta.mFormats.PushBack( FormatMeta );
		//std::Debug << "Camera format: " << FormatMeta << std::endl;
	};
	Platform::NSArray_ForEach<AVCaptureDeviceFormat*>( [Device formats], EnumFormat );

	
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

std::string GetFormatString(const Avf::TCaptureFormatMeta& Meta)
{
	return PopCameraDevice::GetFormatString(Meta.mPixelMeta, Meta.mMaxFps);
}

void Avf::EnumCaptureDevices(std::function<void(const std::string&,ArrayBridge<std::string>&&)> EnumName)
{
	auto EnumMeta = [&](const TDeviceMeta& Meta)
	{
		Array<std::string> FormatStrings;
		for ( auto i=0;	i<Meta.mFormats.GetSize();	i++)
		{
			auto& FormatMeta = Meta.mFormats[i];
			auto FormatString = GetFormatString(FormatMeta);
			FormatStrings.PushBack(FormatString);
		}
		EnumName( Meta.mCookie, GetArrayBridge(FormatStrings) );
	};
	EnumCaptureDevices( EnumMeta );
}

