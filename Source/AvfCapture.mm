#include "AvfCapture.h"
#include "AvfVideoCapture.h"
#include "SoyAvf.h"
#import <ARKit/ARFrame.h>
#import <ARKit/ARSession.h>



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

//	https://docs.unity3d.com/Packages/com.unity.xr.arfoundation@3.0/manual/extensions.html
typedef struct UnityXRNativeSessionPtr
{
	int version;
	void* session;
} UnityXRNativeSessionPtr;

void Avf::TArFrameProxy::ReadNativeHandle(void* ArFrameHandle)
{
	if ( !ArFrameHandle )
		throw Soy::AssertException("ReadNativeHandle null");
	
	auto& UnitySessionPtr = *reinterpret_cast<UnityXRNativeSessionPtr*>(ArFrameHandle);
	@try
	{
		//	gr: do some type check!
		auto* ArSession = (__bridge ARSession*)(UnitySessionPtr.session);
		if ( !ArSession )
			throw Soy::AssertException("Failed to cast bridged ARSession pointer");
		
		auto* Frame = ArSession.currentFrame;
		if ( !Frame )
			throw Soy::AssertException("Failed to cast bridged ARFrame pointer");
		
		auto FrameTime = Soy::Platform::GetTime( Frame.timestamp );
		auto CapDepthTime = Soy::Platform::GetTime( Frame.capturedDepthDataTimestamp );
		std::Debug << "timestamp=" << FrameTime << " capturedDepthDataTimestamp=" << CapDepthTime << std::endl;
		
		//	read specific pixels
		switch( mSource )
		{
			case ArFrameSource::capturedImage:
				PushFrame( Frame.capturedImage, FrameTime );
				return;
			
			case ArFrameSource::capturedDepthData:
				PushFrame( Frame.capturedDepthData, CapDepthTime );
				return;
			/*
			case ArFrameSource::sceneDepth:
				PushFrame( Frame.sceneDepth, FrameTime );
				return;
			 */
		}
		
		std::stringstream Debug;
		Debug << __PRETTY_FUNCTION__ << " unhandled source type " << mSource;
		throw Soy::AssertException(Debug);
	}
	@catch (NSException* e)
	{
		std::stringstream Debug;
		Debug << "NSException " << Soy::NSErrorToString( e );
		throw Soy::AssertException(Debug);
	}
}


void Avf::TArFrameProxy::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	Soy_AssertTodo();
}

void Avf::TArFrameProxy::PushFrame(AVDepthData* DepthData,SoyTime Timestamp)
{
	if ( !DepthData )
		throw Soy::AssertException("AVDepthData null");
	
	//	convert to the format we want, then call again
	Soy::TFourcc DepthFormat(DepthData.depthDataType);
	auto DepthPixelFormat = Avf::GetPixelFormat(DepthFormat.mFourcc32);
	SoyPixelsFormat::Type OutputFormat = SoyPixelsFormat::DepthFloatMetres;
	if ( DepthPixelFormat != OutputFormat )
	{
		auto OutputFourcc = Avf::GetPlatformPixelFormat(OutputFormat);
		auto NewDepthData = [DepthData depthDataByConvertingToDepthDataType:OutputFourcc];
		if ( !NewDepthData )
			throw Soy::AssertException("Failed to convert depth data to desired format");
		//	gr: need to be careful about recursion here
		PushFrame(NewDepthData,Timestamp);
		return;
	}
	
	//	convert format
	//SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);
	auto DepthPixels = DepthData.depthDataMap;
	//Soy::TFourcc DepthFormat(DepthData.depthDataType);
	auto Quality = magic_enum::enum_name(DepthData.depthDataQuality);
	auto Accuracy = magic_enum::enum_name(DepthData.depthDataAccuracy);
	auto IsFiltered = DepthData.depthDataFiltered;
	AVCameraCalibrationData* CameraCalibration = DepthData.cameraCalibrationData;
	
	PushFrame(DepthPixels,Timestamp);
}

void Avf::TArFrameProxy::PushFrame(CVPixelBufferRef PixelBuffer,SoyTime Timestamp)
{
	auto Height = CVPixelBufferGetHeight( PixelBuffer );
	auto Width = CVPixelBufferGetWidth( PixelBuffer );
	auto Format = CVPixelBufferGetPixelFormatType( PixelBuffer );
	auto SoyFormat = Avf::GetPixelFormat( Format );

	float3x3 Transform;
	auto DoRetain = true;
	std::string FrameMeta;
	SoyPixelsMeta PixelMeta( Width, Height, SoyFormat );
	std::shared_ptr<AvfDecoderRenderer> Renderer;
	
	std::shared_ptr<TPixelBuffer> Buffer( new CVPixelBuffer( PixelBuffer, DoRetain, Renderer, Transform ) );
	PopCameraDevice::TDevice::PushFrame( Buffer, PixelMeta, Timestamp, FrameMeta );
}

