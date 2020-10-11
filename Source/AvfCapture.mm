#include "AvfCapture.h"
#include "AvfVideoCapture.h"
#include "SoyAvf.h"

#if defined(TARGET_IOS)
#import <ARKit/ARFrame.h>
#import <ARKit/ARSession.h>
#endif



Avf::TCamera::TCamera(const std::string& DeviceName,json11::Json& Options) :
	TDevice	( Options )
{
	//	for AVF, name has to be serial
	std::string Serial;
	Array<std::string> AllNames;
	auto MatchSerial = [&](const Avf::TDeviceMeta& Meta)
	{
		AllNames.PushBack( Meta.mName );
		if ( Meta.mName != DeviceName )
			return;
		Serial = Meta.mSerial;
	};
	EnumCaptureDevices( MatchSerial );
	
	if ( !Serial.length() )
	{
		std::stringstream Error;
		Error << DeviceName << " didn't match any device names; " << Soy::StringJoin( GetArrayBridge(AllNames), "," );
		throw Soy::AssertException( Error.str() );
	}
	
	
	Avf::TCaptureParams Params(Options);
	
	std::shared_ptr<Opengl::TContext> OpenglContext;
	
	mExtractor.reset(new AvfVideoCapture( Serial, Params, OpenglContext ));
	
	mExtractor->mOnFrame = [this](Avf::TFrame& Frame)
	{
		OnFrame(Frame);
	};
}

void Avf::TCamera::OnFrame(Avf::TFrame& Frame)
{
	this->PushFrame( Frame.mPixelBuffer, Frame.mTimestamp, Frame.mMeta );
}

void Avf::TCamera::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	std::stringstream Error;
	Error << "Todo feature enabling on AVF " << Feature;
	throw Soy::AssertException(Error.str());

}



