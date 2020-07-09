#include "Avf.h"
#include "SoyString.h"
#include "SoyAvf.h"
#include "TCameraDevice.h"
#include "AvfCapture.h"

namespace Avf
{
	TCaptureFormatMeta	GetMeta(AVCaptureDeviceFormat* Format);
	vec2f				GetMinMaxFrameRate(AVCaptureDeviceFormat* Format);
	void				EnumCaptureDevices(NSArray<AVCaptureDevice*>* Devices,std::function<void(const Avf::TDeviceMeta&)>& Enum,ArrayBridge<TCaptureFormatMeta>&& AdditionalFormats);
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
	
	
	TCaptureFormatMeta Meta;

	auto FormatDesc = Format.formatDescription;
	auto Fourcc = CMFormatDescriptionGetMediaSubType(FormatDesc);
	Meta.mCodec = Soy::TFourcc(Fourcc);
		
	Boolean usePixelAspectRatio = false;
	Boolean useCleanAperture = false;
	auto Dim = CMVideoFormatDescriptionGetPresentationDimensions( FormatDesc, usePixelAspectRatio, useCleanAperture );
	Meta.mPixelMeta.DumbSetWidth( Dim.width );
	Meta.mPixelMeta.DumbSetHeight( Dim.height );
	
	auto PixelFormat = Avf::GetPixelFormat(Fourcc);
	Meta.mPixelMeta.DumbSetFormat(PixelFormat);
	
	//	get specific bits
	//GetExtension( Desc, "FullRangeVideo", mMeta.mFullRangeYuv );
		
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

void Avf::EnumCaptureDevices(NSArray<AVCaptureDevice*>* Devices,std::function<void(const Avf::TDeviceMeta&)>& Enum,ArrayBridge<TCaptureFormatMeta>&& AdditionalFormats)
{
	for ( AVCaptureDevice* Device in Devices )
	{
		if ( !Device )
			continue;
		
		//	gr: just to aid debugging, skip non-video ones early
		{
			auto HasVideo = YES == [Device hasMediaType:AVMediaTypeVideo];
			if ( !HasVideo )
				continue;
		}
		
		auto Meta = GetMeta( Device );
		Meta.mFormats.PushBackArray(AdditionalFormats);
		
		if ( !Meta.mHasVideo )
			continue;

		Enum( Meta );
	}

}

void Avf::EnumCaptureDevices(std::function<void(const Avf::TDeviceMeta&)> Enum)
{
#if defined(TARGET_OSX)
	BufferArray<TCaptureFormatMeta,1> SpecialFormats;
	auto Devices = [AVCaptureDevice devices];
	EnumCaptureDevices(Devices,Enum,GetArrayBridge(SpecialFormats));
#else
	auto EnumDevices = [&](AVCaptureDeviceType DeviceType,SoyPixelsFormat::Type SpecialFormat)
	{
		auto* DeviceTypeArray = [NSArray arrayWithObjects: DeviceType,nil];
		//auto MediaType = AVMediaTypeDepthData;
		//auto MediaType = AVMediaTypeVideo;
		AVMediaType MediaType = nil;	//	any
		auto Position = AVCaptureDevicePositionUnspecified;
		auto* Discovery = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:DeviceTypeArray mediaType:MediaType position:Position];
		auto Devices = [Discovery devices];
		
		BufferArray<TCaptureFormatMeta,1> SpecialFormats;
		if ( SpecialFormat )
		{
			//	gr: copy w/h?
			TCaptureFormatMeta CaptureMeta;
			CaptureMeta.mPixelMeta.DumbSetFormat(SpecialFormat);
			SpecialFormats.PushBack(CaptureMeta);
		}
		
		EnumCaptureDevices(Devices,Enum,GetArrayBridge(SpecialFormats));
	};

	EnumDevices(AVCaptureDeviceTypeBuiltInWideAngleCamera,SoyPixelsFormat::Invalid);
	EnumDevices(AVCaptureDeviceTypeBuiltInTelephotoCamera,SoyPixelsFormat::Invalid);
	EnumDevices(AVCaptureDeviceTypeBuiltInDualCamera,SoyPixelsFormat::Invalid);
	EnumDevices(AVCaptureDeviceTypeBuiltInTrueDepthCamera,SoyPixelsFormat::Depth16mm);
	/*
	//	gr: the functions above don't find the back camera
	auto AllDevices = [AVCaptureDevice devices];
	BufferArray<TCaptureFormatMeta,1> SpecialFormats;
	EnumCaptureDevices(AllDevices,Enum,GetArrayBridge(SpecialFormats));
	 */
#endif
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

	EnumArFrameDevices(EnumName);
}

