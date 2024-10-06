#include "ArkitCapture.h"
#include "AvfVideoCapture.h"
#include "SoyAvf.h"
#import <ARKit/ARKit.h>
#include "Json11/json11.hpp"
#include "PopCameraDevice.h"
#include "JsonFunctions.h"

//  gr: make this a proper check, quickly disabling for build here
#define ENABLE_IOS14    (__IPHONE_OS_VERSION_MAX_ALLOWED >= 140000)
#define ENABLE_IOS13    (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
#define ENABLE_IOS12   (__IPHONE_OS_VERSION_MAX_ALLOWED >= 120000)

#pragma warning ARKit sdk __IPHONE_OS_VERSION_MIN_REQUIRED

#if ENABLE_IOS12
#pragma message("IOS 12")
#endif

#if ENABLE_IOS13
#pragma message("IOS 13")
#endif

#if ENABLE_IOS14
#pragma message("IOS 14")
#endif

namespace Arkit
{
	const char*	GeometryStreamName = "Geometry";
	const char*	GetColourStreamName(ArFrameSource::Type Source);
	
	

	float4x4			SimdToFloat4x4(simd_float4x4 simd);
};


float4x4 Arkit::SimdToFloat4x4(simd_float4x4 simd)
{
	float4x4 Matrix(
		simd.columns[0][0],	simd.columns[1][0],	simd.columns[2][0],	simd.columns[3][0],	
		simd.columns[0][1],	simd.columns[1][1],	simd.columns[2][1],	simd.columns[3][1],	
		simd.columns[0][2],	simd.columns[1][2],	simd.columns[2][2],	simd.columns[3][2],	
		simd.columns[0][3],	simd.columns[1][3],	simd.columns[2][3],	simd.columns[3][3]	
	);
	return Matrix;	
}

@interface ARSessionProxy : NSObject<ARSessionObserver,ARSCNViewDelegate,ARSessionDelegate>
{
	Arkit::TSession*	mParent;
}

- (id)initWithSession:(Arkit::TSession*)parent;
- (void)session:(ARSession *)session didUpdateFrame:(ARFrame *)frame;

- (void)session:(ARSession *)session didFailWithError:(NSError *)error;
- (void)session:(ARSession *)session cameraDidChangeTrackingState:(ARCamera *)camera;
- (void)sessionWasInterrupted:(ARSession *)session;
- (void)sessionInterruptionEnded:(ARSession *)session;
- (BOOL)sessionShouldAttemptRelocalization:(ARSession *)session API_AVAILABLE(ios(11.3));
- (void)session:(ARSession *)session didOutputAudioSampleBuffer:(CMSampleBufferRef)audioSampleBuffer;
- (void)session:(ARSession *)session didOutputCollaborationData:(ARCollaborationData *)data API_AVAILABLE(ios(13.0));
- (void)session:(ARSession *)session didChangeGeoTrackingStatus:(ARGeoTrackingStatus*)geoTrackingStatus API_AVAILABLE(ios(14.0));

- (void)session:(ARSession *)session didAddAnchors:(NSArray<__kindof ARAnchor*>*)anchors;
- (void)session:(ARSession *)session didUpdateAnchors:(NSArray<__kindof ARAnchor*>*)anchors;
- (void)session:(ARSession *)session didRemoveAnchors:(NSArray<__kindof ARAnchor*>*)anchors;

@end


class Arkit::TSession
{
public:
	TSession(bool RearCamera,TCaptureParams& Params);
	~TSession();
	
	void				OnFrame(ARFrame* Frame);
	void				OnAnchorChange(ARAnchor* Anchor,AnchorChange::Type Change);
	
	std::function<void(ARFrame*)>	mOnFrame;
	std::function<void(ARAnchor*,AnchorChange::Type)>	mOnAnchorChange;
	ObjcPtr<ARSessionProxy>			mSessionProxy;
	ObjcPtr<ARSession>				mSession;

	//	the test app sits on the main thread, so no frames were coming through
	dispatch_queue_t				mQueue = nullptr;
	TCaptureParams					mParams;
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
	try
	{
		mParent->OnFrame(frame);
	}
	catch(std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << " exception: " << e.what() << std::endl;
	}
}

- (void)session:(ARSession *)session didFailWithError:(NSError *)error
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (void)session:(ARSession *)session cameraDidChangeTrackingState:(ARCamera *)camera
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (void)sessionWasInterrupted:(ARSession *)session
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (void)sessionInterruptionEnded:(ARSession *)session
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (BOOL)sessionShouldAttemptRelocalization:(ARSession *)session
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	return true;
}

- (void)session:(ARSession *)session didOutputAudioSampleBuffer:(CMSampleBufferRef)audioSampleBuffer
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (void)session:(ARSession *)session didOutputCollaborationData:(ARCollaborationData *)data
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

- (void)session:(ARSession *)session didChangeGeoTrackingStatus:(ARGeoTrackingStatus*)geoTrackingStatus
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}


- (void)session:(ARSession *)session didAddAnchors:(NSArray<__kindof ARAnchor*>*)anchors
{
	try
	{
		for (ARAnchor* Anchor in anchors)
		{
			mParent->OnAnchorChange(Anchor,Arkit::AnchorChange::Added);
		}
	}
	catch(std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << " exception: " << e.what() << std::endl;
	}
}

- (void)session:(ARSession *)session didUpdateAnchors:(NSArray<__kindof ARAnchor*>*)anchors
{
	try
	{
		for (ARAnchor* Anchor in anchors)
		{
			mParent->OnAnchorChange(Anchor,Arkit::AnchorChange::Updated);
		}
	}
	catch(std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << " exception: " << e.what() << std::endl;
	}
}

- (void)session:(ARSession *)session didRemoveAnchors:(NSArray<__kindof ARAnchor*>*)anchors
{
	try
	{
		for (ARAnchor* Anchor in anchors)
		{
			mParent->OnAnchorChange(Anchor,Arkit::AnchorChange::Removed);
		}
	}
	catch(std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << " exception: " << e.what() << std::endl;
	}
}

@end


