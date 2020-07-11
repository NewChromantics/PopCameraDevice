#include "ArkitCapture.h"
#include "AvfVideoCapture.h"
#include "SoyAvf.h"
#import <ARKit/ARKit.h>



@interface ARSessionProxy : NSObject <ARSessionDelegate>
{
	Arkit::TSession*	mParent;
}

- (id)initWithSession:(Arkit::TSession*)parent;
- (void)session:(ARSession *)session didUpdateFrame:(ARFrame *)frame;

@end


class Arkit::TSession
{
public:
	TSession(bool RearCamera);
	~TSession();
	
	void				OnFrame(ARFrame* Frame);
	
	std::function<void(ARFrame*)>	mOnFrame;
	ObjcPtr<ARSessionProxy>			mSessionProxy;
	ObjcPtr<ARSession>				mSession;
};



@implementation ARSessionProxy


- (id)initWithSession:(Arkit::TSession*)parent
{
	self = [super init];
	if (self)
	{
		mParent = parent;
	}
	return self;
}

- (void)session:(ARSession *)session didUpdateFrame:(ARFrame *)frame
{
	mParent->OnFrame(frame);
}

@end




void Arkit::EnumDevices(std::function<void(const std::string&,ArrayBridge<std::string>&&)> EnumName)
{
	//	proxy device
	{
		Array<std::string> FormatStrings;
		FormatStrings.PushBack( ArFrameSource::ToString(ArFrameSource::capturedImage) );
		FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::capturedDepthData) );
		FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::FrontDepth) );
		FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::sceneDepth) );
		FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::RearDepth) );
		FormatStrings.PushBack(ArFrameSource::ToString(ArFrameSource::Depth) );
		EnumName( TFrameProxyDevice::DeviceName, GetArrayBridge(FormatStrings) );
	}
	
	//	if ( ArkitSupported )
	{
		Array<std::string> FormatStrings;
		EnumName( TSessionCamera::DeviceName_SceneDepth, GetArrayBridge(FormatStrings) );
		EnumName( TSessionCamera::DeviceName_FrontDepth, GetArrayBridge(FormatStrings) );
	}
}


Arkit::TSession::TSession(bool RearCamera)
{
	mSessionProxy.Retain([[ARSessionProxy alloc] initWithSession:(this)]);
	
	mSession.Retain( [[ARSession alloc] init] );
	mSession.mObject.delegate = mSessionProxy.mObject;

	//	rear
	//	ARWorldTrackingConfiguration
	//	ARBodyTrackingConfiguration
	//	AROrientationTrackingConfiguration
	//	ARImageTrackingConfiguration
	//	ARObjectScanningConfiguration
	
	//	"jsut position"
	//ARPositionalTrackingConfiguration
	
	//	gps
	//ARGeoTrackingConfiguration
	
	//	front camera
	//	ARFaceTrackingConfiguration
	
	ARConfiguration* Configuration;
	if ( RearCamera )
		Configuration = [ARWorldTrackingConfiguration alloc];
	else
		Configuration = [ARFaceTrackingConfiguration alloc];
	ARSessionRunOptions Options = 0;
	//ARSessionRunOptionResetTracking
	//ARSessionRunOptionRemoveExistingAnchors
	[mSession runWithConfiguration:Configuration options:(Options)];
}


Arkit::TSession::~TSession()
{
}

void Arkit::TSession::OnFrame(ARFrame* Frame)
{
	std::Debug << "New ARkit frame" << std::endl;
	if ( mOnFrame )
		mOnFrame( Frame );
}




Arkit::TFrameProxyDevice::TFrameProxyDevice(const std::string& Format)
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

void Arkit::TFrameProxyDevice::ReadNativeHandle(void* ArFrameHandle)
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
		
		PushFrame(Frame,mSource);
	}
	@catch (NSException* e)
	{
		std::stringstream Debug;
		Debug << "NSException " << Soy::NSErrorToString( e );
		throw Soy::AssertException(Debug);
	}
}


void Arkit::TFrameProxyDevice::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	Soy_AssertTodo();
}


void Arkit::TFrameDevice::PushFrame(ARFrame* Frame,ArFrameSource::Type Source)
{
	auto FrameTime = Soy::Platform::GetTime( Frame.timestamp );
	auto CapDepthTime = Soy::Platform::GetTime( Frame.capturedDepthDataTimestamp );
	std::Debug << "timestamp=" << FrameTime << " capturedDepthDataTimestamp=" << CapDepthTime << std::endl;
	
	//	read specific pixels
	switch( Source )
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
			
		default:break;
	}
	
	std::stringstream Debug;
	Debug << __PRETTY_FUNCTION__ << " unhandled source type " << Source;
	throw Soy::AssertException(Debug);
}

void Arkit::TFrameDevice::PushFrame(AVDepthData* DepthData,SoyTime Timestamp)
{
	auto DepthPixels = Avf::GetDepthPixelBuffer(DepthData);
	
	//	convert format
	//SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);
	//Soy::TFourcc DepthFormat(DepthData.depthDataType);
	auto Quality = magic_enum::enum_name(DepthData.depthDataQuality);
	auto Accuracy = magic_enum::enum_name(DepthData.depthDataAccuracy);
	auto IsFiltered = DepthData.depthDataFiltered;
	AVCameraCalibrationData* CameraCalibration = DepthData.cameraCalibrationData;
	
	PushFrame(DepthPixels,Timestamp);
}


void Arkit::TFrameDevice::PushFrame(CVPixelBufferRef PixelBuffer,SoyTime Timestamp)
{
	float3x3 Transform;
	auto DoRetain = true;
	std::string FrameMeta;
	auto PixelMeta = Avf::GetPixelMeta(PixelBuffer);
	std::shared_ptr<AvfDecoderRenderer> Renderer;
	
	std::shared_ptr<TPixelBuffer> Buffer( new CVPixelBuffer( PixelBuffer, DoRetain, Renderer, Transform ) );
	PopCameraDevice::TDevice::PushFrame( Buffer, PixelMeta, Timestamp, FrameMeta );
}



Arkit::TSessionCamera::TSessionCamera(const std::string& DeviceName,const std::string& Format)
{
	if ( DeviceName == DeviceName_SceneDepth )
		mSource = ArFrameSource::sceneDepth;
	else if ( DeviceName == DeviceName_FrontDepth )
		mSource = ArFrameSource::capturedDepthData;
	else
		throw Soy::AssertException( DeviceName + std::string(" is unhandled ARKit session camera name"));

	//	here we may need to join an existing session
	//	or create one with specific configuration
	//	or if we have to have 1 global, restart it with new configuration
	bool RearCameraSession = mSource == ArFrameSource::sceneDepth;
	mSession.reset( new TSession(RearCameraSession) );
	mSession->mOnFrame = [this](ARFrame* Frame)	{	this->OnFrame(Frame);	};
}

void Arkit::TSessionCamera::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	Soy_AssertTodo();
}

void Arkit::TSessionCamera::OnFrame(ARFrame* Frame)
{
	PushFrame( Frame, mSource );
}

