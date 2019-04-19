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



void Avf::EnumCaptureDevices(std::function<void(const std::string&)> AppendName)
{
	auto Devices = [AVCaptureDevice devices];
	for ( AVCaptureDevice* Device in Devices )
	{
		if ( !Device )
			continue;
		
		auto Meta = GetMeta( Device );
		if ( !Meta.mHasVideo )
			continue;

		AppendName( Meta.mCookie );
	}
}