Arkit::TCaptureParams::TCaptureParams(json11::Json& Options)
{
	auto SetInt = [&](const char* Name,size_t& ValueUnsigned)
	{
		auto& Handle = Options[Name];
		if ( !Handle.is_number() )
			return false;
		auto Value = Handle.int_value();
		if ( Value < 0 )
		{
			std::stringstream Error;
			Error << "Value for " << Name << " is " << Value << ", not expecting negative";
			throw Soy::AssertException(Error);
		}
		ValueUnsigned = Value;
		return true;
	};
	auto SetBool = [&](const char* Name,bool& Value)
	{
		auto& Handle = Options[Name];
		if ( !Handle.is_bool() )
			return false;
		Value = Handle.bool_value();
		return true;
	};
	auto SetString = [&](const char* Name,std::string& Value)
	{
		auto& Handle = Options[Name];
		if ( !Handle.is_string() )
			return false;
		Value = Handle.string_value();
		return true;
	};

	auto SetPixelFormat = [&](const char* Name,SoyPixelsFormat::Type& Value)
	{
		std::string EnumString;
		if ( !SetString(Name,EnumString) )
			return false;
		
		Value = SoyPixelsFormat::Validate(EnumString);
		return true;
	};

	SetBool( POPCAMERADEVICE_KEY_HDRCOLOUR, mHdrColour );
	SetBool( POPCAMERADEVICE_KEY_ENABLEAUTOFOCUS, mEnableAutoFocus );
	SetBool( POPCAMERADEVICE_KEY_ENBALEPLANETRACKING, mEnablePlanesHorz );
	SetBool( POPCAMERADEVICE_KEY_ENABLEFACETRACKING, mEnableFaceTracking );
	SetBool( POPCAMERADEVICE_KEY_ENABLELIGHTESTIMATION, mEnableLightEstimation );
	SetBool( POPCAMERADEVICE_KEY_ENABLEBODYTRACKING, mEnableBodyDetection );
	SetBool( POPCAMERADEVICE_KEY_ENABLESEGMENTATION, mEnablePersonSegmentation );
	SetBool( POPCAMERADEVICE_KEY_RESETTRACKING, mResetTracking );
	SetBool( POPCAMERADEVICE_KEY_RESETANCHORS, mResetAnchors );
	SetBool( POPCAMERADEVICE_KEY_FEATURES, mOutputFeatures );
	SetBool( POPCAMERADEVICE_KEY_ANCHORS, mOutputAnchors );	
	SetBool( POPCAMERADEVICE_KEY_ANCHORGEOMETRYSTREAM, mEnableAnchorGeometry );	
	SetBool( POPCAMERADEVICE_KEY_WORLDGEOMETRYSTREAM, mEnableWorldGeometry );	
	SetBool( POPCAMERADEVICE_KEY_DEPTHCONFIDENCE, mOutputSceneDepthConfidence );
	SetBool( POPCAMERADEVICE_KEY_DEPTHSMOOTH, mOutputSceneDepthSmooth );
	SetBool( POPCAMERADEVICE_KEY_DEBUG, mVerboseDebug );

	//	probably don't need this to be seperate
	mEnablePlanesVert = mEnablePlanesHorz;
	

	//	any format = enable	
	auto DepthFormat = SoyPixelsFormat::Invalid;
	SetPixelFormat( POPCAMERADEVICE_KEY_DEPTHFORMAT, DepthFormat );
	mEnableSceneDepth = ( DepthFormat != SoyPixelsFormat::Invalid );
	
	SetPixelFormat( POPCAMERADEVICE_KEY_FORMAT, mColourFormat );
}

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


