#include "AvfCapture.h"
#include "AvfVideoCapture.h"



Avf::TCamera::TCamera(const std::string& DeviceName)
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
	
	TMediaExtractorParams Params(Serial);
	
	//	gr: pitch padding is crashing, my padding code might be wrong... but none of my cameras are giving out unaligned images...
	Params.mApplyHeightPadding = false;
	Params.mApplyWidthPadding = false;
	
	std::shared_ptr<Opengl::TContext> OpenglContext;
	
	mExtractor.reset(new AvfVideoCapture(Params,OpenglContext));
	
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
	std::string Meta;
	this->PushFrame(LatestPacket->mPixelBuffer, LatestPacket->mMeta.mPixelMeta, Meta );
}

void Avf::TCamera::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	std::stringstream Error;
	Error << "Todo feature enabling on AVF " << Feature;
	throw Soy::AssertException(Error.str());

}

