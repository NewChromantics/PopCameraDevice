#include "AvfCapture.h"
#include "AvfVideoCapture.h"

#import <ARKit/ARFrame.h>



Avf::TCamera::TCamera(const std::string& DeviceName,const std::string& Format)
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
	
	
	Avf::TCaptureParams Params;

	PopCameraDevice::DecodeFormatString( Format, Params.mPixelFormat, Params.mFrameRate );
	
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
	std::string Meta;
	SoyTime FrameTime = LatestPacket->GetStartTime();
	this->PushFrame( LatestPacket->mPixelBuffer, LatestPacket->mMeta.mPixelMeta, FrameTime, Meta );
}

void Avf::TCamera::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	std::stringstream Error;
	Error << "Todo feature enabling on AVF " << Feature;
	throw Soy::AssertException(Error.str());

}



void Avf::EnumArFrameDevices(std::function<void(const std::string&,ArrayBridge<std::string>&&)> EnumName)
{
	Array<std::string> FormatStrings;
	FormatStrings.PushBack( ArFrameSource::ToString(ArFrameSource::capturedImage) );
	FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::capturedDepthData) );
	FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::FrontDepth) );
	FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::sceneDepth) );
	FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::RearDepth) );
	FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::Depth) );
	EnumName( TArFrameProxy::DeviceName, GetArrayBridge(FormatStrings) );
}

Avf::TArFrameProxy::TArFrameProxy(const std::string& Format)
{
	if ( !Format.empty() )
		mSource = ArFrameSource::ToType(Format);
	
	//	convert aliases
	if ( mSource == ArFrameSource::Depth )			mSource = ArFrameSource::RearDepth;
	if ( mSource == ArFrameSource::FrontColour )	mSource = ArFrameSource::capturedImage;
	if ( mSource == ArFrameSource::FrontDepth )		mSource = ArFrameSource::capturedDepthData;
	if ( mSource == ArFrameSource::RearDepth )		mSource = ArFrameSource::sceneDepth;

	std::Debug << __PRETTY_FUNCTION__ << " using " << mSource << std::endl;
}

void Avf::TArFrameProxy::ReadNativeHandle(void* ArFrameHandle)
{
	if ( !ArFrameHandle )
		throw Soy::AssertException("ReadNativeHandle null");
	
	//	gr: do some type check!
	auto* Frame = (__bridge ARFrame*)(ArFrameHandle);
	auto FrameTime = Soy::Platform::GetTime( Frame.timestamp );
	auto CapTime = Soy::Platform::GetTime( Frame.capturedDepthDataTimestamp );
	std::Debug << "timestamp=" << FrameTime << " capturedDepthDataTimestamp=" << CapTime << std::endl;
}


void Avf::TArFrameProxy::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	Soy_AssertTodo();
}


