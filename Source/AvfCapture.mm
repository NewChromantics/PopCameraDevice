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
	
	mExtractor->mOnPacketQueued = [this](const SoyTime,size_t StreamIndex)
	{
		PushLatestFrame(StreamIndex);
	};
}

void Avf::TCamera::PushLatestFrame(size_t StreamIndex)
{
	if ( !mExtractor )
	{
		std::Debug << "MediaFoundation::TCamera::PushLatestFrame(" << StreamIndex << ") null extractor" << std::endl;
		return;
	}
	
	auto LatestPacket = mExtractor->PopPacket(StreamIndex);
	if ( !LatestPacket )
		return;
	
	//	todo: get all the frame meta
	json11::Json::object Meta;
	SoyTime FrameTime = LatestPacket->GetStartTime();
	this->PushFrame( LatestPacket->mPixelBuffer, FrameTime, Meta );
}

void Avf::TCamera::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	std::stringstream Error;
	Error << "Todo feature enabling on AVF " << Feature;
	throw Soy::AssertException(Error.str());

}