Arkit::TSession::TSession(bool RearCamera,TCaptureParams& Params) :
	mParams	( Params )
{
	mSessionProxy.Retain([[ARSessionProxy alloc] initWithSession:(this)]);
	
	mSession.Retain( [[ARSession alloc] init] );
	auto* Session = mSession.mObject;

	id<ARSessionDelegate> Proxy = mSessionProxy.mObject;

	mQueue = dispatch_queue_create( __PRETTY_FUNCTION__, NULL);
		
	Session.delegate = Proxy;
	Session.delegateQueue = mQueue;
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
	auto WorldSupported = [ARWorldTrackingConfiguration isSupported];		
	auto WorldSupportsMeshing = [ARWorldTrackingConfiguration supportsSceneReconstruction:ARSceneReconstructionMesh];
	auto WorldSupportsMeshingWithClassification = [ARWorldTrackingConfiguration supportsSceneReconstruction:ARSceneReconstructionMeshWithClassification];
	auto WorldSupportsFaceTracking = [ARWorldTrackingConfiguration supportsUserFaceTracking];
	auto FrontSupported = [ARFaceTrackingConfiguration isSupported];		
	auto BodyTrackingSupported = [ARBodyTrackingConfiguration isSupported];
	
	NSArray<ARVideoFormat *>* WorldVideoFormats = ARWorldTrackingConfiguration.supportedVideoFormats;
	
	std::Debug << "Rear supported=" << WorldSupported << std::endl;
	std::Debug << "Rear SupportsMeshing=" << WorldSupportsMeshing << std::endl;
	std::Debug << "Rear SupportsMeshingWithClassification=" << WorldSupportsMeshingWithClassification << std::endl;
	std::Debug << "Rear SupportsFaceTracking=" << WorldSupportsFaceTracking << std::endl;
	std::Debug << "FrontSupported=" << FrontSupported << std::endl;
	std::Debug << "BodyTrackingSupported=" << BodyTrackingSupported << std::endl;

	std::Debug << "Rear/world ARVideoFormats x" << [WorldVideoFormats count] << std::endl;
	auto DebugVideoFormat = [&](ARVideoFormat* Format)
	{
		if ( !Format )
		{
			std::Debug << "ARVideoFormat* <null>"<<std::endl;
			return;
		}
		auto Size = Format.imageResolution;
		auto Fps = Format.framesPerSecond;
		std::Debug << Size.width << "x" << Size.height << " at " << Fps << " fps" << std::endl;
	};
	Platform::NSArray_ForEach<ARVideoFormat*>(WorldVideoFormats,DebugVideoFormat);

	
	ARConfiguration* Configuration = nullptr;
	if ( RearCamera )
	{
		auto* WorldConfig = [[ARWorldTrackingConfiguration alloc] init];
		Configuration = WorldConfig;
		
		//	oo other format!
		WorldConfig.wantsHDREnvironmentTextures = Params.mHdrColour;	

		//	on by default
		WorldConfig.autoFocusEnabled = Params.mEnableAutoFocus;

		//	gr: does this need to be if colourformat != null ?
		//	AREnvironmentTexturingNone, AREnvironmentTexturingManual, AREnvironmentTexturingAutomatic
		WorldConfig.environmentTexturing = AREnvironmentTexturingAutomatic;
		
		//	enable various options
		WorldConfig.planeDetection = ARPlaneDetectionNone;
		
		//	gr: forced this on as I didnt seem to get any world geo... 
		//		but it still did take a while, so not sure if needed or not
		//		docs say enabling just improves the mesh (flatter planes) 
		auto ForceEnablePlaneTracking = Params.mEnableWorldGeometry;
		
		if ( Params.mEnablePlanesHorz || ForceEnablePlaneTracking )
			WorldConfig.planeDetection |= ARPlaneDetectionHorizontal;
		if ( Params.mEnablePlanesVert || ForceEnablePlaneTracking )
			WorldConfig.planeDetection |= ARPlaneDetectionVertical;
			
		// NSSet<ARReferenceImage *> *detectionImages API_AVAILABLE(ios(11.3));

		//	enable face tracking (not supported)
		WorldConfig.userFaceTrackingEnabled = Params.mEnableFaceTracking && WorldSupportsFaceTracking;
	
		WorldConfig.lightEstimationEnabled = Params.mEnableLightEstimation;
		if ( @available(iOS 13.4, *) ) 
		{
			WorldConfig.sceneReconstruction = Params.mEnableWorldGeometry && WorldSupportsMeshing ? ARSceneReconstructionMesh : ARSceneReconstructionNone;
		}
		
		WorldConfig.frameSemantics = ARFrameSemanticNone;
		if ( Params.mEnableBodyDetection )
			WorldConfig.frameSemantics |= ARFrameSemanticBodyDetection;
		if ( @available(iOS 14.0, *) )
		{
			if ( Params.mEnableSceneDepth )
				WorldConfig.frameSemantics |= ARFrameSemanticSceneDepth;
			if ( Params.mOutputSceneDepthSmooth )
				WorldConfig.frameSemantics |= ARFrameSemanticSmoothedSceneDepth;
		}
		if ( Params.mEnablePersonSegmentation )
			WorldConfig.frameSemantics |= ARFrameSemanticPersonSegmentation;
		
		WorldConfig.worldAlignment = ARWorldAlignmentGravityAndHeading;
		WorldConfig.worldAlignment = ARWorldAlignmentCamera;
		WorldConfig.worldAlignment = ARWorldAlignmentGravity;
		
		//	todo: use colour format
		if ( [WorldVideoFormats count] == 0 )
		{
			//throw Soy::AssertException("Zero camera world-video-formats - can we continue with no format?");
		}
		else
		{
			//	gr: this was 2 (for ipad), but only 2 formats on iphone SE
			//	todo: pick the right one
			WorldConfig.videoFormat = WorldVideoFormats[0];
		}
	}
	else
	{
		Configuration = [[ARFaceTrackingConfiguration alloc] init];
	}
	
	ARSessionRunOptions Options = 0;
	if ( Params.mResetTracking )
		Options |= ARSessionRunOptionResetTracking;
	if ( Params.mResetAnchors )
		Options |= ARSessionRunOptionRemoveExistingAnchors;

	if ( Options == 0 )
		[Session runWithConfiguration:Configuration];
	else
		[Session runWithConfiguration:Configuration options:(Options)];
	
	auto* Frame = Session.currentFrame;
	std::Debug << "Currentframe " << Frame << std::endl;
}


Arkit::TSession::~TSession()
{
	//	delete queue
	if ( mQueue )
	{
#if !defined(ARC_ENABLED)
		dispatch_release( mQueue );
#endif
		mQueue = nullptr;
	}
	
	mSessionProxy.Release();
	mSession.Release();
}

void Arkit::TSession::OnFrame(ARFrame* Frame)
{
	if ( mParams.mVerboseDebug )
		std::Debug << "New ARkit frame" << std::endl;
	if ( mOnFrame )
		mOnFrame( Frame );
}

void Arkit::TSession::OnAnchorChange(ARAnchor* Anchor,AnchorChange::Type Change)
{
	if ( mParams.mVerboseDebug )
	{
		auto Uuid = Soy::NSStringToString(Anchor.identifier.UUIDString);
		std::Debug << "Anchor change " << Uuid << " (" << magic_enum::enum_name(Change) << ")" << std::endl;
	}
	if ( mOnAnchorChange )
		mOnAnchorChange( Anchor, Change );
}




Arkit::TFrameProxyDevice::TFrameProxyDevice(json11::Json& Options) :
	TFrameDevice	( Options )
{
	if ( Options["Source"].is_string() )
	{
		auto Source = Options["Source"].string_value();
		mSource = ArFrameSource::ToType(Source);
	}
	
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


#if !ENABLE_IOS13
class ARSkeleton2D
{
	
};
#endif

namespace Avf
{
	void	GetMeta(ARFrame* Frame,json11::Json::object& Meta,Arkit::TCaptureParams& Params);
	void	GetMeta(AVDepthData* Depth,json11::Json::object& Meta);
	void	GetMeta(ARCamera* Camera,json11::Json::object& Meta);
	void	GetMeta(ARDepthData* Camera,json11::Json::object& Meta);

	json11::Json::object	GetSkeleton(ARSkeleton2D* Skeleton);
}



namespace Avf
{
	namespace GeoTrackingState
	{
		enum Type
		{
			Invalid,
#if ENABLE_IOS14
			NotAvailable = ARGeoTrackingStateNotAvailable,
			Initializing = ARGeoTrackingStateInitializing,
			Localizing = ARGeoTrackingStateLocalizing,
			Localized = ARGeoTrackingStateLocalized
#endif
		};
		DECLARE_SOYENUM(GeoTrackingState);
	}

	namespace GeoTrackingAccuracy
	{
		enum Type
		{
			Invalid,
#if ENABLE_IOS14
			Undetermined = ARGeoTrackingAccuracyUndetermined,
			Low = ARGeoTrackingAccuracyLow,
			Medium = ARGeoTrackingAccuracyMedium,
			High = ARGeoTrackingAccuracyHigh
#endif
		};
		DECLARE_SOYENUM(GeoTrackingAccuracy);
	}

	namespace GeoTrackingStateReason
	{
		enum Type
		{
			Invalid,
#if ENABLE_IOS14
			None = ARGeoTrackingStateReasonNone,
			NotAvailableAtLocation = ARGeoTrackingStateReasonNotAvailableAtLocation,
			NeedLocationPermissions = ARGeoTrackingStateReasonNeedLocationPermissions,
			WorldTrackingUnstable = ARGeoTrackingStateReasonWorldTrackingUnstable,
			WaitingForLocation = ARGeoTrackingStateReasonWaitingForLocation,
			GeoDataNotLoaded = ARGeoTrackingStateReasonGeoDataNotLoaded,
			DevicePointedTooLow = ARGeoTrackingStateReasonDevicePointedTooLow,
			VisualLocalizationFailed = ARGeoTrackingStateReasonVisualLocalizationFailed,
#endif
		};
		DECLARE_SOYENUM(GeoTrackingStateReason);
	}
}

bool IsIdentity(simd_float4x4 Matrix)
{
	static const simd_float4x4 Identity = 
	{
		.columns[0]={1,0,0,0},
		.columns[1]={0,1,0,0},
		.columns[2]={0,0,1,0},
		.columns[3]={0,0,0,1} 
	};
	for ( int c=0;	c<std::size(Matrix.columns);	c++ )
		for ( int r=0;	r<4;	r++ )
			if ( Matrix.columns[c][r] != Identity.columns[c][r] )
				return false;
	return true;
}

void Avf::GetMeta(ARCamera* Camera,json11::Json::object& Meta)
{
	//	"rotation and translation in world space"
	//	so camera to world?
	auto LocalToWorld = GetJsonArray(Camera.transform);	//	todo: this needs to be transposed!
	auto LocalEulerRadians = GetJsonArray( Camera.eulerAngles );
	auto Tracking = magic_enum::enum_name(Camera.trackingState);
	auto TrackingStateReason = magic_enum::enum_name(Camera.trackingStateReason);
	auto Intrinsics = GetJsonArray(Camera.intrinsics);
	auto ProjectionMatrix = GetJsonArray(Camera.projectionMatrix);	//	todo: transpose and convert to 4x4
	auto CameraResolution = GetJsonArray(Camera.imageResolution);
	
	//	skip transform if it's identity rather than write out bad data
	if ( !IsIdentity(Camera.transform) )
		Meta["LocalToWorld"] = LocalToWorld;
	
	Meta["LocalEulerRadians"] = LocalEulerRadians;
	Meta["Tracking"] = std::string(Tracking);	//	json11 can't do string_view atm
	Meta["TrackingStateReason"] = std::string(TrackingStateReason);	//	json11 can't do string_view atm
	Meta["Intrinsics"] = Intrinsics;
	Meta["ProjectionMatrix"] = ProjectionMatrix;
	//	write out original resolution to match Intrinsics matrix in case image gets resized
	//	gr: convert to normalised projection matrix here!
	Meta["IntrinsicsCameraResolution"] = CameraResolution;
}


//ARSkeletonJointNameHead
json11::Json::object Avf::GetSkeleton(ARSkeleton2D* Skeleton)
{
	//	gr: cache this as the definition doesn't change
	//		then copy it and replace values
	json11::Json::object SkeletonJson;
	
	
#if ENABLE_IOS14
	auto EnumJoint = [&](NSString* Name)
	{
		auto Position2 = [Skeleton landmarkForJointNamed:Name];
		auto PositionArray = GetJsonArray(Position2);
		auto Key = Soy::NSStringToString(Name);
		SkeletonJson[Key] = PositionArray;
	};
	
	auto Definition = Skeleton.definition;
	auto JointNames = Definition.jointNames;
	Platform::NSArray_ForEach<NSString*>(JointNames,EnumJoint);
#endif

	return SkeletonJson;
}


std::string GetClassName(NSObject* Object)
{
	auto* Class = [Object class];
	auto ClassNameNs = NSStringFromClass(Class);
	auto ClassName = Soy::NSStringToString(ClassNameNs);
	return ClassName;
}


void GetAnchorTriangles(ARPlaneGeometry* Geometry,ArrayBridge<float>&& PositionFloats,size_t PositionSize)
{
	for ( int t=0;	t<Geometry.triangleCount;	t++ )
	{
		auto ti = t * 3;
		int16_t i0 = Geometry.triangleIndices[ti+0];
		int16_t i1 = Geometry.triangleIndices[ti+1];
		int16_t i2 = Geometry.triangleIndices[ti+2];
		auto& v0 = Geometry.vertices[i0];
		auto& v1 = Geometry.vertices[i1];
		auto& v2 = Geometry.vertices[i2];
		auto w = 1.0f;
		
		PositionFloats.PushBack( v0.x );
		PositionFloats.PushBack( v0.y );
		PositionFloats.PushBack( v0.z );
		if ( PositionSize == 4 )	
			PositionFloats.PushBack(w);
			
		PositionFloats.PushBack( v1.x );
		PositionFloats.PushBack( v1.y );
		PositionFloats.PushBack( v1.z );
		if ( PositionSize == 4 )	
			PositionFloats.PushBack(w);

		PositionFloats.PushBack( v2.x );
		PositionFloats.PushBack( v2.y );
		PositionFloats.PushBack( v2.z );
		if ( PositionSize == 4 )	
			PositionFloats.PushBack(w);
	}

}


void GetAnchorMeta(ARPlaneAnchor* Anchor, json11::Json::object& Meta,bool IncludeGeometry)
{
	//	gr: geometry gets big, so really should only use the geometry stream now
	//		and maybe for convinence, still output normal and bounds?

	/*	gr: center + extent is just bounds, actual plane is dictated by the transform
	//	always add center & extent as a 4 float plane
	json11::Json::array Plane4;
	Plane4.push_back( Anchor.center.x );
	Plane4.push_back( Anchor.center.y );
	Plane4.push_back( Anchor.center.z );
	Plane4.push_back( Anchor.extent.x );
	Plane4.push_back( Anchor.extent.y );	//	always 0
	Plane4.push_back( Anchor.extent.z );
	Meta["Plane"] = Plane4;
	*/
	
	if ( Anchor.geometry )
	{
		if ( IncludeGeometry )
		{
			Array<float> TrianglePositions;
			GetAnchorTriangles( Anchor.geometry, GetArrayBridge(TrianglePositions), 3 );
			Meta["Triangles"] = GetJsonArray(GetArrayBridge(TrianglePositions));
		}
	}
}

//	return false to not report this anchor
bool GetAnchorMeta(ARAnchor* Anchor, json11::Json::object& Meta,bool IncludeGeometry)
{
	if ( [Anchor isKindOfClass:[ARPlaneAnchor class]] )
	{
		GetAnchorMeta( (ARPlaneAnchor*)Anchor, Meta, IncludeGeometry );
	}
	else if ( [Anchor isKindOfClass:[ARMeshAnchor class]] )
	{
		//GetAnchorMeta( (ARMeshAnchor*)Anchor, Meta );
		return false;
	}
	else
	{
		//	todo: ARObjectAnchor	https://developer.apple.com/documentation/arkit/arobjectanchor?language=objc
		//	todo: ARImageAnchor	https://developer.apple.com/documentation/arkit/arimageanchor?language=objc
		//	todo: ARFaceAnchor	https://developer.apple.com/documentation/arkit/arfaceanchor?language=objc
		//	skip
		return false;
	}
	
	auto Uuid = Soy::NSStringToString(Anchor.identifier.UUIDString);
	auto Name = Soy::NSStringToString(Anchor.name);
	auto Type = GetClassName(Anchor);//[Anchor isKindOfClass:[ARPlaneAnchor class]]
#if ENABLE_IOS14
	auto SessionUuid = Soy::NSStringToString(Anchor.sessionIdentifier.UUIDString);
#endif
	auto LocalToWorld = GetJsonArray(Anchor.transform);

	Meta["Uuid"] = Uuid;
#if ENABLE_IOS14
	Meta["SessionUuid"] = SessionUuid;
#endif
	Meta["LocalToWorld"] = LocalToWorld;
	//	omit name if its null
	if ( Anchor.name )
	{
		Meta["Name"] = Name;
	}
	return true;
}

void EnumGeometryAnchors(ARFrame* Frame,std::function<void(ARPlaneAnchor* PlaneAnchor,ARMeshAnchor* MeshAnchor)> Enum)
{
	auto EnumAnchor = [&](ARAnchor* Anchor)
	{
		if ( [Anchor isKindOfClass:[ARPlaneAnchor class]] )
		{
			Enum( (ARPlaneAnchor*)Anchor, nullptr );
		}
		if ( [Anchor isKindOfClass:[ARMeshAnchor class]] )
		{
			Enum( nullptr, (ARMeshAnchor*)Anchor );
		}
	};
	Platform::NSArray_ForEach<ARAnchor*>(Frame.anchors,EnumAnchor);
}
	
void Avf::GetMeta(ARFrame* Frame,json11::Json::object& Meta,Arkit::TCaptureParams& Params)
{
	if ( !Frame )
		return;
	
	if ( Frame.camera )
	{
		json11::Json::object CameraMeta;
		GetMeta(Frame.camera,CameraMeta);
		Meta["Camera"] = CameraMeta;
	}
	
	//	get anchors
	if ( Params.mOutputAnchors )
	{
		json11::Json::array Anchors;
		auto EnumAnchor = [&](ARAnchor* Anchor)
		{
			json11::Json::object AnchorObject;
			if ( GetAnchorMeta( Anchor, AnchorObject, Params.mOutputAnchorGeometry ) )
				Anchors.push_back(AnchorObject);
		};
		Platform::NSArray_ForEach<ARAnchor*>(Frame.anchors,EnumAnchor);
		if ( Anchors.size() )
			Meta["Anchors"] = Anchors;
	}
	
	if ( Frame.lightEstimate )
	{
		Meta["LightIntensity"] = Frame.lightEstimate.ambientIntensity;
		Meta["LightTemperature"] = Frame.lightEstimate.ambientColorTemperature;
	}
	
	if ( Frame.rawFeaturePoints )
	{
		//	write all positions and their 64 bit id's (as string)
		auto* PointCloud = Frame.rawFeaturePoints;
		Meta["FeatureCount"] = static_cast<int>(PointCloud.count);
		
		if ( Params.mOutputFeatures )
		{	
			json11::Json::array PositionArray;
			json11::Json::array UidArray;
			for ( auto i=0;	i<PointCloud.count;	i++ )
			{
				auto& Uid64 = PointCloud.identifiers[i];
				auto& Pos3 = PointCloud.points[i];
				auto UidString = std::to_string(Uid64);
				PositionArray.push_back(Pos3[0]);
				PositionArray.push_back(Pos3[1]);
				PositionArray.push_back(Pos3[2]);
				UidArray.push_back(UidString);
			}
			json11::Json::object Features =
			{
				{"Positions",PositionArray},
				{"Uids",UidArray}
			};
			Meta["Features"] = Features;
		}
	}
	

#if ENABLE_IOS14
	if ( Frame.detectedBody && Frame.detectedBody.skeleton )
	{
		auto Skeleton = GetSkeleton( Frame.detectedBody.skeleton );
		Meta["Skeleton"] = Skeleton;
	}
#endif
	
#if ENABLE_IOS14
	if ( Frame.geoTrackingStatus )
	{
		auto State = Avf::GeoTrackingState::ToString(static_cast<Avf::GeoTrackingState::Type>(Frame.geoTrackingStatus.state));
		auto Accuracy = Avf::GeoTrackingAccuracy::ToString(static_cast<Avf::GeoTrackingAccuracy::Type>(Frame.geoTrackingStatus.accuracy));
		auto StateReason = Avf::GeoTrackingStateReason::ToString(static_cast<Avf::GeoTrackingStateReason::Type>(Frame.geoTrackingStatus.stateReason));
		
		Meta["GeoTrackingState"] = State;
		Meta["GeoTrackingStateReason"] = StateReason;
		Meta["GeoTrackingAccuracy"] = Accuracy;
	}
#endif
	
	auto WorldMappingStatus = magic_enum::enum_name(Frame.worldMappingStatus);
	Meta["WorldMappingStatus"] = std::string(WorldMappingStatus);
}


void Avf::GetMeta(AVDepthData* Depth,json11::Json::object& Meta)
{
	auto Quality = magic_enum::enum_name(Depth.depthDataQuality);
	auto Accuracy = magic_enum::enum_name(Depth.depthDataAccuracy);
	auto IsFiltered = Depth.depthDataFiltered;
	AVCameraCalibrationData* CameraCalibration = Depth.cameraCalibrationData;
	
	Meta["Quality"] = Quality;
	Meta["Accuracy"] = Accuracy;
	Meta["IsFiltered"] = IsFiltered;
	//	gr: rename to Projection & CameraToWorld
	auto IntrinsicMatrix = GetJsonArray(CameraCalibration.intrinsicMatrix);
	auto IntrinsicPixelToMm = CameraCalibration.pixelSize;
	auto IntrinsicSize = GetJsonArray(CameraCalibration.intrinsicMatrixReferenceDimensions);
	auto ExtrinsicMatrix = GetJsonArrayAs4x4(CameraCalibration.extrinsicMatrix);
	
	json11::Json CameraJson = json11::Json::object
	{
		{ "IntrinsicMatrix",IntrinsicMatrix },
		{ "IntrinsicPixelToMm",IntrinsicPixelToMm },
		{ "IntrinsicSize",IntrinsicSize },
		{ "ExtrinsicMatrix",ExtrinsicMatrix }
	};
	Meta["CameraCalibration"] = CameraJson;
}


void Avf::GetMeta(ARDepthData* Depth,json11::Json::object& Meta)
{
	//	no meta here!
}


//	we don't have stream names for the colour frames
const char* Arkit::GetColourStreamName(ArFrameSource::Type Source)
{
	switch(Source)
	{
		case ArFrameSource::FrontColour:
		case ArFrameSource::FrontDepth:
			return "FrontColour";
	
		case ArFrameSource::sceneDepth:	
		case ArFrameSource::RearDepth:
		case ArFrameSource::RearDepthConfidence:
			return "RearColour";
		
		//	gr: we should make sure of others are correct, but output something instead of null as we did before 
		default:
			return "Colour";
	};
}

Arkit::TFrameDevice::TFrameDevice(json11::Json& Options) : 
	TDevice	( Options ),
	mParams	( Options )
{
	//	todo; get capabilities and reject params here
}

void GetGeometry(ARPlaneAnchor* Anchor,Arkit::TAnchorGeometry& Geometry)
{
	Geometry.mBoundsCenter = vec3f( Anchor.center.x, Anchor.center.y, Anchor.center.z );
	Geometry.mBoundsSize = vec3f( Anchor.extent.x, Anchor.extent.y, Anchor.extent.z );
	
	if ( Anchor.geometry )
	{
		auto TriangleCount = Anchor.geometry.triangleCount;
		//	allocate the data to write to
		auto ComponentCount = 4;
		auto FloatsFixedArray = Geometry.AllocatePositions(TriangleCount*3,ComponentCount);
		//	convert to a growable array
		size_t FloatCount = 0;
		auto FloatsArray = GetRemoteArray( FloatsFixedArray.GetArray(), FloatsFixedArray.GetSize(), FloatCount );
		GetAnchorTriangles( Anchor.geometry, GetArrayBridge(FloatsArray), ComponentCount );
	}
}

template<typename INDEXTYPE>
void GetTriangles(ArrayBridge<float>& TrianglePositionFloats,int PositionComponentCount,ARGeometrySource* PositionsSource,const INDEXTYPE* Indexes,int TriangleCount)
{
	uint8_t* PositionsBytes = reinterpret_cast<uint8_t*>(PositionsSource.buffer.contents);
	//	apply offsets
	//	check size
	auto PositionFormat = PositionsSource.format;
	auto Offset = PositionsSource.offset;
	auto ByteStride = PositionsSource.stride;
	
	PositionsBytes += Offset;
	float* Positions = reinterpret_cast<float*>(PositionsBytes);
	std::Debug << "GetTriangles() PositionFormat=" << PositionFormat << " Offset=" << Offset << " ByteStride=" << ByteStride << std::endl;  

	for ( int t=0;	t<TriangleCount;	t++ )
	{
		auto i = t*3;
		auto vi0 = Indexes[i+0];
		auto vi1 = Indexes[i+1];
		auto vi2 = Indexes[i+2];
		auto* xyz0 = &Positions[ vi0*3 ];
		auto* xyz1 = &Positions[ vi1*3 ];
		auto* xyz2 = &Positions[ vi2*3 ];
		auto w = 1.0f;
		TrianglePositionFloats.PushBack( xyz0[0] );
		TrianglePositionFloats.PushBack( xyz0[1] );
		TrianglePositionFloats.PushBack( xyz0[2] );
		if ( PositionComponentCount == 4 )
			TrianglePositionFloats.PushBack( w );
			
		TrianglePositionFloats.PushBack( xyz1[0] );
		TrianglePositionFloats.PushBack( xyz1[1] );
		TrianglePositionFloats.PushBack( xyz1[2] );
		if ( PositionComponentCount == 4 )
			TrianglePositionFloats.PushBack( w );
			
		TrianglePositionFloats.PushBack( xyz2[0] );
		TrianglePositionFloats.PushBack( xyz2[1] );
		TrianglePositionFloats.PushBack( xyz2[2] );
		if ( PositionComponentCount == 4 )
			TrianglePositionFloats.PushBack( w );
	}
}

void GetTriangles(ArrayBridge<float>&& TrianglePositionFloats,int PositionComponentCount,ARGeometrySource* Positions,ARGeometryElement* Faces)
{
	void* Indexes = Faces.buffer.contents;
	auto TriangleCount = Faces.count;
	auto IndexesPerTriangle = Faces.indexCountPerPrimitive;
	if ( IndexesPerTriangle != 3 )
	{
		std::stringstream Error;
		Error << "Anchor mesh geometry has IndexesPerTriangle=" << IndexesPerTriangle << ", expected 3";
		throw Soy::AssertException(Error);
	}
	 
	if ( Faces.bytesPerIndex == sizeof(uint32_t) )
	{
		auto* Indexes32 = reinterpret_cast<uint32_t*>(Indexes);
		GetTriangles( TrianglePositionFloats, PositionComponentCount, Positions, Indexes32, TriangleCount );
	}
	else if ( Faces.bytesPerIndex == sizeof(uint16_t) )
	{
		auto* Indexes16 = reinterpret_cast<uint16_t*>(Indexes);
		GetTriangles( TrianglePositionFloats, PositionComponentCount, Positions, Indexes16, TriangleCount );
	}
	else
	{
		throw Soy::AssertException("Faces.bytesPerIndex unhandled");
	}
}

size_t GetTriangleCount(ARGeometryElement* Faces)
{
	//	ignore lines
	//	todo: detect something other than lines
	if ( Faces.primitiveType == ARGeometryPrimitiveTypeLine )
		return 0;
		
	if ( Faces.primitiveType != ARGeometryPrimitiveTypeTriangle )
	{
		std::Debug << "Anchor has faces which aren't lines or triangles: " << magic_enum::enum_name(Faces.primitiveType) << std::endl;
		return 0;
	}

	return Faces.count;
}


void GetGeometry(ARMeshAnchor* Anchor,Arkit::TAnchorGeometry& Geometry)
{
	//	mesh anchor doesnt have bounds or transform
	//Geometry.mBoundsCenter = vec3f( Anchor.center.x, Anchor.center.y, Anchor.center.z );
	//Geometry.mBoundsSize = vec3f( Anchor.extent.x, Anchor.extent.y, Anchor.extent.z );
	
	if ( Anchor.geometry )
	{
		auto* Faces = Anchor.geometry.faces;
		auto TriangleCount = GetTriangleCount(Faces);
		if ( TriangleCount > 0 )
		{
			//	allocate the data to write to
			auto ComponentCount = 4;
			auto FloatsFixedArray = Geometry.AllocatePositions(TriangleCount*3,ComponentCount);
			//	convert to a growable array
			size_t FloatCount = 0;
			auto FloatsArray = GetRemoteArray( FloatsFixedArray.GetArray(), FloatsFixedArray.GetSize(), FloatCount );
			GetTriangles( GetArrayBridge(FloatsArray), ComponentCount, Anchor.geometry.vertices, Anchor.geometry.faces );
		}
	}
}


void Arkit::TFrameDevice::PushFrame(ARFrame* Frame,ArFrameSource::Type Source)
{
	auto* ColourStreamName = GetColourStreamName(Source);
	auto FrameTime = SoyTime( Soy::Platform::GetTime( Frame.timestamp ) );
	auto CapDepthTime = SoyTime( Soy::Platform::GetTime( Frame.capturedDepthDataTimestamp ) );
	
	//std::Debug << "Arkit frame: Time=" << FrameTime << " Depth=" << CapDepthTime << " Delta=" << (FrameTime-mPreviousFrameTime) << " capdepth=" << (Frame.capturedDepthData?"true":"false") << std::endl;
	
	auto DepthIsValid = CapDepthTime.GetTime() != 0;
	if ( CapDepthTime <= mPreviousDepthTime && DepthIsValid )
	{
		std::Debug << "PushFrame(ARFrame*) ignoring depth frame prev=" << mPreviousDepthTime << " this=" << CapDepthTime << std::endl;
		DepthIsValid = false;
	}
	bool ColourIsNew = true;
	if ( FrameTime <= mPreviousFrameTime )
	{
		std::Debug << "PushFrame(ARFrame*) ignoring [colour] frame prev=" << mPreviousFrameTime << " this=" << FrameTime << std::endl;
		ColourIsNew = false;
	}
	
	if ( mParams.mVerboseDebug )
		std::Debug << __PRETTY_FUNCTION__ << " timestamp=" << FrameTime << " capturedDepthDataTimestamp=" << CapDepthTime << std::endl;

	//	get meta out of the ARFrame
	json11::Json::object Meta;
	Avf::GetMeta( Frame, Meta, mParams );

	//	gr: now we can handle multiple streams, just output anything we have
	if ( Frame.capturedImage )
		if ( ColourIsNew )
			PushFrame( Frame.capturedImage, FrameTime, Meta, ColourStreamName );

	//	captured depth is the truedepth camera
	if ( Frame.capturedDepthData )
		if ( DepthIsValid )
			PushFrame( Frame.capturedDepthData, CapDepthTime, Meta, "CapturedDepthData" );
			
#if ENABLE_IOS13
	if ( mParams.mEnablePersonSegmentation )	//	this may not be enabled by user, but still output
		if ( Frame.segmentationBuffer )
			PushFrame( Frame.segmentationBuffer, CapDepthTime, Meta, "SegmentationBuffer" );
#endif

#if ENABLE_IOS13
	if ( Frame.estimatedDepthData )
		PushFrame( Frame.estimatedDepthData, CapDepthTime, Meta, "EstimatedDepthData" );
#endif

#if ENABLE_IOS14
	auto SmoothDepth = Frame.smoothedSceneDepth ? Frame.smoothedSceneDepth : Frame.sceneDepth;
	auto NormalDepth = Frame.sceneDepth ? Frame.sceneDepth : Frame.smoothedSceneDepth;
	auto SceneDepth = mParams.mOutputSceneDepthSmooth ? SmoothDepth : NormalDepth;
	if ( SceneDepth )
	{
		auto IsSmoothed = SceneDepth == Frame.smoothedSceneDepth;
		Avf::GetMeta( SceneDepth, Meta );
		if ( SceneDepth.depthMap )
			PushFrame( SceneDepth.depthMap, FrameTime, Meta, IsSmoothed ? "SceneDepthSmoothed" : "SceneDepthMap" );

		if ( SceneDepth.confidenceMap )
			if ( mParams.mOutputSceneDepthConfidence )
				PushFrame( SceneDepth.confidenceMap, FrameTime, Meta, "SceneDepthConfidence" );
	}
#endif

	mPreviousFrameTime = FrameTime;
	mPreviousDepthTime = CapDepthTime;
}

void Arkit::TFrameDevice::PushFrame(AVDepthData* DepthData,SoyTime Timestamp,json11::Json::object& Meta,const char* StreamName)
{
	if ( !DepthData )
		return;
	Soy::TScopeTimerPrint Timer("PushFrame(AVDepthData",5);
	auto DepthPixels = Avf::GetDepthPixelBuffer(DepthData);
	
	Avf::GetMeta( DepthData, Meta );
	PushFrame( DepthPixels, Timestamp, Meta, StreamName );
}



void Arkit::TFrameDevice::PushFrame(CVPixelBufferRef PixelBuffer,SoyTime Timestamp,json11::Json::object& Meta,const char* StreamName)
{
	if ( !PixelBuffer )
		return;
	Soy::TScopeTimerPrint Timer("PushFrame(CVPixelBufferRef",5);
	float3x3 Transform;
	auto DoRetain = true;
	std::shared_ptr<AvfDecoderRenderer> Renderer;
	std::shared_ptr<TPixelBuffer> Buffer( new CVPixelBuffer( PixelBuffer, DoRetain, Renderer, Transform ) );
	
	if ( StreamName )
	{
		json11::Json::object MetaCopy = Meta;
		MetaCopy["StreamName"] = StreamName;
		PopCameraDevice::TDevice::PushFrame( Buffer, Timestamp, MetaCopy );
	}
	else
	{
		PopCameraDevice::TDevice::PushFrame( Buffer, Timestamp, Meta );
	}
}


void Arkit::TFrameDevice::PushGeometryFrame(const TAnchorGeometry& Geometry)
{
	Soy::TScopeTimerPrint Timer(__PRETTY_FUNCTION__,5);

	float3x3 Transform;
	auto StreamName = GeometryStreamName;
	std::shared_ptr<SoyPixelsImpl> Pixels = std::dynamic_pointer_cast<SoyPixelsImpl>( Geometry.mTrianglePositionsPixels );
	std::shared_ptr<TPixelBuffer> Buffer( new TDumbSharedPixelBuffer( Pixels, Transform ) );

	json11::Json::object Meta;
	Meta["StreamName"] = StreamName;
	Meta["AnchorUuid"] = Geometry.mUuid;
	Meta["AnchorName"] = Geometry.mName;
	Meta["AnchorType"] = Geometry.mType;
	Meta["PositionCount"] = static_cast<int>(Geometry.mPositionCount);
	Meta["LocalToWorld"] = GetJsonArray(Geometry.mLocalToWorld);
	SoyTime Timestamp(false);
	
	PopCameraDevice::TDevice::PushFrame( Buffer, Timestamp, Meta );
}





Arkit::TSessionCamera::TSessionCamera(const std::string& DeviceName,json11::Json& Options) :
	TFrameDevice	( Options )
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
	@try
	{
		mSession.reset( new TSession(RearCameraSession,mParams) );
	}
	@catch (NSException* e)
	{
		std::stringstream Debug;
		Debug << "NSException " << Soy::NSErrorToString( e );
		throw Soy::AssertException(Debug);
	}

	mSession->mOnFrame = [this](ARFrame* Frame)	
	{	
		this->OnFrame(Frame);	
	};

	mSession->mOnAnchorChange = [this](ARAnchor* Anchor,AnchorChange::Type Change)	
	{	
		this->OnAnchorChange( Anchor, Change );	
	};
}

void Arkit::TSessionCamera::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	Soy_AssertTodo();
}

void Arkit::TSessionCamera::OnFrame(ARFrame* Frame)
{
	PushFrame( Frame, mSource );
}

void Arkit::TSessionCamera::OnAnchorChange(ARAnchor* Anchor,AnchorChange::Type Change)
{
	Arkit::TAnchorGeometry Geometry;
	
	if ( [Anchor isKindOfClass:[ARPlaneAnchor class]] )
	{
		if ( !mParams.mEnableAnchorGeometry )
			return;
		auto* PlaneAnchor = (ARPlaneAnchor*)Anchor;
		GetGeometry( PlaneAnchor, Geometry );
	}
	else if ( [Anchor isKindOfClass:[ARMeshAnchor class]] )
	{
		if ( !mParams.mEnableWorldGeometry )
			return;
		auto* MeshAnchor = (ARMeshAnchor*)Anchor;
		GetGeometry( MeshAnchor, Geometry );
	}
	else
	{
		//	non geometry anchor
		if ( mParams.mVerboseDebug )
			std::Debug << "Skipping non-geometry anchor type " << GetClassName(Anchor) << std::endl;
		return;
	}
	
	Geometry.mUuid = Soy::NSStringToString(Anchor.identifier.UUIDString);
	Geometry.mName = Soy::NSStringToString(Anchor.name);
	Geometry.mType = GetClassName(Anchor);
	Geometry.mTimestamp = SoyTime(true);
	Geometry.mLocalToWorld = SimdToFloat4x4(Anchor.transform);
	
	PushGeometryFrame(Geometry);
}


SoyPixelsFormat::Type GetFloatPixelFormat(size_t ComponentCount)
{
	switch(ComponentCount)
	{
		case 1:	return SoyPixelsFormat::Float1;
		case 2:	return SoyPixelsFormat::Float2;
		case 3:	return SoyPixelsFormat::Float3;
		case 4:	return SoyPixelsFormat::Float4;
		default:break;
	}
	std::stringstream Error;
	Error << "GetFloatPixelFormat() unhandled float component count " << ComponentCount;
	throw Soy::AssertException(Error);
}

FixedRemoteArray<float> Arkit::TAnchorGeometry::AllocatePositions(size_t PositionCount,size_t PositionComponentCount)
{
	//	gr: make a square image? maybe dont need to... it's just data
	auto Width = PositionCount;
	auto Height = 1;
	auto PixelFormat = GetFloatPixelFormat(PositionComponentCount);
	SoyPixelsMeta PixelsMeta( Width, Height, PixelFormat );
	
	mTrianglePositionsPixels.reset( new SoyPixels(PixelsMeta) );
	
	auto& PixelsArray8 = mTrianglePositionsPixels->GetPixelsArray();
	auto PixelsArrayFloat = GetArrayBridge(PixelsArray8).GetSubArray<float>( 0, PositionCount*Height*PositionComponentCount );
	return PixelsArrayFloat;
}

